#include "line_sampling.h"
#include "ks/assertion.h"
#include <algorithm>

namespace dmap
{

using ks::vec2;
using ks::vec3;

//-----------------------------------------------------------------------------
// AllHitsMesh
//-----------------------------------------------------------------------------

namespace
{

// Context filter that records every hit and rejects it, so traversal
// continues past it (the standard embree all-hits pattern).
void record_all_hits_filter(const RTCFilterFunctionNArguments *args, void *payload)
{
    auto *hits = (std::vector<LineHit> *)payload;
    uint32_t N = args->N;
    int *valid = args->valid;
    RTCRayN *ray = args->ray;
    RTCHitN *hit = args->hit;
    for (uint32_t i = 0; i < N; ++i) {
        if (valid[i] != 0) {
            LineHit h;
            h.prim_id = RTCHitN_primID(hit, N, i);
            h.t = RTCRayN_tfar(ray, N, i);
            h.b1 = RTCHitN_u(hit, N, i);
            h.b2 = RTCHitN_v(hit, N, i);
            hits->push_back(h);
            valid[i] = 0;
        }
    }
}

} // namespace

AllHitsMesh::AllHitsMesh(const ks::EmbreeDevice &device, const ks::MeshData &data) : data(&data)
{
    geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
    ASSERT((data.vertices.size() - 1) % 3 == 0 && data.indices.size() % 3 == 0);
    rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, data.vertices.data(), 0,
                               sizeof(float[3]), (data.vertices.size() - 1) / 3);
    rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, data.indices.data(), 0,
                               sizeof(uint32_t[3]), data.indices.size() / 3);
    rtcCommitGeometry(geom);

    scene = rtcNewScene(device);
    rtcSetSceneBuildQuality(scene, RTC_BUILD_QUALITY_HIGH);
    rtcAttachGeometry(scene, geom);
    rtcCommitScene(scene);
}

AllHitsMesh::~AllHitsMesh()
{
    if (scene)
        rtcReleaseScene(scene);
    if (geom)
        rtcReleaseGeometry(geom);
}

void AllHitsMesh::all_hits(const vec3 &origin, const vec3 &dir, float tnear, float tfar,
                           std::vector<LineHit> &hits) const
{
    hits.clear();
    ks::IntersectContext ctx;
    ctx.add_filter(record_all_hits_filter, &hits);
    RTCRayHit rayhit = ks::spawn_rtcrayhit(origin, dir, tnear, tfar);
    rtcIntersect1(scene, (RTCIntersectContext *)&ctx, &rayhit);

    // A triangle stored in several BVH leaves can be reported more than
    // once when every hit is rejected; a line meets a triangle at most
    // once, so deduplicate by primitive.
    std::sort(hits.begin(), hits.end(), [](const LineHit &a, const LineHit &b) { return a.prim_id < b.prim_id; });
    hits.erase(std::unique(hits.begin(), hits.end(),
                           [](const LineHit &a, const LineHit &b) { return a.prim_id == b.prim_id; }),
               hits.end());
}

//-----------------------------------------------------------------------------
// LineSampler
//-----------------------------------------------------------------------------

LineSampler::LineSampler(const ks::AABB3 &box) : box(box)
{
    center = 0.5f * (box.min + box.max);
    rho = 0.5f * (box.max - box.min).norm();
}

bool LineSampler::sample(ks::RNG &rng, vec3 &origin, vec3 &dir, float &t_far) const
{
    dir = ks::sample_uniform_sphere(rng.next2d());
    vec3 X, Y;
    ks::orthonormal_basis(dir, X, Y);
    float u0 = (2.0f * rng.next() - 1.0f) * rho;
    float u1 = (2.0f * rng.next() - 1.0f) * rho;
    vec3 p = center + u0 * X + u1 * Y;

    // Slab test of the infinite line p + t*dir against the box, in double.
    double t_lo = -INFINITY, t_hi = INFINITY;
    for (int a = 0; a < 3; ++a) {
        double d = dir[a], o = p[a];
        if (std::abs(d) < 1e-12) {
            if (o < box.min[a] || o > box.max[a])
                return false;
        } else {
            double t0 = (box.min[a] - o) / d;
            double t1 = (box.max[a] - o) / d;
            if (t0 > t1)
                std::swap(t0, t1);
            t_lo = std::max(t_lo, t0);
            t_hi = std::min(t_hi, t1);
        }
    }
    if (t_lo >= t_hi)
        return false;

    // Start a small margin before the box entry and end past the exit, so
    // hits exactly on the boundary are kept.
    double margin = 1e-4 * rho;
    origin = p + (float)(t_lo - margin) * dir;
    t_far = (float)(t_hi - t_lo + 2.0 * margin);
    return true;
}

} // namespace dmap
