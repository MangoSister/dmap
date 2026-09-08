#include "displaced_area_light.h"
#include "ks/assertion.h"
#include "ks/hash.h"
#include "ks/material.h"
#include "ks/sobol.h"
#include <cmath>

using namespace ks;

namespace dmap
{

namespace
{

// Below this |n_y . wi| the solid-angle density blows up and the
// contribution vanishes (displaced_emitter_light.cpp).
constexpr double min_cos_y = 1e-7;
// ks's mesh lights shorten the shadow ray by this much (light.cpp).
constexpr float shadow_eps = 1e-4f;
// The default probability floor of the descent (S7).
constexpr double default_beta = 0.05;

// The surface point and unit normal at tile coordinates (s, t).
void surface_at(const BaseTriangle &tri, const HeightGrid &field, double s, double t, vec3d &y, vec3d &n)
{
    PointwiseFields f = pointwise_fields(tri, field, s, t);
    y = tri.P(s, t) + f.h * normal_frame_at(tri, s, t).N;
    n = f.n.normalized();
}

} // namespace

DisplacedAreaLightShared::DisplacedAreaLightShared(uint32_t inst_id, uint32_t geom_id,
                                                   const DisplacedGeometry &geometry, const Transform &transform,
                                                   const ShaderField3 &emission, double beta)
    : AreaLightShared(inst_id, geom_id), geometry(&geometry), object(geometry.object), transform(transform),
      emission(&emission)
{
    ASSERT(object->emission, "the displaced object carries no emission statistics");
    for (uint32_t t = 0; t < (uint32_t)object->triangles.size(); ++t) {
        const BaseTriangle &tri = object->triangles[t];
        auto sampler = std::make_unique<DisplacedEmitterLight>(tri, object->field, *object->asset->pyramid,
                                                               *object->emission, geometry.emitter_sampler, beta);
        double mass = sampler->mass();
        if (mass <= 0.0)
            continue;
        auto light = std::make_unique<DisplacedAreaLight>();
        light->shared = this;
        light->prim = t;
        light->sampler = std::move(sampler);
        // Power as ks's mesh lights count it: 2 pi times the emitted
        // radiance integrated over the world-space area, with the metric
        // and the transform's area scale taken at the triangle's centre.
        vec2d c = (tri.t0 + tri.t1 + tri.t2) / 3.0;
        PointwiseFields f = pointwise_fields(tri, object->field, c[0], c[1]);
        light->importance = (float)(two_pi * mass * f.sqrt_det * area_scale(f.n.normalized()));
        prim_ids.push_back(t);
        lights.push_back(std::move(light));
    }
}

double DisplacedAreaLightShared::area_scale(const vec3d &n_object) const
{
    Eigen::Matrix3d A = transform.m.block<3, 3>(0, 0).cast<double>();
    Eigen::Matrix3d A_inv_T = transform.inv.block<3, 3>(0, 0).cast<double>().transpose();
    return std::abs(A.determinant()) * (A_inv_T * n_object).norm();
}

vec3d DisplacedAreaLightShared::to_object_point(const vec3 &p) const
{
    return (transform.inv.cast<double>() * p.cast<double>().homogeneous()).head<3>();
}

vec3d DisplacedAreaLightShared::to_object_normal(const vec3 &n) const
{
    return (transform.m.block<3, 3>(0, 0).cast<double>().transpose() * n.cast<double>()).normalized();
}

color3 DisplacedAreaLightShared::emission_at(const vec2d &uv_object) const
{
    return (*emission)(uv_object.cast<float>(), mat2::Zero());
}

namespace
{

vec3d world_point(const Transform &transform, const vec3d &p)
{
    return (transform.m.cast<double>() * p.homogeneous()).head<3>();
}

vec3d world_normal(const Transform &transform, const vec3d &n)
{
    return (transform.inv.block<3, 3>(0, 0).cast<double>().transpose() * n).normalized();
}

} // namespace

color3 DisplacedAreaLight::eval(const Intersection &hit) const { return (*shared->emission)(hit); }

color3 DisplacedAreaLight::sample_light(const vec3 &p_shade, const vec3d &n_object, RNG &rng, const vec2d &u_leaf,
                                        vec3 &wi, float &wi_dist, float &pdf) const
{
    wi_dist = 0.0f;
    pdf = 0.0f;
    vec3d p_object = shared->to_object_point(p_shade);
    EmitterLightSample s = sampler->sample(p_object, n_object, rng, &u_leaf);
    if (!s.ok)
        return color3::Zero();
    vec3d y = world_point(shared->transform, s.y);
    vec3d n_y = world_normal(shared->transform, s.n_y);
    vec3d d = y - p_shade.cast<double>();
    double dist2 = d.squaredNorm();
    if (dist2 == 0.0)
        return color3::Zero();
    double dist = std::sqrt(dist2);
    vec3d wi_d = d / dist;
    double cos_y = std::abs(n_y.dot(wi_d));
    if (cos_y < min_cos_y)
        return color3::Zero();
    double pdf_area = s.pdf_area / shared->area_scale(s.n_y);
    pdf = (float)(pdf_area * dist2 / cos_y);
    wi = wi_d.cast<float>();
    wi_dist = std::max(0.0f, (float)dist - shadow_eps);
    color3 Le = shared->emission_at(shared->object->object_uv(vec2d(s.u, s.v)));
    return Le / pdf;
}

color3 DisplacedAreaLight::sample(const Intersection &shade, PTRenderSampler &render_sampler, vec3 &wi, float &wi_dist,
                                  float &pdf) const
{
    vec2d u_leaf = render_sampler.sobol.next2d().cast<double>();
    return sample_light(shade.p, shared->to_object_normal(shade.frame.n), render_sampler.rng, u_leaf, wi, wi_dist, pdf);
}

color3 DisplacedAreaLight::sample(const vec3 &p_shade, const vec2 &u, vec3 &wi, float &wi_dist, float &pdf) const
{
    // Without a render sampler the descent draws from a generator seeded
    // by the 2D point, and the point inside the leaf is that 2D point. No
    // shading normal is known; the receiver-aware density reads +z.
    RNG rng(hash(u[0], u[1]));
    return sample_light(p_shade, vec3d::UnitZ(), rng, u.cast<double>(), wi, wi_dist, pdf);
}

float DisplacedAreaLight::pdf_omega_at(const vec3 &p_shade, const vec3 &n_shade, double s, double t) const
{
    vec3d p_object = shared->to_object_point(p_shade);
    vec3d n_object = shared->to_object_normal(n_shade);
    double pdf_area_object = sampler->pdf_area(p_object, n_object, s, t);
    if (pdf_area_object <= 0.0)
        return 0.0f;
    const BaseTriangle &tri = shared->object->triangles[prim];
    vec3d y_object, n_object_y;
    surface_at(tri, shared->object->field, s, t, y_object, n_object_y);
    vec3d y = world_point(shared->transform, y_object);
    vec3d n_y = world_normal(shared->transform, n_object_y);
    vec3d d = y - p_shade.cast<double>();
    double dist2 = d.squaredNorm();
    if (dist2 == 0.0)
        return 0.0f;
    double cos_y = std::abs(n_y.dot(d / std::sqrt(dist2)));
    if (cos_y < min_cos_y)
        return 0.0f;
    return (float)(pdf_area_object / shared->area_scale(n_object_y) * dist2 / cos_y);
}

float DisplacedAreaLight::pdf_hit(const Intersection &shade, const vec3 &wi, const Intersection &hit) const
{
    vec2d st = shared->object->tile_coords(hit.uv.cast<double>());
    return pdf_omega_at(shade.p, shade.frame.n, st[0], st[1]);
}

float DisplacedAreaLight::pdf(const vec3 &p_shade, const vec3 &wi, float wi_dist) const
{
    // The hit this direction produces on the triangle, by the walk. No
    // shading normal is known here; the receiver-aware density reads the
    // direction in its place.
    vec3d o = shared->to_object_point(p_shade);
    vec3d d = (shared->transform.inv.block<3, 3>(0, 0).cast<double>() * wi.cast<double>());
    DisplacedHit h;
    const DisplacedIntersector &tri = shared->geometry->intersector.triangles[prim];
    if (!tri.intersect(o, d, 0.0, INFINITY, shared->geometry->options, h))
        return 0.0f;
    return pdf_omega_at(p_shade, wi, h.st[0], h.st[1]);
}

color3 DisplacedAreaLight::power(const AABB3 &scene_bound) const { return color3::Constant(importance); }

std::unique_ptr<AreaLightShared> DisplacedGeometry::create_area_light(uint32_t inst_id, uint32_t geom_id,
                                                                      const Transform &transform,
                                                                      const Material &material) const
{
    if (!material.emission || !object->emission)
        return nullptr;
    auto shared = std::make_unique<DisplacedAreaLightShared>(inst_id, geom_id, *this, transform, *material.emission,
                                                             default_beta);
    if (shared->light_count() == 0)
        return nullptr;
    return shared;
}

} // namespace dmap
