#include "node_bounds.h"
#include "descent_sampler.h"
#include "ks/assertion.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

TangentFrame tangent_frame(const BaseTriangle &tri)
{
    TangentFrame frame;
    frame.origin = tri.q0;
    frame.ng = tri.e1.cross(tri.e2).normalized();
    frame.from_tangent.col(0) = tri.e1;
    frame.from_tangent.col(1) = tri.e2;
    frame.from_tangent.col(2) = frame.ng;
    frame.to_tangent = frame.from_tangent.inverse();
    return frame;
}

bool Aabb3::clip(const vec3d &o, const vec3d &d, double &t0, double &t1) const
{
    for (int a = 0; a < 3; ++a) {
        if (d[a] != 0.0) {
            double ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
            if (ta > tb)
                std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
        } else if (o[a] < lo[a] || o[a] > hi[a]) {
            return false;
        }
    }
    // A flat box is one plane; its crossing must survive rounding.
    if (t0 > t1 + interval_tolerance(std::max(std::abs(t0), std::abs(t1))))
        return false;
    if (t0 > t1)
        std::swap(t0, t1);
    return true;
}

bool Aabb3::contains(const vec3d &p, double tolerance) const
{
    for (int a = 0; a < 3; ++a)
        if (p[a] < lo[a] - tolerance || p[a] > hi[a] + tolerance)
            return false;
    return true;
}

void Aabb3::expand(const Aabb3 &other)
{
    lo = lo.cwiseMin(other.lo);
    hi = hi.cwiseMax(other.hi);
}

namespace
{

// The interpolated normal over a rectangle of the chart, centre (cx, cy)
// and half-extents (hu, hv): N = m0 + s Mu + t Mv is affine in the symbols.
AffineVec3 normal_form(const BaseTriangle &tri, double cx, double cy, double hu, double hv)
{
    return AffineVec3(tri.m0 + cx * tri.Mu + cy * tri.Mv, hu * tri.Mu, hv * tri.Mv);
}

Aabb3 hull(const AffineVec3 &S)
{
    Aabb3 box;
    for (int a = 0; a < 3; ++a) {
        Interval range = S.interval(a);
        box.lo[a] = range.lo;
        box.hi[a] = range.hi;
    }
    return box;
}

} // namespace

NodeBound node_bound(const BaseTriangle &tri, const TangentFrame &frame, const TaylorPyramid &pyramid, int level,
                     int64_t i, int64_t j)
{
    NodeBound nb;
    nb.level = level;
    nb.i = i;
    nb.j = j;
    nb.node = pyramid.node(level, i, j);
    double w = pyramid.cell_width(level);
    nb.centre = vec2d((i + 0.5) * w, (j + 0.5) * w);

    // TFDM clips the cell to the triangle's texture bounding box.
    vec2d tri_lo = tri.t0.cwiseMin(tri.t1).cwiseMin(tri.t2), tri_hi = tri.t0.cwiseMax(tri.t1).cwiseMax(tri.t2);
    nb.rect_lo = vec2d(i * w, j * w).cwiseMax(tri_lo);
    nb.rect_hi = vec2d((i + 1) * w, (j + 1) * w).cwiseMin(tri_hi);
    vec2d c = 0.5 * (nb.rect_lo + nb.rect_hi);
    vec2d half = (0.5 * (nb.rect_hi - nb.rect_lo)).cwiseMax(0.0);

    AffineVec3 N = normal_form(tri, c[0], c[1], half[0], half[1]);
    N.normalize();
    nb.m = frame.to_tangent * N;

    AffineForm h(0.5 * (nb.node.h_min + nb.node.h_max), 0.0, 0.0, 0.5 * (nb.node.h_max - nb.node.h_min));
    AffineVec3 p(vec3d(c[0], c[1], 0.0), vec3d(half[0], 0.0, 0.0), vec3d(0.0, half[1], 0.0));
    nb.box = hull(p + h * nb.m);
    return nb;
}

