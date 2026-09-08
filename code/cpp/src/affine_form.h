#pragma once
#include "ks/maths.h"

// Affine arithmetic over the two symbols of a cell, εu and εv in [-1, 1],
// with every other error symbol aggregated into one remainder, as GfxExp's
// TFDM does it (code/third-party/GfxExp/tfdm/affine_arithmetic.h,
// AAFloatOn2D), in double and limited to what the node bounds need:
//     f = c + a εu + b εv + ρ,   |ρ| <= k.
// The remainder is what the plan calls the third symbol: heights enter
// through it (mid ± half-range), and products and the normalization drop
// their nonlinear parts into it. Interval extraction is c ± (|a| + |b| + k).
//
// The normalization of an affine vector follows GfxExp: a conservative
// affine form of the squared length, then 1/sqrt by the Min-Range
// approximation over the squared length's interval hull, then the product.
// Keeping the arithmetic identical to GfxExp's (up to double precision) is
// what makes the box test TFDM's test ("Plan — Path tracer with displaced
// surfaces", D2).

namespace dmap
{

struct Interval
{
    double lo = 0.0, hi = 0.0;

    Interval() = default;
    Interval(double lo, double hi) : lo(lo), hi(hi) {}
    static Interval point(double x) { return {x, x}; }

    double width() const { return hi - lo; }
    double magnitude() const { return std::max(std::abs(lo), std::abs(hi)); }
    bool contains(double x) const { return lo <= x && x <= hi; }
};

Interval operator+(const Interval &a, const Interval &b);
Interval operator+(const Interval &a, double b);
Interval operator*(double s, const Interval &a);

struct AffineForm
{
    double c = 0.0;          // central value
    double a = 0.0, b = 0.0; // coefficients of εu, εv
    double k = 0.0;          // remainder bound, k >= 0

    AffineForm() = default;
    AffineForm(double c) : c(c) {}
    AffineForm(double c, double a, double b, double k) : c(c), a(a), b(b), k(k) {}

    Interval interval() const
    {
        double e = std::abs(a) + std::abs(b) + k;
        return {c - e, c + e};
    }

    AffineForm operator-() const { return {-c, -a, -b, k}; }
    AffineForm &operator+=(const AffineForm &r);
    AffineForm &operator-=(const AffineForm &r);
    AffineForm &operator*=(const AffineForm &r);
    AffineForm &operator*=(double s);
};

AffineForm operator+(AffineForm a, const AffineForm &b);
AffineForm operator-(AffineForm a, const AffineForm &b);
AffineForm operator*(AffineForm a, const AffineForm &b);
AffineForm operator*(double s, AffineForm a);
AffineForm operator*(AffineForm a, double s);

// 1/sqrt(f) by the Min-Range approximation over f's interval [lo, hi],
// lo > 0 (GfxExp recSqrt): alpha f + beta ± delta with alpha = -lo^{-3/2}/2
// evaluated at hi, the chord matched at both ends.
AffineForm rsqrt(const AffineForm &f);

struct AffineVec3
{
    AffineForm x, y, z;

    AffineVec3() = default;
    AffineVec3(const AffineForm &x, const AffineForm &y, const AffineForm &z) : x(x), y(y), z(z) {}
    explicit AffineVec3(const ks::vec3d &c, const ks::vec3d &a = ks::vec3d::Zero(),
                        const ks::vec3d &b = ks::vec3d::Zero(), const ks::vec3d &k = ks::vec3d::Zero())
        : x(c[0], a[0], b[0], k[0]), y(c[1], a[1], b[1], k[1]), z(c[2], a[2], b[2], k[2])
    {}

    ks::vec3d centre() const { return {x.c, y.c, z.c}; }
    ks::vec3d coeff_u() const { return {x.a, y.a, z.a}; }
    ks::vec3d coeff_v() const { return {x.b, y.b, z.b}; }
    ks::vec3d remainder() const { return {x.k, y.k, z.k}; }
    Interval interval(int axis) const { return axis == 0 ? x.interval() : axis == 1 ? y.interval() : z.interval(); }

    // GfxExp sqLength: the quadratic terms lie in [0, 2 offset] with
    // offset = |(|a| + |b| + k)|^2 / 2, written as offset ± offset.
    AffineForm sq_length() const;
    // rsqrt(sq_length()) times the vector. When the squared length's
    // interval reaches zero the Min-Range rule has no domain (GfxExp
    // divides by NaN there); a unit vector's components lie in [-1, 1],
    // which is the conservative fallback used then.
    void normalize();

    friend AffineVec3 operator*(const AffineForm &s, const AffineVec3 &v) { return {s * v.x, s * v.y, s * v.z}; }
};

AffineVec3 operator+(const AffineVec3 &a, const AffineVec3 &b);
AffineVec3 operator+(const ks::vec3d &a, const AffineVec3 &b);
// Row dot product; a linear map scales the remainder by |entries|.
AffineForm dot(const ks::vec3d &a, const AffineVec3 &b);
AffineVec3 operator*(const Eigen::Matrix3d &A, const AffineVec3 &v);

} // namespace dmap
