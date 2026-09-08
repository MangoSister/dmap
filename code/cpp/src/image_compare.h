#pragma once
#include "ks/maths.h"
#include <cstdint>
#include <filesystem>
#include <vector>
namespace fs = std::filesystem;

// Pixel statistics between two linear RGB images, for the verification
// tasks: exact differences for a regression gate, luminance statistics
// for a convergence check.

namespace dmap
{

struct RgbImage
{
    int width = 0, height = 0;
    std::vector<ks::color3> pixels;

    // Over the pixels whose mask entry is nonzero when a mask is given.
    double mean_luminance(const std::vector<uint8_t> *mask = nullptr) const;
};

RgbImage load_rgb_exr(const fs::path &path);

struct ImageDifference
{
    int64_t differing = 0; // pixels whose channels differ at all
    int64_t nonfinite = 0; // pixels with a NaN or an infinity in either image
    float worst = 0.0f;    // largest channel difference
    double mean_lum_a = 0.0, mean_lum_b = 0.0;
    double mean_rel_diff = 0.0; // |mean_a - mean_b| / mean_b
    // Relative MSE on luminance against b, with an epsilon tied to b's mean
    // so dark pixels do not dominate (render_displaced_emitter.cpp).
    double rel_mse = 0.0;
};

// With a mask, only the pixels whose mask entry is nonzero count.
ImageDifference compare_images(const RgbImage &a, const RgbImage &b, const std::vector<uint8_t> *mask = nullptr);

} // namespace dmap
