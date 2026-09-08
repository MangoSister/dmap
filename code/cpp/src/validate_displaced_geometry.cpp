// Task validate_displaced_geometry ("Plan — Path tracer with displaced
// surfaces", T6): the displaced surfaces of a scene as embree user
// geometry inside ks's scene, on world-space rays from two distributions
// (camera rays that reach a displaced placement, rays from origins near a
// displaced surface in random directions).
// (1) Scene::intersect1 returns the hits of the standalone T4 intersector:
//     for every ray, the nearest displaced crossing over all placements
//     (each instance's inverse transform applied to the ray, the walk in
//     object space) must be the scene's hit when the scene hits a
//     displaced surface, and must lie no nearer than the scene's hit when
//     that is a mesh, within rounding of the float ray. A disagreement
//     counts only when the nearer hit is not grazing and not within
//     edge_margin of the domain boundary.
// (2) Placements with different transforms: (1) runs over every instance,
//     and the hits per placement are reported.
// (3) occlude1 agrees with intersect1: occluded to infinity iff hit, not
//     occluded up to half the hit distance.
// (4) An opacity map on the first displaced object (a checkerboard in its
//     texture coordinates) culls hits through the context filter: no
//     accepted hit lands on a transparent square, hits behind the culled
//     crossing are found, and the filtered hit is never nearer than the
//     unfiltered one.
// (5) Every field of the Intersection of a displaced hit is finite, the
//     frame is orthonormal and equals the shading frame, and the position
//     lies on the ray within rounding.
// Verdict: PASS only if all hold.
//
// Config: scene, n_rays (per distribution, 20000), seed, edge_margin (1e-3).

#include "displaced_scene.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include "ks/material.h"
#include "ks/opacity_map.h"
#include "ks/parallel.h"
#include "ks/rng.h"
#include "ks/shader_field.h"
#include "scene_spec.h"
#include "traversal_test_util.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace ks;

namespace
{

// One placement of one displaced object: the geometry and the instance.
struct Placement
{
    int displaced = -1;
    int shape = -1;
    uint32_t instance = 0;
    Transform to_world, to_object;
    const dmap::DisplacedGeometry *geometry = nullptr;
};

struct ReferenceHit
{
    bool hit = false;
    double t = INFINITY;
    int placement = -1;
    dmap::DisplacedHit h;
};

// The nearest displaced crossing over the placements, in world units (an
// affine instance transform keeps the ray parameter).
ReferenceHit reference_hit(const std::vector<Placement> &placements, const Ray &ray)
{
    ReferenceHit best;
    for (size_t p = 0; p < placements.size(); ++p) {
        Ray local = transform_ray(placements[p].to_object, ray);
        dmap::DisplacedHit h;
        int tri;
        if (placements[p].geometry->intersector.intersect(local.origin.cast<double>(), local.dir.cast<double>(), 0.0,
                                                          best.t, placements[p].geometry->options, h, tri) &&
            h.t < best.t) {
            best.hit = true;
            best.t = h.t;
            best.placement = (int)p;
            best.h = h;
        }
    }
    return best;
}

// The least barycentric weight of a point of the base triangle.
double domain_margin(const dmap::BaseTriangle &tri, const vec2d &st)
{
    vec2d b = tri.bary(st[0], st[1]);
    return std::min({b[0], b[1], 1.0 - b[0] - b[1]});
}

// A checkerboard opacity, 8 squares per unit of texture coordinates.
struct CheckerField : ShaderField1
{
    color<1> operator()(const vec2 &uv, const mat2 &) const override
    {
        int cu = (int)std::floor(uv[0] * 8.0f), cv = (int)std::floor(uv[1] * 8.0f);
        return color<1>::Constant(((cu + cv) & 1) ? 1.0f : 0.0f);
    }
};

bool orthonormal(const Frame &f, double tolerance)
{
    return std::abs(f.t.norm() - 1.0) < tolerance && std::abs(f.b.norm() - 1.0) < tolerance &&
           std::abs(f.n.norm() - 1.0) < tolerance && std::abs(f.t.dot(f.b)) < tolerance &&
           std::abs(f.t.dot(f.n)) < tolerance && std::abs(f.b.dot(f.n)) < tolerance;
}

} // namespace

