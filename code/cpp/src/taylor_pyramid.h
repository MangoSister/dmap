#pragma once
#include <cstdint>
#include <vector>

// The Taylor-model bound pyramid, ported from the numpy reference
// (code/python/poc/dmapref/pyramid.py), which implements the note
// "Taylor-model bound pyramid" S2-S5 for the bilinear interpolant.
//
// A pyramid is built over a (2^L + 1)^2 node grid on the unit square,
// giving 2^L x 2^L leaf cells at level 0 and one root cell at level L.
// Node semantics over a cell with centre (u0, v0) and half-extents (s, s),
// writing du = u - u0, dv = v - v0:
//
//     |h - (h0 + gu*du + gv*dv)| <= r,   |h_u - gu| <= ru,   |h_v - gv| <= rv.
//
// Grid convention matches HeightGrid: values[j*N + i] sits at
// (u, v) = (i/(N-1), j/(N-1)), so the j axis of every level array is v and
// the i axis is u. The arithmetic mirrors the reference operation for
// operation so golden comparisons can demand near-bit agreement.

namespace dmap
{

// One pyramid level: m x m cells, arrays indexed [j*m + i].
struct TaylorLevel
{
    int m = 0;
    std::vector<double> h0, gu, gv, r, ru, rv;

    explicit TaylorLevel(int m)
        : m(m), h0((size_t)m * m), gu((size_t)m * m), gv((size_t)m * m), r((size_t)m * m), ru((size_t)m * m),
          rv((size_t)m * m)
    {}
};

struct TaylorPyramid
{
    int n_leaf = 0;   // leaf cells per side (2^L)
    int n_levels = 0; // L + 1, root included
    std::vector<TaylorLevel> levels;

    // nodes x nodes grid (nodes = 2^L + 1), row-major, scaled by scale.
    TaylorPyramid(const double *values, int nodes, double scale);

    double cell_width(int level) const { return (double)(1 << level) / n_leaf; }
    double half_extent(int level) const { return 0.5 * cell_width(level); }
};

// Min-max h recovered from a Taylor level (note S5):
// h in h0 +- (|gu|*s + |gv|*s + r), with s the level's half-extent.
void minmax_from_taylor(const TaylorLevel &level, double s, std::vector<double> &lo, std::vector<double> &hi);

// Mean and max mipmap over a 2^L x 2^L cell grid (the emission pyramid:
// per-node mean drives descent weights, per-node max drives bounds).
struct MipLevel
{
    int m = 0;
    std::vector<double> mean, max;
};

struct MipPyramid
{
    int n_leaf = 0;
    int n_levels = 0;
    std::vector<MipLevel> levels;

    MipPyramid(const double *cell_values, int n); // n x n cells, n = 2^L
};

} // namespace dmap
