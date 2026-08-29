#pragma once
#include "ks/aabb.h"
#include "ks/embree_util.h"
#include "ks/geometry.h"
#include "ks/rng.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

// Uniform sampling of a surface by casting random lines (Ling, Madan,
// Sharp & Jacobson 2025, "Uniform Sampling of Surfaces by Casting Rays"),
// ported to displaced surfaces: the surface is the pre-tessellated mesh
// (plan phase S2) and all intersections along a line come from embree with
// a recording filter, replacing the paper's modified sphere tracing.
//
// Line measure and estimator. A line is a uniform direction omega on the
// sphere plus a uniform origin offset on the perpendicular square of
// half-side rho (the box half-diagonal) through the box centre, so every
// line meeting the box is reachable. For a fixed direction the hits of the
// line family on a surface have density |n . omega| / offset_area per unit
// area; averaging over the sphere gives E|n . omega| = 1/2. Hence
//
//     integral_S f dA  ~=  (2 * offset_area / M) * sum_lines sum_hits f(x)
//
// with M counting ALL sampled lines, including those that miss the box.
// (The official code normalizes differently because it resamples until a
// fixed count of box-hitting rays; counting misses in M keeps the constant
// exact. Validated against analytic areas in validate_line_sampling.)

namespace dmap
{

// One recorded intersection along a line.
struct LineHit
{
    uint32_t prim_id;
    float t;
    float b1, b2; // embree barycentrics of vertices 1 and 2
};

// Embree scene over one MeshData with an all-hits query. The MeshData must
// outlive this object (shared buffers).
struct AllHitsMesh
{
    AllHitsMesh(const ks::EmbreeDevice &device, const ks::MeshData &data);
    ~AllHitsMesh();
    AllHitsMesh(const AllHitsMesh &) = delete;
    AllHitsMesh &operator=(const AllHitsMesh &) = delete;

    // All intersections in [tnear, tfar], deduplicated by primitive
    // (a line meets a triangle at most once). Clears and fills hits.
    void all_hits(const ks::vec3 &origin, const ks::vec3 &dir, float tnear, float tfar,
                  std::vector<LineHit> &hits) const;

    ks::AABB3 bound() const { return ks::scene_bound(scene); }

    RTCScene scene = nullptr;
    RTCGeometry geom = nullptr;
    const ks::MeshData *data = nullptr;
};

// Uniform random lines through an axis-aligned box.
struct LineSampler
{
    explicit LineSampler(const ks::AABB3 &box);

    // Draws one line. Returns false when it misses the box (the line still
    // counts toward M in the estimator). On success the origin sits at the
    // box entry minus a small margin and t_far spans the box exit.
    bool sample(ks::RNG &rng, ks::vec3 &origin, ks::vec3 &dir, float &t_far) const;

    double offset_area() const { return 4.0 * (double)rho * rho; }

    ks::vec3 center;
    float rho; // half-diagonal of the box
    ks::AABB3 box;
};

// Estimate of integral_S f dA over the mesh surface, with the standard
// error of the mean (SE). Each line contributes one independent value
// X = 2 * offset_area * sum_hits f; the estimate is the mean of the X and
// SE = stddev(X) / sqrt(n_lines) is its statistical uncertainty: how far a
// rerun with another seed typically lands, shrinking as 1/sqrt(n_lines).
// An unbiased estimate sits within a few SE of the true value.
// f is called per hit as f(hit); n_lines counts all sampled lines. Also
// accumulates per-primitive hit counts when prim_hit_counts is non-null
// (for uniformity checks).
template <typename F>
std::pair<double, double> estimate_surface_integral(const AllHitsMesh &mesh, const LineSampler &lines, ks::RNG &rng,
                                                    int64_t n_lines, F &&f,
                                                    std::vector<int64_t> *prim_hit_counts = nullptr)
{
    std::vector<LineHit> hits;
    double sum = 0.0, sum_sq = 0.0;
    const double c = 2.0 * lines.offset_area();
    for (int64_t l = 0; l < n_lines; ++l) {
        ks::vec3 o, d;
        float t_far;
        double x = 0.0;
        if (lines.sample(rng, o, d, t_far)) {
            mesh.all_hits(o, d, 0.0f, t_far, hits);
            for (const LineHit &h : hits) {
                x += f(h);
                if (prim_hit_counts)
                    ++(*prim_hit_counts)[h.prim_id];
            }
        }
        x *= c;
        sum += x;
        sum_sq += x * x;
    }
    double mean = sum / n_lines;
    double var = std::max(0.0, sum_sq / n_lines - mean * mean) / std::max<int64_t>(1, n_lines - 1);
    return {mean, std::sqrt(var)};
}

} // namespace dmap
