#include "displaced_surface.h"
#include "ks/assertion.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

void HeightGrid::cell(double x, double y, int &i, int &j, double &s, double &t) const
{
    if (repeat) {
        x -= std::floor(x);
        y -= std::floor(y);
    } else {
        x = std::clamp(x, 0.0, 1.0);
        y = std::clamp(y, 0.0, 1.0);
    }
    double fx = x * (W - 1);
    double fy = y * (H - 1);
    i = std::min((int)fx, W - 2);
    j = std::min((int)fy, H - 2);
    s = fx - i;
    t = fy - j;
}

double HeightGrid::h(double x, double y) const
{
    int i, j;
    double s, t;
    cell(x, y, i, j, s, t);
    return scale * ((1.0 - s) * (1.0 - t) * value(j, i) + s * (1.0 - t) * value(j, i + 1) +
                    (1.0 - s) * t * value(j + 1, i) + s * t * value(j + 1, i + 1)) +
           offset;
}

vec2d HeightGrid::grad(double x, double y) const
{
    int i, j;
    double s, t;
    cell(x, y, i, j, s, t);
    double dh_ds = (1.0 - t) * (value(j, i + 1) - value(j, i)) + t * (value(j + 1, i + 1) - value(j + 1, i));
    double dh_dt = (1.0 - s) * (value(j + 1, i) - value(j, i)) + s * (value(j + 1, i + 1) - value(j, i + 1));
    return scale * vec2d(dh_ds * (W - 1), dh_dt * (H - 1));
}

BaseTriangle::BaseTriangle(const vec3d &p0, const vec3d &p1, const vec3d &p2, const vec3d &m0, const vec3d &m1,
                           const vec3d &m2)
    : BaseTriangle(p0, p1, p2, m0, m1, m2, vec2d(0.0, 0.0), vec2d(1.0, 0.0), vec2d(0.0, 1.0))
{}

BaseTriangle::BaseTriangle(const vec3d &p0, const vec3d &p1, const vec3d &p2, const vec3d &m0, const vec3d &m1,
                           const vec3d &m2, const vec2d &t0, const vec2d &t1, const vec2d &t2)
    : p0(p0), p1(p1), p2(p2), t0(t0), t1(t1), t2(t2)
{
    // Barycentric edges and normal differences.
    vec3d eb1 = p1 - p0, eb2 = p2 - p0;
    vec3d Mb1 = m1 - m0, Mb2 = m2 - m0;

    // (u, v) = J ((s, t) - t0) with T = [t1 - t0 | t2 - t0], J = T^{-1}.
    Eigen::Matrix2d T;
    T.col(0) = t1 - t0;
    T.col(1) = t2 - t0;
    double det = T.determinant();
    ASSERT(std::abs(det) > 1e-30, "base triangle is degenerate in the texture plane (det %.3e)", det);
    J = T.inverse();

    // P(s, t) = p0 + [eb1 eb2] J ((s, t) - t0): parameter origin and
    // parameter derivatives. The identity chart gives q0 = p0, e1 = eb1,
    // e2 = eb2 exactly (J is the identity, t0 is zero).
    Eigen::Matrix<double, 3, 2> E, D;
    E.col(0) = eb1;
    E.col(1) = eb2;
    D.col(0) = Mb1;
    D.col(1) = Mb2;
    Eigen::Matrix<double, 3, 2> EJ = E * J, DJ = D * J;
    vec2d c = -J * t0;
    q0 = p0 + E * c;
    e1 = EJ.col(0);
    e2 = EJ.col(1);
    this->m0 = m0 + D * c;
    Mu = DJ.col(0);
    Mv = DJ.col(1);

    G0(0, 0) = e1.dot(e1);
    G0(0, 1) = e1.dot(e2);
    G0(1, 0) = e2.dot(e1);
    G0(1, 1) = e2.dot(e2);
}

double BaseTriangle::param_area() const
{
    vec2d a = t1 - t0, b = t2 - t0;
    return 0.5 * std::abs(a[0] * b[1] - a[1] * b[0]);
}

