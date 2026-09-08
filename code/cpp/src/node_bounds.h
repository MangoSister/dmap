#pragma once
#include "affine_form.h"
#include "displaced_surface.h"
#include "taylor_pyramid.h"
#include "traversal_options.h"
#include "uv_clip.h"
#include <cstdint>

// The per-node tests of the displaced-surface traversal ("Plan — Path
// tracer with displaced surfaces", D2 and D3), on one base triangle in
// TFDM's uv-aligned tangent space (GfxExp tfdm_intersection_kernels.h,
// tfdm_preprocess_kernels.cu, tfdm_main.cpp).
//
// Tangent space. The linear map [e1 e2 ng]^-1 about q0 sends the base
// point P(s, t) to (s, t, 0) and the geometric normal ng to (0, 0, 1), so a
// displaced point S = P + h N̂ goes to
//     (s, t, 0) + h m(s, t),   m = to_tangent N̂,   m_z = N̂ · ng.
// Rays keep their object-space parameter, so every t-interval below is in
// object units.
//
// Box (TFDM). Over a cell clipped to the triangle's texture bounding box,
// (s, t) and the interpolated normal are affine forms in the cell symbols;
// the normal is normalized by the Min-Range rule, mapped to tangent space,
// multiplied by the height form mid ± half-range of the min-max channel,
// and the three interval hulls are the AABB. Its ray clip is the box test.
//
// Slab (D3). With h = h0 + gu (s - u0) + gv (t - v0) + ρ, |ρ| <= r, the
// Taylor channels of the same node, eliminating s, t and ρ from the tangent
// coordinates (x, y, z) gives one linear form with interval coefficients,
//     F(x, y, z) = A z + B x + C y + D in [-R, R],
//     A = 1 + gu m_x + gv m_y,  B = -gu m_z,  C = -gv m_z,
//     D = -(h0 - gu u0 - gv v0) m_z,  R = r max|m_z|,
// m ranging over its interval hull. Along a ray F is affine in t with
// interval coefficients, and the admissible t are those where that
// interval meets [-R, R]. For constant m these are two planes tilted by
// (gu, gv) and 2r apart; as m varies the region widens and stays
// conservative. The slab is unbounded sideways, so the slab test first
// clips the ray to the cell's column, the x and y extents of the box, and
// anchors the form at the column's entry, where the coefficient widths
// have the least distance to grow over.
// Both is the intersection of the two intervals.

namespace dmap
{

struct TangentFrame
{
    Eigen::Matrix3d to_tangent, from_tangent;
    ks::vec3d origin, ng;

    ks::vec3d point(const ks::vec3d &p) const { return to_tangent * (p - origin); }
    ks::vec3d direction(const ks::vec3d &d) const { return to_tangent * d; }
};
TangentFrame tangent_frame(const BaseTriangle &tri);

struct TangentRay
{
    ks::vec3d o, d;
    double t_min = 0.0, t_max = INFINITY;
};

struct Aabb3
{
    ks::vec3d lo, hi;

    // The ray parameter interval inside the box, intersected with [t0, t1].
    bool clip(const ks::vec3d &o, const ks::vec3d &d, double &t0, double &t1) const;
    bool contains(const ks::vec3d &p, double tolerance) const;
    void expand(const Aabb3 &other);
};

// What both tests need for one cell of one triangle.
struct NodeBound
{
    int level = 0;
    int64_t i = 0, j = 0;
    TaylorNode node;            // heights in world units, plane anchored at the cell centre
    ks::vec2d centre;           // the cell centre (u0, v0)
    ks::vec2d rect_lo, rect_hi; // the cell clipped to the triangle's texture bounding box
    AffineVec3 m;               // to_tangent N̂ over the rectangle
    Aabb3 box;                  // TFDM's AABB in tangent space
};
NodeBound node_bound(const BaseTriangle &tri, const TangentFrame &frame, const TaylorPyramid &pyramid, int level,
                     int64_t i, int64_t j);

struct SlabForm
{
    Interval A, B, C, D;
    double R = 0.0;
    Interval eval(const ks::vec3d &p) const;
};
SlabForm slab_form(const NodeBound &nb);

// Each returns false when the ray misses; otherwise [t0, t1] is the
// admissible interval within [ray.t_min, ray.t_max]. Rays start at t >= 0.
bool box_interval(const NodeBound &nb, const TangentRay &ray, double &t0, double &t1);
bool slab_interval(const NodeBound &nb, const TangentRay &ray, double &t0, double &t1);
bool both_interval(const NodeBound &nb, const TangentRay &ray, double &t0, double &t1);
double interval_tolerance(double t_scale);
// The test the options select at this node's level (D2's level policy).
bool node_interval(const NodeBound &nb, const TangentRay &ray, const TraversalOptions &options, double &t0, double &t1);

// TFDM's per-triangle pass: the height range over the cells of the
// triangle's footprint, and the object-space AABB of the displaced
// triangle by affine arithmetic over three parallelograms covering it.
void triangle_height_range(const BaseTriangle &tri, const TaylorPyramid &pyramid, double &h_min, double &h_max);
Aabb3 triangle_aabb(const BaseTriangle &tri, double h_min, double h_max);

} // namespace dmap
