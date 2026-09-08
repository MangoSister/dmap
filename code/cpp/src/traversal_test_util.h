#pragma once
#include "displaced_intersector.h"
#include "displaced_tessellation.h"
#include "ks/embree_util.h"
#include "ks/geometry.h"
#include "ks/rng.h"
#include "scene_spec.h"
#include <memory>
#include <string>
#include <vector>

// Shared by the traversal's validation (T4) and cost (T5) tasks of "Plan —
// Path tracer with displaced surfaces": the summary of one displaced
// object, its two ray distributions in object space, and embree over a
// pre-tessellated substitute (the oracles of D4).

namespace dmap
{

struct RayQuery
{
    ks::vec3d o, d;
};

struct TraceResult
{
    bool hit = false;
    DisplacedHit h;
    int triangle = -1;
};

// The object intersector on the ray from t = 0 to infinity.
TraceResult trace(const ObjectIntersector &obj, const RayQuery &r, const TraversalOptions &options,
                  TraversalStats *stats = nullptr);

ks::vec3d uniform_direction(ks::RNG &rng);
// Uniform barycentric coordinates of a triangle (the weights of p1, p2).
ks::vec2d uniform_bary(ks::RNG &rng);

bool identity_chart(const BaseTriangle &tri);

// The figures of the per-object log line.
struct ObjectSummary
{
    int n_triangles = 0;
    std::vector<double> edges; // the mean edge of every base triangle
    double median_edge = 0.0;
    double median_span = 0.0; // the domain's larger extent, in leaf texels
    int max_root_level = 0;
    bool identity = false; // every triangle on the identity chart
};

ObjectSummary summarize_object(const DisplacedObject &d, const ObjectIntersector &obj);
// "<task> on <scene> object [<shape>]: ..." on the default logger.
void log_object(const char *task, const std::string &scene_name, const SceneSpec &spec, const DisplacedObject &d,
                const ObjectSummary &summary);

// The two ray distributions, in object space: up to n camera-like rays
// through the scene's camera that reach the object's bound (none without
// a camera), then n rays from origins near the surface: a uniform point
// of a random base triangle, moved by 0.1 to 3.2 mean edges (log-uniform)
// in a random direction, shot in a random direction.
struct ObjectRays
{
    std::vector<RayQuery> rays;
    int n_camera = 0; // rays[0, n_camera) are the camera rays
};

ObjectRays object_rays(const SceneSpec &spec, const DisplacedObject &d, const ObjectIntersector &obj,
                       const ObjectSummary &summary, int n, ks::RNG &rng);

// Twice the largest deviation between the surface and a pre-tessellated
// substitute over the interior faces of each base triangle, at the edge
// midpoints and the centroids.
std::vector<double> chord_errors(const DisplacedObject &d, const ks::MeshData &mesh,
                                 const std::vector<TexelFace> &faces);

// Embree over a pre-tessellated substitute of one displaced object, rays
// in object space. Every face knows its base triangle and the tile
// coordinates of its vertices.
struct MeshOracle
{
    ks::MeshData mesh;
    std::vector<TexelFace> faces;
    ks::MeshGeometry geom;
    RTCScene scene = nullptr;

    MeshOracle(const ks::EmbreeDevice &device, ks::MeshData &&data, std::vector<TexelFace> &&face_record);
    MeshOracle(const MeshOracle &) = delete;
    ~MeshOracle();

    // The barycentric-grid substitute at subdivision n (scene_spec.h
    // tessellated) with its face record.
    static std::unique_ptr<MeshOracle> barycentric(const ks::EmbreeDevice &device, const SceneSpec &spec,
                                                   const DisplacedObject &d, int n);
    // The texel-aligned substitute with m sub-cells per texel.
    static std::unique_ptr<MeshOracle> texel_aligned(const ks::EmbreeDevice &device, const SceneSpec &spec,
                                                     const DisplacedObject &d, int m);

    struct Hit
    {
        bool hit = false;
        double t = INFINITY;
        int prim = -1;
        double u = 0.0, v = 0.0; // the micro-triangle's barycentric weights of its second and third vertex
        ks::vec3d ng = ks::vec3d::Zero();
        int triangle = -1;
        ks::vec2d st = ks::vec2d::Zero();
    };

    Hit intersect(const RayQuery &r) const;

    // Twice the largest deviation between the surface and the linear
    // interpolant over the interior faces of each base triangle, at the
    // edge midpoints and the centroids.
    std::vector<double> chord_errors(const DisplacedObject &d) const;

    // The nearest crossing of the ray with the faces of base triangle k
    // alone, in double, for diagnosis.
    double nearest_on_triangle(const RayQuery &r, int k) const;

    // The bytes of the vertex, normal, texture coordinate and index arrays.
    size_t array_bytes() const;
};

} // namespace dmap
