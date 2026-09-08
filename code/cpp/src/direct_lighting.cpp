#include "direct_lighting.h"
#include "displaced_area_light.h"
#include "ks/assertion.h"
#include "ks/bsdf.h"
#include "ks/log_util.h"
#include "ks/material.h"
#include "ks/nee.h"
#include "ks/parallel.h"
#include <chrono>
#include <cmath>

using namespace ks;

namespace dmap
{

const char *strategy_name(Strategy s)
{
    switch (s) {
    case Strategy::NEE:
        return "nee";
    case Strategy::BSDF:
        return "bsdf";
    case Strategy::MIS:
        return "mis";
    case Strategy::Uniform:
        return "uniform";
    }
    return "?";
}

Strategy strategy_from_string(const std::string &name)
{
    if (name == "nee")
        return Strategy::NEE;
    if (name == "bsdf")
        return Strategy::BSDF;
    if (name == "mis")
        return Strategy::MIS;
    if (name == "uniform")
        return Strategy::Uniform;
    ASSERT(false, "strategy must be nee, bsdf, mis or uniform, got [%s]", name.c_str());
    return Strategy::MIS;
}

namespace
{

// ks's mesh lights shorten the shadow ray by this much (light.cpp).
constexpr float shadow_eps = 1e-4f;

// A delta light's contribution at a shading point: it can only be sampled,
// so the strategies without next-event estimation add every delta light
// this way.
color3 delta_light_direct(const Scene &scene, const Light &light, const BSDF &bsdf, const Intersection &it,
                          const vec3 &wo_local, PTRenderSampler &sampler)
{
    vec3 wi;
    float wi_dist, pdf_light;
    color3 Le_over_pdf = light.sample(it, sampler, wi, wi_dist, pdf_light);
    if (Le_over_pdf.isZero() || pdf_light <= 0.0f)
        return color3::Zero();
    color3 f = bsdf.eval(wo_local, it.sh_vector_to_local(wi), it);
    if (f.isZero())
        return color3::Zero();
    Ray shadow = spawn_ray<OffsetType::NextBounce>(it.p, wi, it.frame.n, 0.0f, wi_dist);
    if (scene.occlude1(shadow))
        return color3::Zero();
    return Le_over_pdf * f;
}

color3 delta_lights_direct(const PathTraceSetup &setup, const BSDF &bsdf, const Intersection &it, const vec3 &wo_local,
                           PTRenderSampler &sampler)
{
    const LightSampler &light_sampler = *setup.light_sampler;
    color3 L = color3::Zero();
    for (uint32_t i = 0; i < light_sampler.light_count(); ++i)
        if (light_sampler.get(i)->delta())
            L += delta_light_direct(*setup.input.scene, *light_sampler.get(i), bsdf, it, wo_local, sampler);
    return L;
}

// The next crossing along the line lies beyond this hit, past rounding.
float advance_past(float t) { return t + std::max(1e-6f, 1e-5f * t); }

// The integrand of direct lighting over the emitter's area at the hit y:
// Le f(x, wi) |n_y . wi| / r^2, times visibility.
color3 line_hit_contribution(const PathTraceSetup &setup, const SceneHit &y, const BSDF &bsdf, const Intersection &it,
                             const vec3 &wo_local)
{
    auto [light, pr] = setup.light_sampler->get_area_light(MeshTriIndex{y.inst_id, y.geom_id, y.prim_id});
    color3 Le = light->eval(y.it);
    if (Le.isZero())
        return color3::Zero();
    vec3 d = y.it.p - it.p;
    float dist2 = d.squaredNorm();
    if (dist2 == 0.0f)
        return color3::Zero();
    float dist = std::sqrt(dist2);
    vec3 wi = d / dist;
    color3 f = bsdf.eval(wo_local, it.sh_vector_to_local(wi), it);
    if (f.isZero())
        return color3::Zero();
    Ray shadow = spawn_ray<OffsetType::NextBounce>(it.p, wi, it.frame.n, 0.0f, std::max(0.0f, dist - shadow_eps));
    if (setup.input.scene->occlude1(shadow))
        return color3::Zero();
    float cos_y = std::abs(y.it.frame.n.dot(wi));
    return Le * f * (cos_y / dist2);
}

} // namespace

std::vector<AABB3> displaced_emitter_bounds(const PathTraceSetup &setup)
{
    std::vector<AABB3> boxes;
    for (const auto &al : setup.built->scene.area_lights) {
        const auto *dl = dynamic_cast<const DisplacedAreaLightShared *>(al.get());
        if (!dl)
            continue;
        AABB3 box;
        const Aabb3 &b = dl->geometry->intersector.bound;
        for (int corner = 0; corner < 8; ++corner) {
            vec3d p((corner & 1) ? b.hi[0] : b.lo[0], (corner & 2) ? b.hi[1] : b.lo[1],
                    (corner & 4) ? b.hi[2] : b.lo[2]);
            vec3d w = (dl->transform.m.cast<double>() * p.homogeneous()).head<3>();
            box.expand(w.cast<float>());
        }
        boxes.push_back(box);
    }
    return boxes;
}

AABB3 displaced_emitter_bound(const PathTraceSetup &setup)
{
    std::vector<AABB3> boxes = displaced_emitter_bounds(setup);
    ASSERT(!boxes.empty(), "the line estimator needs a displaced emitter");
    AABB3 box;
    for (const AABB3 &b : boxes)
        box.expand(b);
    return box;
}

bool line_emitter_hits(const PathTraceSetup &setup, const LineEstimator &estimator, RNG &rng,
                       std::vector<SceneHit> &hits)
{
    hits.clear();
    vec3 o, d;
    float t_far;
    if (!estimator.lines.sample(rng, o, d, t_far))
        return false;
    const Scene &scene = *setup.input.scene;
    float t_near = 0.0f;
    for (int k = 0; k < estimator.max_hits && t_near < t_far; ++k) {
        SceneHit hit;
        if (!scene.intersect1(Ray(o, d, t_near, t_far), hit))
            break;
        t_near = advance_past(hit.it.thit);
        if (hit.material->emission &&
            setup.light_sampler->get_area_light(MeshTriIndex{hit.inst_id, hit.geom_id, hit.prim_id}).first)
            hits.push_back(hit);
    }
    return true;
}

color3 direct_lighting(const PathTraceSetup &setup, const Ray &camera_ray, Strategy strategy, PTRenderSampler &sampler,
                       const LineEstimator *lines)
{
    const Scene &scene = *setup.input.scene;
    const LightSampler &light_sampler = *setup.light_sampler;
    SceneHit hit;
    if (!scene.intersect1(camera_ray, hit))
        return color3::Zero();
    const Intersection &it = hit.it;
    color3 L = color3::Zero();
    if (hit.material->emission) {
        auto [light, pr] = light_sampler.get_area_light(MeshTriIndex{hit.inst_id, hit.geom_id, hit.prim_id});
        if (light)
            L += light->eval(it);
    }
    const BSDF &bsdf = *hit.material->bsdf;
    if (bsdf.delta())
        return L;
    vec3 wo = -camera_ray.dir.normalized();
    vec3 wo_local = it.sh_vector_to_local(wo);

    if (strategy == Strategy::Uniform) {
        ASSERT(lines, "the uniform strategy needs a line estimator");
        static thread_local std::vector<SceneHit> hits;
        if (line_emitter_hits(setup, *lines, sampler.rng, hits)) {
            color3 sum = color3::Zero();
            for (const SceneHit &y : hits)
                sum += line_hit_contribution(setup, y, bsdf, it, wo_local);
            L += sum * (float)(2.0 * lines->lines.offset_area());
        }
        return L + delta_lights_direct(setup, bsdf, it, wo_local, sampler);
    }

    if (strategy != Strategy::BSDF) {
        float pr_light;
        auto [light_index, light] = light_sampler.sample(sampler.sobol.next(), pr_light);
        vec3 wi;
        float wi_dist, pdf_light;
        color3 Le_over_pdf = light->sample(it, sampler, wi, wi_dist, pdf_light);
        if (!Le_over_pdf.isZero() && pdf_light > 0.0f) {
            vec3 wi_local = it.sh_vector_to_local(wi);
            auto [f, pdf_bsdf] = bsdf.eval_and_pdf(wo_local, wi_local, it);
            if (!f.isZero()) {
                Ray shadow = spawn_ray<OffsetType::NextBounce>(it.p, wi, it.frame.n, 0.0f, wi_dist);
                if (!scene.occlude1(shadow)) {
                    float w = 1.0f;
                    if (strategy == Strategy::MIS && !light->delta())
                        w = power_heur(pdf_light * pr_light, pdf_bsdf);
                    L += Le_over_pdf * f * w / pr_light;
                }
            }
        }
    }

    if (strategy == Strategy::BSDF)
        L += delta_lights_direct(setup, bsdf, it, wo_local, sampler);

    if (strategy != Strategy::NEE) {
        vec3 wi_local;
        float pdf_bsdf;
        float u_lobe = sampler.sobol.next();
        vec2 u_wi = sampler.sobol.next2d();
        color3 beta = bsdf.sample(wo_local, wi_local, it, u_lobe, u_wi, pdf_bsdf);
        if (!beta.isZero() && pdf_bsdf > 0.0f) {
            vec3 wi = it.sh_vector_to_world(wi_local);
            Ray ray = spawn_ray<OffsetType::NextBounce>(it.p, wi, it.frame.n, 0.0f, inf);
            SceneHit next;
            if (scene.intersect1(ray, next) && next.material->emission) {
                auto [light, pr_light] =
                    light_sampler.get_area_light(MeshTriIndex{next.inst_id, next.geom_id, next.prim_id});
                if (light) {
                    float w = 1.0f;
                    if (strategy == Strategy::MIS)
                        w = power_heur(pdf_bsdf, light->pdf_hit(it, wi, next.it) * pr_light);
                    L += beta * light->eval(next.it) * w;
                }
            }
        }
    }
    return L;
}

RgbImage render_direct(const PathTraceSetup &setup, Strategy strategy, int spp, int seed, const fs::path &task_dir,
                       const std::string &prefix, const LineEstimator *lines, double *seconds)
{
    const SmallPTInput &in = setup.input;
    int width = in.render_width, height = in.render_height;
    RgbImage img;
    img.width = width;
    img.height = height;
    img.pixels.assign((size_t)width * height, color3::Zero());
    auto start = std::chrono::steady_clock::now();
    parallel_tile_2d(width, height, [&](int x, int y) {
        color3 sum = color3::Zero();
        for (int s = 0; s < spp; ++s) {
            PTRenderSampler sampler(arr2u(width, height), arr2u(x, y), spp, s, seed);
            vec2 offset = spp == 1 ? vec2::Zero() : in.pixel_filter->sample(sampler.sobol.next2d());
            vec2 film = (vec2(x + 0.5f, y + 0.5f) + offset).cwiseQuotient(vec2(width, height));
            Ray ray = in.camera->spawn_ray(film, vec2i(width, height), in.scale_ray_diff ? spp : 1);
            color3 L = direct_lighting(setup, ray, strategy, sampler, lines);
            thread_monitor_check(L.allFinite());
            sum += L;
        }
        img.pixels[(size_t)y * width + x] = sum / spp;
    });
    if (seconds)
        *seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    RenderTargetArgs rt_args;
    rt_args.width = width;
    rt_args.height = height;
    rt_args.backdrop = color3::Zero();
    RenderTarget2 rt(rt_args);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            RenderTargetPixel p;
            p.main = img.pixels[(size_t)y * width + x];
            rt.add(x, y, 1.0f, p);
        }
    rt.composite_and_save_to_exr(task_dir / prefix);
    if (setup.tone_mapper)
        rt.composite_and_save_to_png(task_dir / prefix, {}, setup.tone_mapper.get());
    return img;
}

} // namespace dmap