SlabForm slab_form(const NodeBound &nb)
{
    const TaylorNode &n = nb.node;
    Interval mx = nb.m.interval(0), my = nb.m.interval(1), mz = nb.m.interval(2);
    SlabForm f;
    f.A = Interval::point(1.0) + n.gu * mx + n.gv * my;
    f.B = -n.gu * mz;
    f.C = -n.gv * mz;
    f.D = -(n.h0 - n.gu * nb.centre[0] - n.gv * nb.centre[1]) * mz;
    f.R = n.r * mz.magnitude();
    return f;
}

Interval SlabForm::eval(const vec3d &p) const { return p[2] * A + p[0] * B + p[1] * C + D; }

// Rounding room for the ends of a t-interval: a few ulps of the largest
// distance involved, so a degenerate interval survives as a point.
double interval_tolerance(double t_scale) { return 1e-9 * std::max(1.0, t_scale); }

bool box_interval(const NodeBound &nb, const TangentRay &ray, double &t0, double &t1)
{
    t0 = ray.t_min;
    t1 = ray.t_max;
    return nb.box.clip(ray.o, ray.d, t0, t1);
}

bool slab_interval(const NodeBound &nb, const TangentRay &ray, double &t0, double &t1)
{
    // The cell's column, the x and y extents of the box, first: the slab
    // is unbounded sideways, and anchoring the form at the column's entry
    // keeps the interval coefficients from widening over the distance from
    // the ray origin.
    Aabb3 column = nb.box;
    column.lo[2] = -INFINITY;
    column.hi[2] = INFINITY;
    t0 = ray.t_min;
    t1 = ray.t_max;
    if (!column.clip(ray.o, ray.d, t0, t1))
        return false;
    vec3d anchor = ray.o + t0 * ray.d;
    double span = t1 - t0;

    SlabForm f = slab_form(nb);
    Interval F0 = f.eval(anchor);
    Interval F1 = ray.d[2] * f.A + ray.d[0] * f.B + ray.d[1] * f.C;
    // For s in [0, span] the interval F0 + s F1 is [F0.lo + s F1.lo,
    // F0.hi + s F1.hi]; it meets [-R, R] iff its low end is at most R and
    // its high end at least -R, two linear inequalities in s.
    double s0 = 0.0, s1 = span;
    if (F1.lo > 0.0)
        s1 = std::min(s1, (f.R - F0.lo) / F1.lo);
    else if (F1.lo < 0.0)
        s0 = std::max(s0, (f.R - F0.lo) / F1.lo);
    else if (F0.lo > f.R)
        return false;
    if (F1.hi > 0.0)
        s0 = std::max(s0, (-f.R - F0.hi) / F1.hi);
    else if (F1.hi < 0.0)
        s1 = std::min(s1, (-f.R - F0.hi) / F1.hi);
    else if (F0.hi < -f.R)
        return false;
    if (s0 > s1 + interval_tolerance(std::abs(t0) + span))
        return false;
    if (s0 > s1)
        std::swap(s0, s1);
    t1 = t0 + s1;
    t0 = t0 + s0;
    return true;
}

bool both_interval(const NodeBound &nb, const TangentRay &ray, double &t0, double &t1)
{
    double b0, b1, s0, s1;
    if (!box_interval(nb, ray, b0, b1) || !slab_interval(nb, ray, s0, s1))
        return false;
    t0 = std::max(b0, s0);
    t1 = std::min(b1, s1);
    // Over a flat cell both intervals degenerate to the one crossing of a
    // plane, each rounded differently; the intersection must not lose it
    // (log 2026-09-07: 14% of the surface points of a panelled map).
    double tol = interval_tolerance(std::max(std::abs(t0), std::abs(t1)));
    if (t0 > t1 + tol)
        return false;
    if (t0 > t1)
        std::swap(t0, t1);
    return true;
}