NormalFrame normal_frame_at(const BaseTriangle &tri, double s, double t)
{
    vec3d M = tri.M(s, t);
    double Mlen = M.norm();
    NormalFrame f;
    f.N = M / Mlen;
    f.Nu = (tri.Mu - f.N * f.N.dot(tri.Mu)) / Mlen;
    f.Nv = (tri.Mv - f.N * f.N.dot(tri.Mv)) / Mlen;
    return f;
}

PointwiseFields pointwise_fields(const BaseTriangle &tri, const HeightGrid &field, double s, double t)
{
    vec2d gh = field.grad(s, t);
    return pointwise_fields(tri, s, t, field.h(s, t), gh[0], gh[1]);
}

PointwiseFields pointwise_fields(const BaseTriangle &tri, double s, double t, double h_in, double hu_in, double hv_in)
{
    PointwiseFields out;
    out.h = h_in;
    out.hu = hu_in;
    out.hv = hv_in;

    NormalFrame f = normal_frame_at(tri, s, t);
    const vec3d &e1 = tri.e1;
    const vec3d &e2 = tri.e2;

    // Base forms (metric note S6): B~ = -[e . N_derivative], symmetrized.
    double bt00 = -f.Nu.dot(e1), bt01 = -f.Nv.dot(e1);
    double bt10 = -f.Nu.dot(e2), bt11 = -f.Nv.dot(e2);
    double B00 = bt00, B01 = 0.5 * (bt01 + bt10), B11 = bt11;
    double C00 = f.Nu.dot(f.Nu), C01 = f.Nu.dot(f.Nv), C11 = f.Nv.dot(f.Nv);
    double a0 = f.N.dot(e1), a1 = f.N.dot(e2);

    double h = out.h, hu = out.hu, hv = out.hv;
    out.G00 = tri.G0(0, 0) - 2.0 * h * B00 + h * h * C00 + hu * hu + 2.0 * hu * a0;
    out.G01 = tri.G0(0, 1) - 2.0 * h * B01 + h * h * C01 + hu * hv + hu * a1 + hv * a0;
    out.G11 = tri.G0(1, 1) - 2.0 * h * B11 + h * h * C11 + hv * hv + 2.0 * hv * a1;

    out.det = out.G00 * out.G11 - out.G01 * out.G01;
    out.sqrt_det = std::sqrt(std::max(out.det, 0.0));

    // Eigenvalues of G0^{-1} G: T = tr(G0^{-1} G), D = det G / det G0.
    Eigen::Matrix2d A = tri.G0.inverse();
    double T = A(0, 0) * out.G00 + 2.0 * A(0, 1) * out.G01 + A(1, 1) * out.G11;
    double det_G0 = tri.G0(0, 0) * tri.G0(1, 1) - tri.G0(0, 1) * tri.G0(1, 0);
    double D = out.det / det_G0;
    double disc = std::max(T * T - 4.0 * D, 0.0);
    double sd = std::sqrt(disc);
    out.lam_max = 0.5 * (T + sd);
    out.lam_min = 0.5 * (T - sd);

    vec3d Su = e1 + hu * f.N + h * f.Nu;
    vec3d Sv = e2 + hv * f.N + h * f.Nv;
    out.n = Su.cross(Sv);
    return out;
}

vec3d winding_normal(const BaseTriangle &tri) { return (tri.p1 - tri.p0).cross(tri.p2 - tri.p0); }

bool chart_mirrored(const BaseTriangle &tri) { return tri.e1.cross(tri.e2).dot(winding_normal(tri)) < 0.0; }

vec3d displaced_position(const BaseTriangle &tri, const HeightGrid &field, double s, double t)
{
    NormalFrame f = normal_frame_at(tri, s, t);
    return tri.P(s, t) + field.h(s, t) * f.N;
}

double mean_edge(const BaseTriangle &tri)
{
    return ((tri.p1 - tri.p0).norm() + (tri.p2 - tri.p0).norm() + (tri.p2 - tri.p1).norm()) / 3.0;
}

} // namespace dmap
