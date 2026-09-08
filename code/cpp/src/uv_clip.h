#pragma once
#include "ks/maths.h"

// A base triangle's domain in the texture plane, and the three questions a
// pyramid cell asks of it. Only the third needs a polygon
// ("Plan — Path tracer with displaced surfaces", D5):
//   - does the cell overlap the domain at all, and is it fully inside
//     (a separating-axis test on the square and the triangle, TFDM's
//     texel discard, no polygon);
//   - the exact area of cell ∩ domain (a square clipped by three
//     half-planes, at most seven vertices), for the leaf density and for
//     the per-face boundary cache built once at construction;
//   - a uniform point inside cell ∩ domain.

namespace dmap
{

struct UvTriangle
{
    ks::vec2d t[3];
    ks::vec2d n[3]; // inward edge normals, edge k from t[k] to t[k+1]
    double d[3];    // n[k] . t[k]; a point p is inside edge k iff n[k].p >= d[k]
    ks::vec2d lo, hi;
    double area = 0.0;

    UvTriangle() = default;
    UvTriangle(const ks::vec2d &t0, const ks::vec2d &t1, const ks::vec2d &t2);

    bool inside(const ks::vec2d &p) const
    {
        for (int k = 0; k < 3; ++k)
            if (n[k].dot(p) < d[k])
                return false;
        return true;
    }
};

enum class Overlap
{
    Outside,
    Inside,
    Straddle,
};

// Square with centre c and half-width s against the triangle. Exact in real
// arithmetic; a measure-zero touch may classify either way, which costs
// nothing since such a cell holds no mass.
Overlap classify_square(const UvTriangle &tri, const ks::vec2d &c, double s);

// The clipped polygon (Sutherland–Hodgman of the square against the three
// half-planes; at most 7 vertices).
struct ClipPolygon
{
    int n = 0;
    ks::vec2d p[8];
    double area() const;
};
ClipPolygon clip_square(const UvTriangle &tri, const ks::vec2d &c, double s);

// Uniform point in the polygon: choose a fan triangle by area, then a
// uniform point in it. xi_tri selects the triangle, (xi1, xi2) the point.
ks::vec2d sample_polygon(const ClipPolygon &poly, double xi_tri, double xi1, double xi2);

// Identity-chart closed form (the domain u + v <= 1): area of the square
// [u0, u0 + w] x [v0, v0 + w] clipped to it. With T(x) = max(x, 0)^2 / 2
// and d = 1 - u0 - v0, the area is T(d) - 2 T(d - w) + T(d - 2w). Kept as
// the special case and as the test of clip_square.
double clipped_cell_area(double u0, double v0, double w);

} // namespace dmap
