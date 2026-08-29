#include "texture_grid.h"
#include "ks/assertion.h"
#include <cmath>
#include <stb_image.h>

namespace dmap
{

TextureGrid load_height_texture(const fs::path &path)
{
    std::string path_str = path.string();
    int W, H, channels;
    TextureGrid tex;

    if (stbi_is_16_bit(path_str.c_str())) {
        stbi_us *data = stbi_load_16(path_str.c_str(), &W, &H, &channels, 0);
        ASSERT(data, "Cannot load texture [%s].", path_str.c_str());
        tex.W = W;
        tex.H = H;
        tex.values.resize((size_t)W * H);
        int nc = std::min(channels, 3);
        for (size_t p = 0; p < (size_t)W * H; ++p) {
            double sum = 0.0;
            for (int c = 0; c < nc; ++c)
                sum += data[p * channels + c];
            tex.values[p] = sum / nc / 65535.0;
        }
        stbi_image_free(data);
    } else {
        stbi_uc *data = stbi_load(path_str.c_str(), &W, &H, &channels, 0);
        ASSERT(data, "Cannot load texture [%s].", path_str.c_str());
        tex.W = W;
        tex.H = H;
        tex.values.resize((size_t)W * H);
        int nc = std::min(channels, 3);
        for (size_t p = 0; p < (size_t)W * H; ++p) {
            double sum = 0.0;
            for (int c = 0; c < nc; ++c)
                sum += data[p * channels + c];
            tex.values[p] = sum / nc / 255.0;
        }
        stbi_image_free(data);
    }
    return tex;
}

TextureGrid checkerboard(int n, int cells, double lo, double hi)
{
    TextureGrid tex;
    tex.W = tex.H = n;
    tex.values.resize((size_t)n * n);
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            int ci = i * cells / n, cj = j * cells / n;
            tex.values[(size_t)j * n + i] = ((ci + cj) % 2 == 0) ? lo : hi;
        }
    }
    return tex;
}

TextureGrid gradient_map(int n, double lo, double hi)
{
    TextureGrid tex;
    tex.W = tex.H = n;
    tex.values.resize((size_t)n * n);
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            double t = 0.5 * ((i + 0.5) / n + (j + 0.5) / n);
            tex.values[(size_t)j * n + i] = lo + (hi - lo) * t;
        }
    }
    return tex;
}

TextureGrid gaussian_spot(int n, double cx, double cy, double sigma, double peak, double floor_value)
{
    TextureGrid tex;
    tex.W = tex.H = n;
    tex.values.resize((size_t)n * n);
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            double du = (i + 0.5) / n - cx, dv = (j + 0.5) / n - cy;
            tex.values[(size_t)j * n + i] = floor_value + peak * std::exp(-(du * du + dv * dv) / (2.0 * sigma * sigma));
        }
    }
    return tex;
}

TextureGrid downsample_box(const TextureGrid &tex, int n)
{
    int bh = tex.H / n, bw = tex.W / n;
    ASSERT(bh >= 1 && bw >= 1, "Texture %dx%d too small for %d nodes.", tex.W, tex.H, n);
    TextureGrid out;
    out.W = out.H = n;
    out.values.resize((size_t)n * n);
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            double sum = 0.0;
            for (int y = 0; y < bh; ++y)
                for (int x = 0; x < bw; ++x)
                    sum += tex.values[(size_t)(j * bh + y) * tex.W + (i * bw + x)];
            out.values[(size_t)j * n + i] = sum / (bh * bw);
        }
    }
    return out;
}

} // namespace dmap
