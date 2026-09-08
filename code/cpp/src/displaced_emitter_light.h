#pragma once
#include "descent_sampler.h"
#include "displaced_surface.h"
#include "ks/rng.h"
#include <memory>

// Phase S8 ("Plan — A-MVP sampling implementation"): the displaced emissive
// triangle as a light. Mirrors the ks::Light contract — sample() returns a
// direction, distance, and a solid-angle pdf at the shading point; the
// pdf-of-direction for MIS re-walks from a hit's (u, v), the mesh-light
// lookup pattern — but is built on the tessellation-free S6 samplers: the
// sampled point and pdf come from the smooth surface S(u, v), never from
// the visibility mesh. Standalone in dmap; adapting it into ks's
// LightSampler is the engine-integration step (path tracer plan, T8).
//
// Solid-angle conversion: pdf_omega = pdf_area * dist^2 / |n_y . wi|. Both
// the sample side and the query side derive (y, n_y, dist) from the smooth
// surface at (u, v), so the two sides of the MIS weight use identical
// arithmetic; a BSDF hit on the S2 mesh only contributes its (u, v).
//
// Emission is scalar (the texel grid, two-sided); the renderer applies any
// color tint. The pyramid and the emission tile are shared per asset.

namespace dmap
{

enum class EmitterSamplerKind
{
    EmissionTable,  // §5A baseline 2
    ProductTable,   // §5A baseline 5
    AreaDescent,    // descent, weights sqrt(det G) only
    ProductDescent, // descent, weights E x sqrt(det G)
    ReceiverDescent // descent, weights E x sqrt(det G) x cos_r+ / r^2
};

struct EmitterLightSample
{
    bool ok = false; // false: zero-contribution draw (outside domain, grazing)
    double u, v;
    ks::vec3d y, n_y; // smooth surface point and unit normal
    double Le;        // scalar emitted radiance (two-sided)
    ks::vec3d wi;     // unit direction from the shading point to y
    double dist;
    double pdf_area;  // area pdf on the smooth surface
    double pdf_omega; // solid-angle pdf at the shading point
};

struct DisplacedEmitterLight
{
    const BaseTriangle *tri = nullptr;
    HeightGrid field;
    const EmissionTile *emission = nullptr;
    EmitterSamplerKind kind;
    std::unique_ptr<DescentSampler> descent;
    std::unique_ptr<TexelTableSampler> table;

    DisplacedEmitterLight(const BaseTriangle &tri, const HeightGrid &field, const TaylorPyramid &pyramid,
                          const EmissionTile &emission, EmitterSamplerKind kind, double beta);

    double Le_at(double u, double v) const;

    // The emission mass the sampler sees (its weights sum); zero means
    // the triangle never draws a sample and is no light.
    double mass() const;
    // Bytes the triangle's sampler owns (descent_sampler.h).
    size_t memory_bytes() const;

    // The point inside the chosen leaf or texel comes from u_leaf when
    // given (descent_sampler.h).
    EmitterLightSample sample(const ks::vec3d &p_shade, const ks::vec3d &n_shade, ks::RNG &rng,
                              const ks::vec2d *u_leaf = nullptr) const;

    // Area pdf of this sampler at (u, v); receiver-dependent kinds re-walk
    // with the shading point (deterministic, the MIS contract from S6).
    double pdf_area(const ks::vec3d &p_shade, const ks::vec3d &n_shade, double u, double v) const;

    // Solid-angle pdf of the direction toward S(u, v) from the shading
    // point, for a BSDF-sampled hit at (u, v) on the visibility mesh.
    double pdf_omega_from_uv(const ks::vec3d &p_shade, const ks::vec3d &n_shade, double u, double v) const;
};

} // namespace dmap
