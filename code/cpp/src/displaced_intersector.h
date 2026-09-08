#pragma once
#include "node_bounds.h"
#include "uv_clip.h"
#include <cstdint>
#include <embree3/rtcore.h>
#include <vector>

// The ray traversal of one displaced base triangle ("Plan — Path tracer
// with displaced surfaces", T4, D2): TFDM's stackless walk over the
// implicit quadtree of the triangle's texture footprint (GfxExp
// tfdm_shared.h: findRoots, down, next, up; tfdm_intersection_kernels.h:
// displacedSurface_generic), in double, with the node test of T3 selected
// by the traversal options and one of two leaf intersectors.
//
// The walk. The roots are the cells of the footprint's root block that
// meet the domain's bounding box, at the coarsest level where that box
// spans at most two cells per axis. From each root the walk visits the
// subtree depth first without a stack: `cell_down` enters the child the
// ray meets first (the signs of the ray direction fix the child order),
// `cell_next` moves to the next sibling in that order or climbs to the
// parent's next sibling, and the root's subtree ends when `cell_next`
// returns the cell that follows the root. Cells outside the domain are
// skipped by the square-versus-triangle test (TFDM's texel discard)
// before any node data is read; the others are tested against the ray
// with the current maximum distance, and the interval test of
// node_bounds.h decides the descent. The leaf level is the texel level;
// there is no level of detail (D2).
//
// Leaves. Two-triangle: the texel's four corner points S split along the
// diagonal from (i + 1, j) to (i, j + 1), the split of tessellation_grid,
// so that on the identity chart the leaves are exactly the pre-tessellated
// mesh at texel resolution (D4); hits outside the domain are rejected.
// Newton, certified: with d1, d2 an orthonormal basis of the plane normal
// to the ray, the crossings are the roots of
//     F(s, t) = ((S(s, t) - o) · d1, (S(s, t) - o) · d2) = 0
// on the true surface S = P + h N̂, h the texel's bilinear interpolant.
// GfxExp runs Newton's method from the texel centre, which on rough
// content finds the far root or nothing for about one ray in a hundred,
// and seeding it from the chord or bilinear-patch crossings still loses
// the nearer of two close roots for about one ray in ten thousand (log
// 2026-09-07). Here the texel is searched as a quadtree with the affine
// bounds of the traversal: over a sub-square, P is affine, h is affine
// plus a twist remainder, the normalized normal is the texel's form
// restricted to the sub-square, and F's two interval bounds exclude a
// sub-square that holds no root; the survivors are visited nearest first
// by their bound on t, pruned by the best root, and at subdivision_depth
// Newton's method runs from the sub-square's centre, with the iterate
// clamped to the texel and GfxExp's stopping rules, accepted when the
// residual is below the tolerance, (s, t) lies in the domain and
// t = d · (S - o) / |d|² is in range. A root can only be missed when two
// crossings share one finest sub-square.
//
// Rays and hits are in object space; the tangent frame of node_bounds.h
// is applied inside, and the ray parameter is the same in both spaces.

namespace dmap
{

// A cell of the implicit quadtree, TFDM's Texel (x, y, lod).
struct Cell
{
    int level = 0;
    int64_t i = 0, j = 0;

    bool operator==(const Cell &r) const { return level == r.level && i == r.i && j == r.j; }
    bool operator!=(const Cell &r) const { return !(*this == r); }
};

// TFDM's walk. sign_i and sign_j are set when the ray direction is
// negative along that axis, so that the child the ray meets first comes
// first. cell_next climbs until it leaves root_level, where it returns the
// cell after the root.
void cell_up(Cell &c);
void cell_down(Cell &c, bool sign_i, bool sign_j);
void cell_next(Cell &c, bool sign_i, bool sign_j, int root_level);

// One node test as the walk made it, kept when TraversalStats::records
// is set (T5): the decisions of the box and the slab tests at that node
// with the walk's current maximum distance, and the lengths of their
// intervals without it (-1 when the test without it fails), T3's measure
// of tightness on the walk's own nodes.
struct NodeTestRecord
{
    int level = 0;
    bool box_pass = false, slab_pass = false;
    double box_length = -1.0, slab_length = -1.0, both_length = -1.0;
};

struct TraversalStats
{
    static constexpr int max_levels = 32;
    int64_t triangle_tests = 0; // per-triangle AABB tests (ObjectIntersector)
    int64_t node_tests = 0;
    int64_t leaf_tests = 0;
    int64_t sub_squares = 0; // bounded by the certified Newton leaf
    int64_t node_tests_per_level[max_levels] = {};
    // Tests made after the walk's final hit, in walks that hit: what a
    // front-to-back child order with an early exit could save at most
    // (plan, open decisions).
    int64_t node_tests_after_hit = 0, leaf_tests_after_hit = 0;
    // When set, every node test of the walk is appended.
    std::vector<NodeTestRecord> *records = nullptr;

