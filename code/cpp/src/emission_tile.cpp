#include "emission_tile.h"
#include "ks/assertion.h"
#include "ks/shader_field.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

namespace
{

int64_t wrap_index(int64_t i, int m)
{
    int64_t r = i % m;
    return r < 0 ? r + m : r;
}

int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

bool is_power_of_two(int n) { return n >= 1 && (n & (n - 1)) == 0; }

} // namespace

EmissionTile::EmissionTile(const TextureGrid &texels) : n_leaf(texels.W), i0(0), j0(0), repeat(true)
{
    build_levels(texels);
}

EmissionTile::EmissionTile(const TextureGrid &texels, int n_leaf, int64_t i0, int64_t j0)
    : n_leaf(n_leaf), i0(i0), j0(j0), repeat(false)
{
    build_levels(texels);
    ASSERT(i0 % n == 0 && j0 % n == 0, "emission box origin (%lld, %lld) is not aligned to its size %d", (long long)i0,
           (long long)j0, n);
}

void EmissionTile::build_levels(const TextureGrid &texels)
{
    n = texels.W;
    ASSERT(texels.W == texels.H, "emission grid needs a square texel grid, got %d x %d", texels.W, texels.H);
    ASSERT(is_power_of_two(n), "emission grid needs a 2^K x 2^K texel grid, got %d", n);
    n_levels = 1;
    while ((1 << n_levels) <= n)
        ++n_levels;

    Level leaf;
    leaf.m = n;
    leaf.mean = texels.values;
    leaf.max = texels.values;
    levels.push_back(std::move(leaf));

    while (levels.back().m > 1) {
        const Level &c = levels.back();
        Level p;
        p.m = c.m / 2;
        p.mean.resize((size_t)p.m * p.m);
        p.max.resize((size_t)p.m * p.m);
        for (int J = 0; J < p.m; ++J) {
            for (int I = 0; I < p.m; ++I) {
                double sum = 0.0, mx = -INFINITY;
                for (int a = 0; a < 2; ++a) {
                    for (int b = 0; b < 2; ++b) {
                        size_t k = (size_t)(2 * J + a) * c.m + (2 * I + b);
                        sum += c.mean[k];
                        mx = std::max(mx, c.max[k]);
                    }
                }
                p.mean[(size_t)J * p.m + I] = 0.25 * sum;
                p.max[(size_t)J * p.m + I] = mx;
            }
        }
        levels.push_back(std::move(p));
    }
}

double EmissionTile::cell_area(int level) const
{
    double w = std::ldexp(1.0, level) / n_leaf;
    return w * w;
}

bool EmissionTile::local_cell(int level, int64_t i, int64_t j, size_t &index) const
{
    const Level &lvl = levels[level];
    // The origin is a multiple of n, hence of 2^level, so cells at this
    // level are inside the grid or disjoint from it.
    int64_t li = i - (i0 >> level), lj = j - (j0 >> level);
    if (li < 0 || li >= lvl.m || lj < 0 || lj >= lvl.m)
        return false;
    index = (size_t)lj * lvl.m + li;
    return true;
}

double EmissionTile::mean(int level, int64_t i, int64_t j) const
{
    if (repeat) {
        if (level >= n_levels)
            return levels.back().mean[0];
        const Level &lvl = levels[level];
        return lvl.mean[(size_t)wrap_index(j, lvl.m) * lvl.m + wrap_index(i, lvl.m)];
    }
    if (level >= n_levels) {
        // The grid lies inside one cell of this level: its mass spread
        // over the cell's area, zero elsewhere.
        int64_t side = (int64_t)1 << level;
        if (i != floor_div(i0, side) || j != floor_div(j0, side))
            return 0.0;
        double grid_over_cell = (double)n / (double)side;
        return levels.back().mean[0] * grid_over_cell * grid_over_cell;
    }
    size_t index;
    return local_cell(level, i, j, index) ? levels[level].mean[index] : 0.0;
}

double EmissionTile::max(int level, int64_t i, int64_t j) const
{
    if (repeat) {
        if (level >= n_levels)
            return levels.back().max[0];
        const Level &lvl = levels[level];
        return lvl.max[(size_t)wrap_index(j, lvl.m) * lvl.m + wrap_index(i, lvl.m)];
    }
    if (level >= n_levels) {
        int64_t side = (int64_t)1 << level;
        if (i != floor_div(i0, side) || j != floor_div(j0, side))
            return 0.0;
        return levels.back().max[0];
    }
    size_t index;
    return local_cell(level, i, j, index) ? levels[level].max[index] : 0.0;
}

TextureGrid emission_from_field(const ks::ShaderField<ks::color3> &field, int n, int supersample)
{
    return emission_from_field(field, n, 0, 0, n, [](const ks::vec2d &t) { return t; }, supersample);
}

TextureGrid emission_from_field(const ks::ShaderField<ks::color3> &field, int n_leaf, int64_t i0, int64_t j0, int n,
                                const std::function<ks::vec2d(const ks::vec2d &)> &tile_to_uv, int supersample)
{
    TextureGrid tex;
    tex.W = tex.H = n;
    tex.values.resize((size_t)n * n);
    ks::mat2 duvdxy = ks::mat2::Zero();
    double w = 1.0 / n_leaf;
    for (int b = 0; b < n; ++b) {
        for (int a = 0; a < n; ++a) {
            double sum = 0.0;
            for (int y = 0; y < supersample; ++y) {
                for (int x = 0; x < supersample; ++x) {
                    ks::vec2d t((i0 + a + (x + 0.5) / supersample) * w, (j0 + b + (y + 0.5) / supersample) * w);
                    ks::vec2d uv = tile_to_uv(t);
                    sum += ks::luminance(field(uv.cast<float>(), duvdxy));
                }
            }
            tex.values[(size_t)b * n + a] = sum / ((double)supersample * supersample);
        }
    }
    return tex;
}

} // namespace dmap
