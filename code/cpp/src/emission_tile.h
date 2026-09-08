#pragma once
#include "ks/maths.h"
#include "texture_grid.h"
#include <cstdint>
#include <functional>
#include <vector>

// Emission statistics over the cells of a displacement's leaf lattice, for
// the descent samplers (master plan §5A: "a sum pyramid of E is exact at
// texel granularity"): per cell, the mean and the max of the emission over
// the cell, unclipped. The mean folds by averaging and the max by max, both
// exactly. A triangle's own shape enters at query time through its
// boundary cache (descent_sampler.h): interior cells read these numbers
// directly, and a zero max certifies zero mass, which keeps certified-zero
// pruning exact.
//
// Two layouts over the same lattice (n_leaf leaf cells per unit tile):
// - the texture's own tile, repeating: indices wrap, levels above the root
//   are unions of whole tiles and read the root (the A-MVP experiments,
//   whose emission is a texel grid over the tile);
// - an object's box: an aligned 2^K x 2^K block of leaf cells at origin
//   (i0, j0), a multiple of 2^K, holding the object's material emission
//   sampled in its own texture coordinates. Cells outside hold zero, and
//   the one cell per higher level that contains the block carries its
//   whole mass ("Plan — Path tracer with displaced surfaces", T2: emission
//   is a material property, so its statistics belong to the object, at the
//   displacement's leaf resolution).
//
// The max channel bounds a piecewise-constant emission exactly. For a
// textured emission it is the supersampled texel mean, an estimate; making
// it a bound is T8's business, where the light is wired.

namespace ks
{
template <typename T>
struct ShaderField;
}

namespace dmap
{

struct EmissionTile
{
    struct Level
    {
        int m = 0;
        std::vector<double> mean, max;
    };
    int n_leaf = 0;   // leaf cells per unit tile, 2^L
    int n = 0;        // cells per side of the grid, 2^K (K = L for a tile)
    int n_levels = 0; // K + 1
    int64_t i0 = 0, j0 = 0;
    bool repeat = true;
    std::vector<Level> levels;

    // Bytes of the mean and max channels over every level.
    size_t memory_bytes() const
    {
        size_t bytes = 0;
        for (const Level &level : levels)
            bytes += (level.mean.size() + level.max.size()) * sizeof(double);
        return bytes;
    }

    // The texture's own tile: texels n x n, n a power of two.
    explicit EmissionTile(const TextureGrid &texels);
    // An object's box at origin (i0, j0) on a lattice of n_leaf cells per
    // tile; texels n x n with n a power of two and i0, j0 multiples of n.
    EmissionTile(const TextureGrid &texels, int n_leaf, int64_t i0, int64_t j0);

    double texel(int64_t i, int64_t j) const { return mean(0, i, j); }
    double cell_area(int level) const;
    double mean(int level, int64_t i, int64_t j) const;
    double max(int level, int64_t i, int64_t j) const;
    // Integral over the unclipped cell, in parameter units.
    double sum(int level, int64_t i, int64_t j) const { return mean(level, i, j) * cell_area(level); }

  private:
    void build_levels(const TextureGrid &texels);
    // The grid-local cell index at a level, or false when the cell lies
    // outside the grid (box layout only).
    bool local_cell(int level, int64_t i, int64_t j, size_t &index) const;
};

// Scalar emission per texel from a ks colour field by supersampled
// luminance: n x n texels over the unit tile, supersample^2 points each.
TextureGrid emission_from_field(const ks::ShaderField<ks::color3> &field, int n, int supersample);

// The same over an object's box: texels (i0 + a, j0 + b), a, b < n, of a
// lattice with n_leaf cells per tile, each point mapped from tile
// coordinates to the field's texture coordinates by tile_to_uv.
TextureGrid emission_from_field(const ks::ShaderField<ks::color3> &field, int n_leaf, int64_t i0, int64_t j0, int n,
                                const std::function<ks::vec2d(const ks::vec2d &)> &tile_to_uv, int supersample);

} // namespace dmap