    void add(const TraversalStats &other);
};

struct DisplacedHit
{
    double t = INFINITY;
    ks::vec2d st = ks::vec2d::Zero();   // tile parameterization
    ks::vec2d bary = ks::vec2d::Zero(); // the base triangle's barycentric coordinates (weights of p1, p2)
    ks::vec3d ng = ks::vec3d::Zero();   // unit geometric normal, object space
    int64_t i = 0, j = 0;               // the leaf texel
};

struct DisplacedIntersector
{
    const BaseTriangle *tri = nullptr;
    const TaylorPyramid *pyramid = nullptr;
    HeightGrid field;
    TangentFrame frame;
    UvTriangle domain;
    int root_level = 0;
    Cell roots[4];
    int n_roots = 0;
    // Newton: the residual normal to the ray, object units, and the
    // iteration cap. The tolerance is 1e-10 times the mean edge.
    double residual_tolerance = 0.0;
    int max_iterations = 20;
    // The certified leaf subdivides the texel this many times (finest
    // sub-square 2^-depth of the texel) before Newton runs.
    int subdivision_depth = 5;

    DisplacedIntersector(const BaseTriangle &tri, const TaylorPyramid &pyramid, const HeightGrid &field);

    // Ray in object space, hits with t in (t_min, t_max).
    bool intersect(const ks::vec3d &o, const ks::vec3d &d, double t_min, double t_max, const TraversalOptions &options,
                   DisplacedHit &hit, TraversalStats *stats = nullptr) const;

    // The leaf intersectors on texel (i, j), exposed for validation. The
    // Newton leaf counts the sub-squares it bounded when asked.
    bool intersect_newton(const ks::vec3d &o, const ks::vec3d &d, double t_min, double t_max, int64_t i, int64_t j,
                          DisplacedHit &hit, int64_t *sub_squares = nullptr) const;
    // One Newton run from a starting point (s, t) in the texel.
    bool newton_from(const ks::vec3d &o, const ks::vec3d &d, double t_min, double t_max, int64_t i, int64_t j,
                     ks::vec2d guess, DisplacedHit &hit) const;
    bool intersect_two_triangle(const ks::vec3d &o, const ks::vec3d &d, double t_min, double t_max, int64_t i,
                                int64_t j, DisplacedHit &hit) const;

    // The surface the two-triangle leaves describe, at (s, t).
    ks::vec3d two_triangle_position(double s, double t) const;

  private:
    struct LeafBound;
    // The bound of one sub-square (centre (a, b), half-width rho, texel
    // units): false when the ray cannot cross the surface there or the
    // crossing cannot be in (t_min, t_max); otherwise its lower bound on t.
    bool sub_square_bound(const LeafBound &leaf, double a, double b, double rho, double t_min, double t_max,
                          double &t_lo) const;
    void search_leaf(const LeafBound &leaf, int64_t i, int64_t j, int depth, double a, double b, double rho,
                     double t_min, double &t_max, DisplacedHit &hit, bool &found, int64_t &sub_squares) const;
};

// Floats for embree: the float at or below x, the float at or above x,
// and a box widened by a few float ulps of its coordinates so that a
// float ray cannot slip past a box the double ray meets.
float float_below(double x);
float float_above(double x);
void rtc_bounds(const Aabb3 &box, RTCBounds &out);

// Every base triangle of one displaced object behind its object-space
// AABB (T3): a flat loop over the boxes or, after build_bvh, embree over
// the same boxes as user geometry with the triangle's walk as the
// intersection callback (T6's arrangement). Rays and hits stay in double;
// embree sees the float ray and the boxes widened to floats.
struct ObjectIntersector
{
    std::vector<DisplacedIntersector> triangles;
    std::vector<Aabb3> boxes;
    Aabb3 bound;
    RTCScene scene = nullptr;

    ObjectIntersector(const std::vector<BaseTriangle> &triangles, const TaylorPyramid &pyramid,
                      const HeightGrid &field);
    ~ObjectIntersector();
    // embree keeps a pointer to the object.
    ObjectIntersector(const ObjectIntersector &) = delete;
    ObjectIntersector &operator=(const ObjectIntersector &) = delete;

    void build_bvh(RTCDevice device);

    // The BVH when built, the flat loop otherwise.
    bool intersect(const ks::vec3d &o, const ks::vec3d &d, double t_min, double t_max, const TraversalOptions &options,
                   DisplacedHit &hit, int &triangle, TraversalStats *stats = nullptr) const;
    bool intersect_flat(const ks::vec3d &o, const ks::vec3d &d, double t_min, double t_max,
                        const TraversalOptions &options, DisplacedHit &hit, int &triangle,
                        TraversalStats *stats = nullptr) const;
};

} // namespace dmap
