#pragma once
#include "displaced_intersector.h"
#include "ks/geometry.h"
#include "scene_spec.h"

// The displaced surface as an embree user geometry ("Plan — Path tracer
// with displaced surfaces", T6, D1): one user primitive per base triangle,
// bounded by T3's object-space AABB, intersected by T4's walk. The
// callbacks run in instance space, where the walk lives, and hand every
// crossing to embree's filter functions, so ks's context filters (the
// opacity map, the local-geometry filter) apply; a crossing they reject
// is skipped and the walk resumes behind it. A hit records the base
// triangle as the primitive and the base triangle's barycentric
// coordinates as (u, v), the convention of ks's meshes.
//
// The intersection record: the position S(s, t) and the derivatives Su,
// Sv of the true surface at the hit's parameters, rescaled to the
// object's texture coordinates (dpdu, dpdv); the geometric normal Su x Sv
// oriented along the base face's winding, then toward the viewer when
// the base mesh is two-sided, as for meshes; the shading frame equal to
// the geometric frame (the displaced surface has no separate shading
// normal, a normal map applies on top as for meshes); uv equal to the
// object's texture coordinates; and the uv partials from the ray
// differentials.

namespace dmap
{

struct DisplacedGeometry : ks::Geometry
{
    const DisplacedObject *object = nullptr;
    ObjectIntersector intersector; // the triangles and their boxes; embree holds the BVH
    TraversalOptions options;
    bool twosided = true; // the base mesh's flag
    // The kind of light sampler create_area_light builds: the object's
    // by default; a study changes it and rebuilds the scene's area lights
    // (test_emitter_ladder.cpp).
    EmitterSamplerKind emitter_sampler;

    DisplacedGeometry(const DisplacedObject &d, bool twosided);
    void create_rtc_geom(const ks::EmbreeDevice &device) override;
    ks::Intersection compute_intersection(const RTCRayHit &rayhit, const ks::Ray &ray,
                                          const ks::Transform &transform) const override;
    ks::vec2 compute_hit_texcoord(uint32_t prim_id, ks::vec2 uv) const override;
    // The displaced area lights of this geometry when the material emits
    // (displaced_area_light.h, T8).
    std::unique_ptr<ks::AreaLightShared> create_area_light(uint32_t inst_id, uint32_t geom_id,
                                                           const ks::Transform &transform,
                                                           const ks::Material &material) const override;

    // The surface at the base triangle's barycentric coordinates (u, v),
    // in object space: the position, the derivatives along the object's
    // texture coordinates, the unit normal along the displacement
    // direction, and the texture coordinates.
    struct SurfacePoint
    {
        ks::vec3d p, dpdu, dpdv, ng;
        ks::vec2d uv;
    };
    SurfacePoint surface_point(uint32_t prim_id, double u, double v) const;

    // Crossings the filters reject are skipped up to this many times per
    // primitive and ray.
    static constexpr int max_filter_passes = 64;
};

} // namespace dmap
