#include "affine_form.h"
#include "ks/assertion.h"
#include <cmath>

namespace dmap
{

using ks::vec3d;

Interval operator+(const Interval &a, const Interval &b) { return {a.lo + b.lo, a.hi + b.hi}; }
Interval operator+(const Interval &a, double b) { return {a.lo + b, a.hi + b}; }
Interval operator*(double s, const Interval &a)
{
    return s >= 0.0 ? Interval{s * a.lo, s * a.hi} : Interval{s * a.hi, s * a.lo};
}

AffineForm &AffineForm::operator+=(const AffineForm &r)
{
    c += r.c;
    a += r.a;
    b += r.b;
    k += r.k;
    return *this;
}

AffineForm &AffineForm::operator-=(const AffineForm &r)
{
    c -= r.c;
    a -= r.a;
    b -= r.b;
    k += r.k;
    return *this;
}

// (c + a εu + b εv ± k)(c' + a' εu + b' εv ± k'): the linear part exactly,
// the products of deviations bounded by (|a| + |b| + k)(|a'| + |b'| + k').
AffineForm &AffineForm::operator*=(const AffineForm &r)
{
    double u = std::abs(a) + std::abs(b) + k;
    double v = std::abs(r.a) + std::abs(r.b) + r.k;
    double c0 = c;
    c = c0 * r.c;
    a = c0 * r.a + r.c * a;
    b = c0 * r.b + r.c * b;
    k = std::abs(r.c) * k + std::abs(c0) * r.k + u * v;
    return *this;
}

AffineForm &AffineForm::operator*=(double s)
{
    c *= s;
    a *= s;
    b *= s;
    k *= std::abs(s);
    return *this;
}

AffineForm operator+(AffineForm a, const AffineForm &b) { return a += b; }
AffineForm operator-(AffineForm a, const AffineForm &b) { return a -= b; }
AffineForm operator*(AffineForm a, const AffineForm &b) { return a *= b; }
AffineForm operator*(double s, AffineForm a) { return a *= s; }
AffineForm operator*(AffineForm a, double s) { return a *= s; }

AffineForm rsqrt(const AffineForm &f)
{
    Interval range = f.interval();
    double lo = range.lo, hi = range.hi;
    ASSERT(lo > 0.0, "rsqrt of an affine form whose interval [%g, %g] reaches zero", lo, hi);
    auto g = [](double x) { return 1.0 / std::sqrt(x); };
    double alpha = -0.5 * g(hi) * g(hi) * g(hi);
    double beta = 0.5 * (g(lo) + g(hi) - alpha * (lo + hi));
    double delta = 0.5 * std::abs(g(lo) - g(hi) - alpha * (lo - hi));
    // alpha f + beta, the remainder scaled by |alpha| plus delta.
    AffineForm out(alpha * f.c + beta, alpha * f.a, alpha * f.b, std::abs(alpha) * f.k + delta);
    return out;
}

AffineForm AffineVec3::sq_length() const
{
    vec3d xc = centre(), xu = coeff_u(), xv = coeff_v(), xk = remainder();
    vec3d dev = xu.cwiseAbs() + xv.cwiseAbs() + xk;
    double offset = 0.5 * dev.squaredNorm();
    return AffineForm(xc.squaredNorm() + offset, 2.0 * xc.dot(xu), 2.0 * xc.dot(xv),
                      2.0 * std::abs(xc.dot(xk)) + offset);
}

void AffineVec3::normalize()
{
    AffineForm l = sq_length();
    if (l.interval().lo <= 0.0) {
        x = y = z = AffineForm(0.0, 0.0, 0.0, 1.0);
        return;
    }
    *this = rsqrt(l) * *this;
}

AffineVec3 operator+(const AffineVec3 &a, const AffineVec3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

AffineVec3 operator+(const vec3d &a, const AffineVec3 &b) { return {a[0] + b.x, a[1] + b.y, a[2] + b.z}; }

AffineForm dot(const vec3d &a, const AffineVec3 &b) { return a[0] * b.x + a[1] * b.y + a[2] * b.z; }

AffineVec3 operator*(const Eigen::Matrix3d &A, const AffineVec3 &v)
{
    return {dot(A.row(0).transpose(), v), dot(A.row(1).transpose(), v), dot(A.row(2).transpose(), v)};
}

} // namespace dmap