bool node_interval(const NodeBound &nb, const TangentRay &ray, const TraversalOptions &options, double &t0, double &t1)
{
    bool read_slab = nb.level <= options.slab_max_level;
    switch (options.bound) {
    case BoundMode::Box:
        return box_interval(nb, ray, t0, t1);
    case BoundMode::Slab:
        return read_slab ? slab_interval(nb, ray, t0, t1) : box_interval(nb, ray, t0, t1);
    case BoundMode::Both:
        return read_slab ? both_interval(nb, ray, t0, t1) : box_interval(nb, ray, t0, t1);
    }
    return false;
}

namespace
{

// TFDM computeAABBs: cells inside the triangle contribute their min-max
// node at their own level, straddling cells descend to the leaf.
void height_range_walk(const UvTriangle &domain, const TaylorPyramid &pyramid, int level, int64_t i, int64_t j,
                       double &h_min, double &h_max)
{
    double w = pyramid.cell_width(level);
    vec2d c((i + 0.5) * w, (j + 0.5) * w);
    Overlap overlap = classify_square(domain, c, 0.5 * w);
    if (overlap == Overlap::Outside)
        return;
    if (overlap == Overlap::Inside || level == 0) {
        TaylorNode node = pyramid.node(level, i, j);
        h_min = std::min(h_min, node.h_min);
        h_max = std::max(h_max, node.h_max);
        return;
    }
    for (int ch = 0; ch < 4; ++ch)
        height_range_walk(domain, pyramid, level - 1, 2 * i + (ch & 1), 2 * j + (ch >> 1), h_min, h_max);
}

} // namespace

void triangle_height_range(const BaseTriangle &tri, const TaylorPyramid &pyramid, double &h_min, double &h_max)
{
    UvTriangle domain(tri.t0, tri.t1, tri.t2);
    int level;
    int64_t i0, j0;
    footprint_roots(domain, pyramid.n_leaf, level, i0, j0);
    h_min = INFINITY;
    h_max = -INFINITY;
    for (int ch = 0; ch < 4; ++ch)
        height_range_walk(domain, pyramid, level, i0 + (ch & 1), j0 + (ch >> 1), h_min, h_max);
    ASSERT(h_min <= h_max, "a triangle's footprint holds no cell");
}

Aabb3 triangle_aabb(const BaseTriangle &tri, double h_min, double h_max)
{
    // Three parallelograms cover the triangle: at vertex k, barycentric
    // weights of the two other vertices in [0, 1/2] each, written as
    // centre + (1/4)(t_k+1 - t_k) εu + (1/4)(t_k+2 - t_k) εv.
    const vec2d t[3] = {tri.t0, tri.t1, tri.t2};
    AffineForm h(0.5 * (h_min + h_max), 0.0, 0.0, 0.5 * (h_max - h_min));
    Aabb3 box{vec3d::Constant(INFINITY), vec3d::Constant(-INFINITY)};
    for (int k = 0; k < 3; ++k) {
        vec2d c = 0.5 * t[k] + 0.25 * t[(k + 1) % 3] + 0.25 * t[(k + 2) % 3];
        vec2d du = 0.25 * (t[(k + 1) % 3] - t[k]), dv = 0.25 * (t[(k + 2) % 3] - t[k]);
        // (s, t) = c + du εu + dv εv; P and N are affine in (s, t).
        AffineVec3 P(tri.P(c[0], c[1]), du[0] * tri.e1 + du[1] * tri.e2, dv[0] * tri.e1 + dv[1] * tri.e2);
        AffineVec3 N(tri.M(c[0], c[1]), du[0] * tri.Mu + du[1] * tri.Mv, dv[0] * tri.Mu + dv[1] * tri.Mv);
        N.normalize();
        box.expand(hull(P + h * N));
    }
    return box;
}

} // namespace dmap
