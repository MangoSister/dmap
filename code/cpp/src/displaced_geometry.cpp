#include "displaced_geometry.h"
#include "ks/assertion.h"
#include <cmath>
#include <limits>

using namespace ks;

namespace dmap
{

namespace
{

void bounds_callback(const RTCBoundsFunctionArguments *args)
{
    const DisplacedGeometry *g = (const DisplacedGeometry *)args->geometryUserPtr;
    rtc_bounds(g->intersector.boxes[args->primID], *args->bounds_o);
}

// The potential hit the filters judge, from the walk's hit.
RTCHit potential_hit(const DisplacedHit &h, unsigned prim_id, unsigned geom_id, const RTCIntersectContext *context)
{
    RTCHit hit;
    hit.Ng_x = (float)h.ng[0];
    hit.Ng_y = (float)h.ng[1];
    hit.Ng_z = (float)h.ng[2];
    hit.u = (float)h.bary[0];
    hit.v = (float)h.bary[1];
    hit.primID = prim_id;
    hit.geomID = geom_id;
    for (unsigned l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l)
        hit.instID[l] = context->instID[l];
    return hit;
}

RTCFilterFunctionNArguments filter_arguments(int *valid, void *user_ptr, RTCIntersectContext *context, RTCRay *ray,
                                             RTCHit *hit)
{
    RTCFilterFunctionNArguments fargs;
    fargs.valid = valid;
    fargs.geometryUserPtr = user_ptr;
    fargs.context = context;
    fargs.ray = (RTCRayN *)ray;
    fargs.hit = (RTCHitN *)hit;
    fargs.N = 1;
    return fargs;
}

void intersect_callback(const RTCIntersectFunctionNArguments *args)
{
    if (!args->valid[0])
        return;
    ASSERT(args->N == 1, "the displaced geometry intersects one ray at a time");
    const DisplacedGeometry *g = (const DisplacedGeometry *)args->geometryUserPtr;
    RTCRayHit *rayhit = (RTCRayHit *)args->rayhit;
    RTCRay &ray = rayhit->ray;
    vec3d o(ray.org_x, ray.org_y, ray.org_z), d(ray.dir_x, ray.dir_y, ray.dir_z);
    double t_min = ray.tnear, t_max = ray.tfar;
    const DisplacedIntersector &tri = g->intersector.triangles[args->primID];
    for (int pass = 0; pass < DisplacedGeometry::max_filter_passes; ++pass) {
        DisplacedHit h;
        if (!tri.intersect(o, d, t_min, t_max, g->options, h))
            return;
        RTCHit hit = potential_hit(h, args->primID, args->geomID, args->context);
        int valid = -1;
        RTCFilterFunctionNArguments fargs = filter_arguments(&valid, args->geometryUserPtr, args->context, &ray, &hit);
        float t_far = ray.tfar;
        ray.tfar = (float)h.t;
        rtcFilterIntersection(args, &fargs);
        if (valid == -1) {
            rayhit->hit = hit;
            return;
        }
        ray.tfar = t_far;
        t_min = h.t; // the walk accepts t > t_min only
    }
}

void occluded_callback(const RTCOccludedFunctionNArguments *args)
{
    if (!args->valid[0])
        return;
    ASSERT(args->N == 1, "the displaced geometry occludes one ray at a time");
    const DisplacedGeometry *g = (const DisplacedGeometry *)args->geometryUserPtr;
    RTCRay &ray = *(RTCRay *)args->ray;
    vec3d o(ray.org_x, ray.org_y, ray.org_z), d(ray.dir_x, ray.dir_y, ray.dir_z);
    double t_min = ray.tnear, t_max = ray.tfar;
    const DisplacedIntersector &tri = g->intersector.triangles[args->primID];
    for (int pass = 0; pass < DisplacedGeometry::max_filter_passes; ++pass) {
        DisplacedHit h;
        if (!tri.intersect(o, d, t_min, t_max, g->options, h))
            return;
        RTCHit hit = potential_hit(h, args->primID, args->geomID, args->context);
        int valid = -1;
        RTCFilterFunctionNArguments fargs = filter_arguments(&valid, args->geometryUserPtr, args->context, &ray, &hit);
        float t_far = ray.tfar;
        ray.tfar = (float)h.t;
        rtcFilterOcclusion(args, &fargs);
        if (valid == -1) {
            ray.tfar = -std::numeric_limits<float>::infinity();
            return;
        }
        ray.tfar = t_far;
        t_min = h.t;
    }
}

} // namespace

DisplacedGeometry::DisplacedGeometry(const DisplacedObject &d, bool twosided)
    : object(&d), intersector(d.triangles, *d.asset->pyramid, d.field), options(d.traversal), twosided(twosided),
      emitter_sampler(d.emitter_sampler)
{}

void DisplacedGeometry::create_rtc_geom(const EmbreeDevice &device)
{
    rtcgeom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_USER);
    rtcSetGeometryUserPrimitiveCount(rtcgeom, (unsigned)intersector.triangles.size());
    rtcSetGeometryUserData(rtcgeom, this);
    rtcSetGeometryBoundsFunction(rtcgeom, bounds_callback, this);
    rtcSetGeometryIntersectFunction(rtcgeom, intersect_callback);
    rtcSetGeometryOccludedFunction(rtcgeom, occluded_callback);
    rtcCommitGeometry(rtcgeom);
}

