#pragma once
#include <filesystem>
#include <vector>
namespace fs = std::filesystem;

// Displacement textures as double grids in [0, 1], mirroring the numpy
// reference (code/python/poc/dmapref/displacement.py): integer images are
// divided by their type maximum, color channels are averaged, and
// downsample_box crops to a multiple of n and box-filters to n x n.

namespace dmap
{

struct TextureGrid
{
    int W = 0, H = 0;
    std::vector<double> values; // row-major, values[j*W + i]
};

TextureGrid load_height_texture(const fs::path &path);
TextureGrid downsample_box(const TextureGrid &tex, int n);

// Procedural test maps (no emission assets exist yet; plan phase S5).
// checkerboard: cells x cells alternating lo/hi over an n x n grid.
TextureGrid checkerboard(int n, int cells, double lo, double hi);
// gradient: lo at (0, 0) to hi at (1, 1), linear in u + v.
TextureGrid gradient_map(int n, double lo, double hi);
// gaussian spot: peak * exp(-((u-cx)^2+(v-cy)^2)/(2 sigma^2)) + floor.
TextureGrid gaussian_spot(int n, double cx, double cy, double sigma, double peak, double floor_value);

} // namespace dmap
