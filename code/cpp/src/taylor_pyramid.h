#pragma once
#include "displaced_surface.h"
#include <cstdint>
#include <string>
#include <vector>

// The Taylor-model bound pyramid, ported from the numpy reference
// (code/python/poc/dmapref/pyramid.py), which implements the note
// "Taylor-model bound pyramid" S2-S5 for the bilinear interpolant.
//
// A pyramid is built over a (2^L + 1)^2 node grid on the unit tile,
// giving 2^L x 2^L leaf cells at level 0 and one root cell at level L.
// Node semantics over a cell with centre (u0, v0) and half-extents (s, s),
// writing du = u - u0, dv = v - v0:
//
//     |h - (h0 + gu*du + gv*dv)| <= r,   |h_u - gu| <= ru,   |h_v - gv| <= rv,
//     h_min <= h <= h_max.
//
// Eight numbers. The first six are the Taylor model: a plane plus a
// remainder, whose slab is O(s^2) thick. The last two are the ordinary
// min-max channel, whose box is O(s) tall but axis-aligned. Neither
// replaces the other, because they answer differently shaped queries: the
// slab is the tighter region in space at fine cells, the box the tighter
// scalar interval at coarse cells, which is where a ray traversal starts
// (master plan S2.2).
//
// Grid convention matches HeightGrid: values[j*N + i] sits at
// (u, v) = (i/(N-1), j/(N-1)), so the j axis of every level array is v and
// the i axis is u. The arithmetic mirrors the reference operation for
// operation so golden comparisons can demand near-bit agreement.
//
// The pyramid is a property of the texture alone and is shared by every
// triangle that uses it. Texel indices wrap into the tile (the texture
// repeats), and a node above the root, a union of whole tiles, is served
// as a min-max node: the plane's slope would not be one plane across
// tiles, and the six numbers can encode a min-max node outright.

namespace dmap
{

enum class PyramidBuild
{
    Fold,   // bottom-up 4-to-1 fold (note S4)
    Direct, // per level, enumerate the texel nodes the cell covers (note S4)
};

// Config vocabulary: "fold" or "direct". Anything else is a config error.
PyramidBuild pyramid_build_from_string(const std::string &name);
const char *pyramid_build_name(PyramidBuild build);

// One pyramid level: m x m cells, arrays indexed [j*m + i].
struct TaylorLevel
{
    int m = 0;
    std::vector<double> h0, gu, gv, r, ru, rv, h_min, h_max;

    explicit TaylorLevel(int m)
        : m(m), h0((size_t)m * m), gu((size_t)m * m), gv((size_t)m * m), r((size_t)m * m), ru((size_t)m * m),
          rv((size_t)m * m), h_min((size_t)m * m), h_max((size_t)m * m)
    {}
};

struct TaylorNode
{
    double h0, gu, gv, r, ru, rv, h_min, h_max;
};

struct TaylorPyramid
{
    int n_leaf = 0;   // leaf cells per side (2^L)
    int n_levels = 0; // L + 1, root included
    PyramidBuild build = PyramidBuild::Fold;
    std::vector<TaylorLevel> levels;

    // Bytes of the eight channels over every level.
    size_t memory_bytes() const
    {
        size_t cells = 0;
        for (const TaylorLevel &level : levels)
            cells += (size_t)level.m * level.m;
        return 8 * cells * sizeof(double);
    }

    // nodes x nodes grid (nodes = 2^L + 1), row-major, scaled by scale and
    // shifted by offset (h = scale * value + offset, as HeightGrid).
    // The two constructions agree exactly on gu, gv, ru, rv, h_min and
    // h_max; Direct gives a smaller r, because the fold compounds the
    // remainders of its children, and moves h0 with it.
    TaylorPyramid(const double *values, int nodes, double scale, PyramidBuild build = PyramidBuild::Fold,
                  double offset = 0.0);
    // The pyramid of a height grid: its nodes, scale and offset.
    TaylorPyramid(const HeightGrid &field, PyramidBuild build = PyramidBuild::Fold);

    double cell_width(int level) const { return std::ldexp(1.0, level) / n_leaf; }
    double half_extent(int level) const { return 0.5 * cell_width(level); }

    // Node (level, i, j) with wrapped texel indices; levels above the root
    // are min-max nodes (see the header comment).
    TaylorNode node(int level, int64_t i, int64_t j) const;
};

// Min-max h recovered from a Taylor level (note S5):
// h in h0 +- (|gu|*s + |gv|*s + r), with s the level's half-extent. This is
// the baseline the stored h_min/h_max channel is measured against, not a
// substitute for it: the recovery adds the plane's excursion across the
// cell to the remainder as if their extremes coincided.
void minmax_from_taylor(const TaylorLevel &level, double s, std::vector<double> &lo, std::vector<double> &hi);

} // namespace dmap