void validate_displaced_geometry(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    std::string scene_name = args.load_string("scene");
    const dmap::SceneSpec *spec = args.asset_table().get<dmap::SceneSpec>(scene_name);
    ASSERT(spec, "no scene named [%s]", scene_name.c_str());
    spec->require_valid();
    int n_rays = args.load_integer("n_rays", 20000);
    int seed = args.load_integer("seed", 2027);
    double edge_margin = (double)args.load_float("edge_margin", 1e-3f);
    constexpr double float_rounding = 1e-5, grazing = 1e-2;

    auto &log = get_default_logger();
    EmbreeDevice device;
    std::unique_ptr<dmap::DisplacedScene> built = dmap::create_displaced_scene(*spec, device, 0);
    Scene &scene = built->scene;
    AABB3 bound = scene.bound();
    double scale = (bound.max - bound.min).norm();
    ASSERT(!spec->displaced.empty(), "the scene has no displaced object");

    std::vector<Placement> placements;
    for (size_t di = 0; di < spec->displaced.size(); ++di) {
        const dmap::ShapePlacement &sp = built->shapes[spec->displaced[di].shape];
        for (uint32_t inst : sp.instances) {
            Placement p;
            p.displaced = (int)di;
            p.shape = spec->displaced[di].shape;
            p.instance = inst;
            p.to_world = scene.get_instance_transform(inst);
            p.to_object = p.to_world.inverse();
            p.geometry = built->displaced[di];
            placements.push_back(p);
        }
    }
    log.info("validate_displaced_geometry on {}: {} shapes, {} displaced objects in {} placements, {} subscenes, "
             "{} instances",
             scene_name, spec->shapes.size(), spec->displaced.size(), placements.size(), scene.subscenes.size(),
             scene.instances.size());

    // Rays in world space, float as the scene sees them.
    RNG rng((uint64_t)seed);
    std::vector<Ray> rays;
    int n_camera = 0;
    if (spec->camera) {
        for (int64_t tries = 0; tries < 200 * (int64_t)n_rays && n_camera < n_rays; ++tries) {
            Ray world = spec->camera->spawn_ray(vec2(rng.next(), rng.next()), vec2i(640, 480), 1);
            bool reaches = false;
            for (const Placement &p : placements) {
                Ray local = transform_ray(p.to_object, world);
                double t0 = 0.0, t1 = INFINITY;
                reaches = reaches || p.geometry->intersector.bound.clip(local.origin.cast<double>(),
                                                                        local.dir.cast<double>(), t0, t1);
            }
            if (!reaches)
                continue;
            rays.push_back(world);
            ++n_camera;
        }
    }
    for (int k = 0; k < n_rays; ++k) {
        const Placement &p = placements[(size_t)(rng.next() * placements.size()) % placements.size()];
        const dmap::DisplacedObject &d = spec->displaced[p.displaced];
        int t = (int)(rng.next() * d.triangles.size()) % (int)d.triangles.size();
        vec2d b = dmap::uniform_bary(rng);
        vec2d st = d.triangles[t].param(b[0], b[1]);
        vec3d S = dmap::displaced_position(d.triangles[t], d.field, st[0], st[1]);
        double r = dmap::mean_edge(d.triangles[t]) * std::pow(10.0, -1.0 + 1.5 * rng.next());
        vec3d o = S + r * dmap::uniform_direction(rng);
        Ray local(o.cast<float>(), dmap::uniform_direction(rng).cast<float>(), 0.0f, INFINITY);
        rays.push_back(transform_ray(p.to_world, local));
    }
    int n_total = (int)rays.size();
    log.info("  rays: {} camera, {} near the surfaces", n_camera, n_total - n_camera);
    bool pass = true;

    // (1), (2): the scene against the standalone intersector.
    std::vector<SceneHit> hits(n_total);
    std::vector<char> hit_flag(n_total, 0);
    std::vector<int> hit_displaced(n_total, -1);
    std::vector<ReferenceHit> reference(n_total);
    parallel_for(n_total, [&](int r) {
        hit_flag[r] = scene.intersect1(rays[r], hits[r]) ? 1 : 0;
        if (hit_flag[r])
            hit_displaced[r] = built->displaced_of(hits[r].subscene_id, hits[r].geom_id);
        reference[r] = reference_hit(placements, rays[r]);
    });
    std::vector<int64_t> hits_per_placement(placements.size(), 0);
    int64_t agree = 0, mesh_in_front = 0, explained = 0, unexplained = 0;
    double worst_dt = 0.0;
    for (int r = 0; r < n_total; ++r) {
        const SceneHit &a = hits[r];
        const ReferenceHit &b = reference[r];
        bool a_displaced = hit_flag[r] && hit_displaced[r] >= 0;
        if (a_displaced) {
            for (size_t p = 0; p < placements.size(); ++p)
                if (placements[p].instance == a.inst_id && placements[p].displaced == hit_displaced[r])
                    ++hits_per_placement[p];
        }
        bool disagree;
        double cos_hit = 1.0, margin = 1.0;
        if (a_displaced) {
            double cos_a = std::abs(rays[r].dir.normalized().dot(a.it.frame.n));
            disagree = !b.hit;
            if (b.hit) {
                double dt = std::abs((double)a.it.thit - b.t) * std::max(cos_a, grazing) / (b.t + scale);
                worst_dt = std::max(worst_dt, dt);
                disagree = dt > float_rounding;
            }
            if (!disagree) {
                ++agree;
                continue;
            }
            // The nearer hit decides whether rounding can explain it.
            if (!b.hit || a.it.thit <= b.t) {
                const dmap::DisplacedObject &d = spec->displaced[hit_displaced[r]];
                vec2d st = d.tile_coords(a.it.uv.cast<double>());
                margin = domain_margin(d.triangles[a.prim_id], st);
                cos_hit = cos_a;
            } else {
                margin = std::min({b.h.bary[0], b.h.bary[1], 1.0 - b.h.bary[0] - b.h.bary[1]});
                cos_hit = std::abs(rays[r].dir.cast<double>().normalized().dot(b.h.ng));
            }
        } else if (hit_flag[r]) {
            // A mesh in front: the displaced crossing must not be nearer.
            disagree = b.hit && b.t < (double)a.it.thit - float_rounding * (b.t + scale);
            if (!disagree) {
                ++mesh_in_front;
                continue;
            }
            margin = std::min({b.h.bary[0], b.h.bary[1], 1.0 - b.h.bary[0] - b.h.bary[1]});
            cos_hit = std::abs(rays[r].dir.cast<double>().normalized().dot(b.h.ng));
        } else {
            disagree = b.hit;
            if (!disagree) {
                ++agree;
                continue;
            }
            margin = std::min({b.h.bary[0], b.h.bary[1], 1.0 - b.h.bary[0] - b.h.bary[1]});
            cos_hit = std::abs(rays[r].dir.cast<double>().normalized().dot(b.h.ng));
        }
        if (cos_hit < grazing || margin < edge_margin) {
            ++explained;
        } else {
            ++unexplained;
            if (unexplained <= 5)
                log.info("    disagreement: scene {} t={:.9g} (displaced {}), standalone {} t={:.9g}, margin {:.2e}, "
                         "cos {:.3f}",
                         hit_flag[r] ? "hit" : "miss", hit_flag[r] ? (double)a.it.thit : 0.0, a_displaced,
                         b.hit ? "hit" : "miss", b.hit ? b.t : 0.0, margin, cos_hit);
        }
    }
    log.info("(1) scene vs standalone intersector: {} agree ({} displaced hits and misses), {} rays with a mesh "
             "in front, {} rounding cases (edge margin < {:.0e} or grazing), {} unexplained; worst normalized "
             "|dt| {:.2e} (tolerance {:.0e})",
             agree + mesh_in_front, agree, mesh_in_front, explained, edge_margin, unexplained, worst_dt,
             float_rounding);
    pass = pass && unexplained == 0;
    {
        std::string per = "";
        for (size_t p = 0; p < placements.size(); ++p)
            per += ks::string_format(" [%s x%u] %lld", spec->shapes[placements[p].shape].name.c_str(),
                                     placements[p].instance, (long long)hits_per_placement[p]);
        log.info("(2) displaced hits per placement (shape x instance):{}", per);
        pass =
            pass && std::all_of(hits_per_placement.begin(), hits_per_placement.end(), [](int64_t n) { return n > 0; });
    }

    // (3) occlude1 against intersect1.
    {
        std::vector<char> bad(n_total, 0);
        parallel_for(n_total, [&](int r) {
            bool occluded = scene.occlude1(rays[r]);
            if (occluded != (hit_flag[r] != 0)) {
                bad[r] = 1;
                return;
            }
            if (hit_flag[r]) {
                Ray half = rays[r];
                half.tmax = 0.5f * hits[r].it.thit;
                if (scene.occlude1(half))
                    bad[r] = 2;
            }
        });
        int64_t n_bad_inf = std::count(bad.begin(), bad.end(), 1), n_bad_half = std::count(bad.begin(), bad.end(), 2);
        log.info("(3) occlude1 vs intersect1: {} rays, {} disagree to infinity, {} occluded before half the hit "
                 "distance",
                 n_total, n_bad_inf, n_bad_half);
        pass = pass && n_bad_inf == 0 && n_bad_half == 0;
    }

    // (4) The opacity map on the first displaced object.
    {
        const dmap::DisplacedObject &d0 = spec->displaced[0];
        const dmap::ShapePlacement &sp = built->shapes[d0.shape];
        SubScene &sub = *scene.subscenes[sp.subscene];
        const Material *original = sub.materials[sp.geometry];
        OpacityMap opacity;
        opacity.map = std::make_unique<CheckerField>();
        Material masked;
        masked.bsdf = original->bsdf;
        masked.opacity_map = &opacity;
        sub.materials[sp.geometry] = &masked;

        std::vector<int> outcome(n_total, 0); // 1 transparent accepted, 2 nearer than unfiltered
        std::vector<char> culled(n_total, 0), reached_behind(n_total, 0);
        parallel_for(n_total, [&](int r) {
            SceneHit h;
            bool hit = scene.intersect1(rays[r], h);
            bool on_d0 = hit && built->displaced_of(h.subscene_id, h.geom_id) == 0;
            if (on_d0 && opacity.eval(h.it.uv) < 1.0f)
                outcome[r] = 1;
            bool was_on_d0 = hit_flag[r] && hit_displaced[r] == 0;
            bool was_transparent = was_on_d0 && opacity.eval(hits[r].it.uv) < 1.0f;
            if (was_transparent) {
                culled[r] = 1;
                if (hit && h.it.thit > hits[r].it.thit)
                    reached_behind[r] = 1;
            }
            if (hit && hit_flag[r] && h.it.thit < hits[r].it.thit - float_rounding * (hits[r].it.thit + scale))
                outcome[r] = 2;
            if (hit && !hit_flag[r])
                outcome[r] = 2;
        });
        sub.materials[sp.geometry] = original;
        int64_t n_transparent = std::count(outcome.begin(), outcome.end(), 1);
        int64_t n_nearer = std::count(outcome.begin(), outcome.end(), 2);
        int64_t n_culled = std::count(culled.begin(), culled.end(), 1);
        int64_t n_behind = std::count(reached_behind.begin(), reached_behind.end(), 1);
        log.info("(4) opacity checkerboard on [{}]: {} hits culled, {} of them found a surface behind, {} accepted "
                 "hits on transparent squares, {} filtered hits nearer than the unfiltered",
                 spec->shapes[d0.shape].name, n_culled, n_behind, n_transparent, n_nearer);
        pass = pass && n_transparent == 0 && n_nearer == 0 && n_culled > 0;
    }

    // (5) The intersection record.
    {
        int64_t n_checked = 0, n_nonfinite = 0, n_frame = 0, n_shading = 0, n_off_ray = 0;
        double worst_off = 0.0;
        for (int r = 0; r < n_total; ++r) {
            if (!hit_flag[r] || hit_displaced[r] < 0)
                continue;
            const Intersection &it = hits[r].it;
            ++n_checked;
            bool finite = it.p.allFinite() && it.dpdu.allFinite() && it.dpdv.allFinite() && it.uv.allFinite() &&
                          it.frame.t.allFinite() && it.frame.b.allFinite() && it.frame.n.allFinite() &&
                          it.dpdx.allFinite() && it.dpdy.allFinite() && std::isfinite(it.dudx) &&
                          std::isfinite(it.dvdx) && std::isfinite(it.dudy) && std::isfinite(it.dvdy) &&
                          it.dpdu.norm() > 0.0f && it.dpdv.norm() > 0.0f;
            if (!finite)
                ++n_nonfinite;
            if (!orthonormal(it.frame, 1e-3))
                ++n_frame;
            if ((it.sh_frame.t - it.frame.t).norm() > 0.0f || (it.sh_frame.n - it.frame.n).norm() > 0.0f)
                ++n_shading;
            double off = (double)(it.p - rays[r](it.thit)).norm() / (it.thit + scale);
            worst_off = std::max(worst_off, off);
            if (off > 1e-4)
                ++n_off_ray;
        }
        log.info("(5) intersection records of {} displaced hits: {} with non-finite fields, {} with a frame not "
                 "orthonormal, {} with a shading frame differing from the geometric, {} off the ray (worst "
                 "normalized distance {:.2e})",
                 n_checked, n_nonfinite, n_frame, n_shading, n_off_ray, worst_off);
        pass = pass && n_nonfinite == 0 && n_frame == 0 && n_shading == 0 && n_off_ray == 0;
    }

    log.info("VERDICT: {} — displaced geometry on {}", pass ? "PASS" : "FAIL", scene_name);
}
