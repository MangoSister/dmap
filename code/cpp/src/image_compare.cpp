#include "image_compare.h"
#include "ks/assertion.h"
#include "ks/image_util.h"
#include <algorithm>
#include <cmath>

using namespace ks;

namespace dmap
{

double RgbImage::mean_luminance(const std::vector<uint8_t> *mask) const
{
    double sum = 0.0;
    size_t count = 0;
    for (size_t k = 0; k < pixels.size(); ++k) {
        if (mask && !(*mask)[k])
            continue;
        sum += luminance(pixels[k]);
        ++count;
    }
    return count == 0 ? 0.0 : sum / count;
}

RgbImage load_rgb_exr(const fs::path &path)
{
    RgbImage img;
    std::unique_ptr<color3[]> data = load_from_exr<3>(path, img.width, img.height);
    ASSERT(data, "cannot load [%s]", path.string().c_str());
    img.pixels.assign(data.get(), data.get() + (size_t)img.width * img.height);
    return img;
}

ImageDifference compare_images(const RgbImage &a, const RgbImage &b, const std::vector<uint8_t> *mask)
{
    ASSERT(a.width == b.width && a.height == b.height, "images differ in size: %dx%d and %dx%d", a.width, a.height,
           b.width, b.height);
    ImageDifference d;
    ASSERT(!mask || mask->size() == a.pixels.size(), "the mask must have one entry per pixel");
    d.mean_lum_a = a.mean_luminance(mask);
    d.mean_lum_b = b.mean_luminance(mask);
    d.mean_rel_diff = d.mean_lum_b > 0.0 ? std::abs(d.mean_lum_a - d.mean_lum_b) / d.mean_lum_b : 0.0;
    double eps = 1e-2 * d.mean_lum_b;
    eps *= eps;
    double sum = 0.0;
    size_t count = 0;
    for (size_t k = 0; k < a.pixels.size(); ++k) {
        if (mask && !(*mask)[k])
            continue;
        ++count;
        const color3 &pa = a.pixels[k], &pb = b.pixels[k];
        if (!pa.allFinite() || !pb.allFinite()) {
            ++d.nonfinite;
            continue;
        }
        float diff = (pa - pb).abs().maxCoeff();
        if (diff > 0.0f) {
            ++d.differing;
            d.worst = std::max(d.worst, diff);
        }
        double la = luminance(pa), lb = luminance(pb);
        sum += (la - lb) * (la - lb) / (lb * lb + eps);
    }
    d.rel_mse = count == 0 ? 0.0 : sum / count;
    return d;
}

} // namespace dmap
