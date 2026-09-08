#pragma once
#include "displaced_emitter_light.h"
#include "displaced_geometry.h"
#include "ks/light.h"
#include <memory>
#include <vector>

// The displaced surface as a ks area light ("Plan — Path tracer with
// displaced surfaces", T8): one AreaLightShared per instanced displaced
// geometry whose material emits, one AreaLight per base triangle that
// holds emission mass, each wrapping the tessellation-free sampler of the
// object's selected kind (S8's DisplacedEmitterLight). Built by
// DisplacedGeometry::create_area_light, addressed by the base triangle as
// the primitive, which is what the walk reports as the hit's primID.
//
// Everything of the sampler happens in object space, where the surface
// S(s, t) lives; the instance transform moves points and normals to the
// world, and an area density converts by the transform's area scale at
// the point's normal, |det A| |A^-T n|. The sample and the query side
// derive the point, the normal and the distance from the smooth surface at
// (s, t) with the same arithmetic, so the two sides of an MIS weight agree
// exactly on sampled points (S8's contract).
//
// Draws (D7): the descent from the render sampler's rng, the point inside
// the leaf from the sampler's 2D Sobol draw, the one draw every ks light
// consumes, so the Sobol dimensions of every other light are unchanged.
//
// Emission is the material's field in the object's texture coordinates,
// evaluated at the sample's or the hit's uv, in colour; the scalar tile
// the samplers steer by is its luminance (scene_spec.h). Two-sided, as
// ks's mesh lights are.

namespace dmap
{

struct DisplacedAreaLightShared;

struct DisplacedAreaLight : ks::AreaLight
{
    const DisplacedAreaLightShared *shared = nullptr;
    uint32_t prim = 0; // the base triangle
    std::unique_ptr<DisplacedEmitterLight> sampler;
    float importance = 0.0f;

    bool delta_position() const override { return false; }
    bool delta_direction() const override { return false; }

    using ks::Light::sample;
    ks::color3 eval(const ks::Intersection &hit) const override;
    ks::color3 sample(const ks::Intersection &shade, ks::PTRenderSampler &render_sampler, ks::vec3 &wi, float &wi_dist,
                      float &pdf) const override;
    ks::color3 sample(const ks::vec3 &p_shade, const ks::vec2 &u, ks::vec3 &wi, float &wi_dist,
                      float &pdf) const override;
    // The direction's density from the hit it produces on this triangle
    // (the walk finds the hit; the tracer calls pdf_hit instead).
    float pdf(const ks::vec3 &p_shade, const ks::vec3 &wi, float wi_dist) const override;
    float pdf_hit(const ks::Intersection &shade, const ks::vec3 &wi, const ks::Intersection &hit) const override;
    ks::color3 power(const ks::AABB3 &scene_bound) const override;

  private:
    // One sample toward the shading point: the descent from rng, the leaf
    // point from u_leaf, the shading normal in object space for the
    // receiver-aware kind. Returns Le / pdf as ks's lights do.
    ks::color3 sample_light(const ks::vec3 &p_shade, const ks::vec3d &n_object, ks::RNG &rng, const ks::vec2d &u_leaf,
                            ks::vec3 &wi, float &wi_dist, float &pdf) const;
    // The solid-angle density at the world shading point of the surface
    // point at tile coordinates (s, t), from the sampler's area density.
    float pdf_omega_at(const ks::vec3 &p_shade, const ks::vec3 &n_shade, double s, double t) const;
};

struct DisplacedAreaLightShared : ks::AreaLightShared
{
    const DisplacedGeometry *geometry = nullptr;
    const DisplacedObject *object = nullptr;
    ks::Transform transform; // object to world
    const ks::ShaderField3 *emission = nullptr;
    std::vector<std::unique_ptr<DisplacedAreaLight>> lights; // one per emitting base triangle
    std::vector<uint32_t> prim_ids;

    DisplacedAreaLightShared(uint32_t inst_id, uint32_t geom_id, const DisplacedGeometry &geometry,
                             const ks::Transform &transform, const ks::ShaderField3 &emission, double beta);

    uint32_t light_count() const override { return (uint32_t)lights.size(); }
    const ks::AreaLight &light(uint32_t index) const override { return *lights[index]; }
    uint32_t prim_id(uint32_t index) const override { return prim_ids[index]; }

    // World area per object area at a unit object-space normal.
    double area_scale(const ks::vec3d &n_object) const;
    ks::vec3d to_object_point(const ks::vec3 &p) const;
    ks::vec3d to_object_normal(const ks::vec3 &n) const;
    ks::color3 emission_at(const ks::vec2d &uv_object) const;
};

} // namespace dmap
