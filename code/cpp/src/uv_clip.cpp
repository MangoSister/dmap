#include "uv_clip.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

using ks::vec2d;

UvTriangle::UvTriangle(const vec2d &t0, const vec2d &t1, const vec2d &t2)
{
    t[0] = t0;
    t[1] = t1;
    t[2] = t2;
    double signed_area = 0.5 * ((t1 - t0)[0] * (t2 - t0)[1] - (t1 - t0)[1] * (t2 - t0)[0]);
    area = std::abs(signed_area);
    double orient = signed_area >= 0.0 ? 1.0 : -1.0; // inward normals for either winding
    for (int k = 0; k < 3; ++k) {
        vec2d e = t[(k + 1) % 3] - t[k];
        n[k] = orient * vec2d(-e[1], e[0]);
        d[k] = n[k].dot(t[k]);
    }
    lo = t0.cwiseMin(t1).cwiseMin(t2);
    hi = t0.cwiseMax(t1).cwiseMax(t2);
}

Overlap classify_square(const UvTriangle &tri, const vec2d &c, double s)
{
    // Separating axis: the square's axes (bounding boxes) ...
    if (c[0] + s <= tri.lo[0] || c[0] - s >= tri.hi[0] || c[1] + s <= tri.lo[1] || c[1] - s >= tri.hi[1])
        return Overlap::Outside;
    // ... and the triangle's edge normals: the square's extent along n[k]
    // is |n_x| s + |n_y| s around n . c.
    bool all_inside = true;
    for (int k = 0; k < 3; ++k) {
        double proj = tri.n[k].dot(c);
        double ext = (std::abs(tri.n[k][0]) + std::abs(tri.n[k][1])) * s;
        if (proj + ext <= tri.d[k])
            return Overlap::Outside;
        if (proj - ext < tri.d[k])
            all_inside = false;
    }
    return all_inside ? Overlap::Inside : Overlap::Straddle;
}

double ClipPolygon::area() const
{
    double a = 0.0;
    for (int k = 0; k < n; ++k) {
        const vec2d &p0 = p[k], &p1 = p[(k + 1) % n];
        a += p0[0] * p1[1] - p1[0] * p0[1];
    }
    return 0.5 * std::abs(a);
}

ClipPolygon clip_square(const UvTriangle &tri, const vec2d &c, double s)
{
    ClipPolygon in;
    in.n = 4;
    in.p[0] = c + vec2d(-s, -s);
    in.p[1] = c + vec2d(s, -s);
    in.p[2] = c + vec2d(s, s);
    in.p[3] = c + vec2d(-s, s);
    for (int k = 0; k < 3; ++k) {
        ClipPolygon out;
        const vec2d &nk = tri.n[k];
        double dk = tri.d[k];
        for (int a = 0; a < in.n; ++a) {
            const vec2d &pa = in.p[a], &pb = in.p[(a + 1) % in.n];
            double fa = nk.dot(pa) - dk, fb = nk.dot(pb) - dk;
            if (fa >= 0.0)
                out.p[out.n++] = pa;
            if ((fa >= 0.0) != (fb >= 0.0)) {
                double t = fa / (fa - fb);
                out.p[out.n++] = pa + t * (pb - pa);
            }
        }
        in = out;
        if (in.n == 0)
            break;
    }
    return in;
}

vec2d sample_polygon(const ClipPolygon &poly, double xi_tri, double xi1, double xi2)
{
    // Fan from vertex 0; cumulative areas select the triangle.
    double areas[8];
    double total = 0.0;
    for (int k = 1; k + 1 < poly.n; ++k) {
        vec2d a = poly.p[k] - poly.p[0], b = poly.p[k + 1] - poly.p[0];
        areas[k] = 0.5 * std::abs(a[0] * b[1] - a[1] * b[0]);
        total += areas[k];
    }
    double target = xi_tri * total, acc = 0.0;
    int chosen = poly.n - 2;
    for (int k = 1; k + 1 < poly.n; ++k) {
        acc += areas[k];
        if (target < acc) {
            chosen = k;
            break;
        }
    }
    // Uniform in a triangle: the square-root warp.
    double r = std::sqrt(xi1);
    double b1 = 1.0 - r, b2 = r * xi2;
    return poly.p[0] + b1 * (poly.p[chosen] - poly.p[0]) + b2 * (poly.p[chosen + 1] - poly.p[0]);
}

double clipped_cell_area(double u0, double v0, double w)
{
    auto T = [](double x) {
        x = std::max(x, 0.0);
        return 0.5 * x * x;
    };
    double d = 1.0 - u0 - v0;
    return T(d) - 2.0 * T(d - w) + T(d - 2.0 * w);
}

} // namespace dmap