DisplacedGeometry::SurfacePoint DisplacedGeometry::surface_point(uint32_t prim_id, double u, double v) const
{
    const BaseTriangle &tri = object->triangles[prim_id];
    vec2d st = tri.param(u, v);
    double s = st[0], t = st[1];
    NormalFrame f = normal_frame_at(tri, s, t);
    double h = object->field.h(s, t);
    vec2d gh = object->field.grad(s, t);
    // S = P + h N; Su = e1 + hu N + h Nu, Sv likewise (displaced_surface.h).
    vec3d Ss = tri.e1 + gh[0] * f.N + h * f.Nu;
    vec3d St = tri.e2 + gh[1] * f.N + h * f.Nv;
    SurfacePoint sp;
    sp.p = tri.P(s, t) + h * f.N;
    // Oriented along the base face's winding, as ks's meshes are.
    sp.ng = Ss.cross(St);
    if (sp.ng.dot(winding_normal(tri)) < 0.0)
        sp.ng = -sp.ng;
    sp.ng.normalize();
    if (!sp.ng.allFinite())
        sp.ng = winding_normal(tri).normalized();
    // Tile coordinates are (u, v') uv_scale + uv_offset with v' = 1 - v
    // under flip_v (scene_spec.h).
    sp.dpdu = Ss * object->uv_scale[0];
    sp.dpdv = St * (object->flip_v ? -object->uv_scale[1] : object->uv_scale[1]);
    sp.uv = object->object_uv(st);
    return sp;
}

Intersection DisplacedGeometry::compute_intersection(const RTCRayHit &rayhit, const Ray &ray,
                                                     const Transform &transform) const
{
    Intersection it;
    it.thit = rayhit.ray.tfar;
    SurfacePoint sp = surface_point(rayhit.hit.primID, rayhit.hit.u, rayhit.hit.v);
    it.p = sp.p.cast<float>();
    it.dpdu = sp.dpdu.cast<float>();
    it.dpdv = sp.dpdv.cast<float>();
    it.uv = sp.uv.cast<float>();
    vec3 ng = sp.ng.cast<float>();

    // The geometric frame as MeshGeometry builds it: ng is respected, the
    // tangent follows dpdu.
    vec3 b = ng.cross(it.dpdu).normalized();
    if (!b.allFinite())
        b = ng.cross(std::abs(ng.x()) < 0.9f ? vec3::UnitX() : vec3::UnitY()).normalized();
    vec3 t = b.cross(ng).normalized();
    if (twosided) {
        vec3 ray_dir = vec3(rayhit.ray.dir_x, rayhit.ray.dir_y, rayhit.ray.dir_z).normalized();
        vec3 wo_object = transform_dir(transform.inv, -ray_dir);
        if (wo_object.dot(ng) < 0.0f) {
            ng = -ng;
            it.dpdu = -it.dpdu;
            t = -t;
        }
    }
    it.frame = Frame(t, b, ng);
    it.sh_frame = it.frame;

    it = transform_it(transform, it);
    it.compute_uv_partials(ray);
    return it;
}

vec2 DisplacedGeometry::compute_hit_texcoord(uint32_t prim_id, vec2 uv) const
{
    const BaseTriangle &tri = object->triangles[prim_id];
    return object->object_uv(tri.param(uv[0], uv[1])).cast<float>();
}

} // namespace dmap
