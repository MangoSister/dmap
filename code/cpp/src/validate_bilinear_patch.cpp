// Phase S5 validation ("Plan — A-MVP sampling implementation"), four checks:
// (1) constant emission on a rectangle: the area pdf is exactly 1/area;
// (2) checkerboard emission on a tilted rectangle: per-texel sample
//     histogram matches the table (TV distance), and the Monte Carlo
//     estimate of integral E dA through the pdf matches the exact texel sum;
// (3) non-planar patch with a gaussian-spot emission: integral E dA against
//     per-texel quadrature of the Jacobian (exercises the varying-Jacobian
//     path), plus sample-side vs query-side pdf consistency;
// (4) the corner-weight sampler (no image): E[1/pdf] recovers the patch
//     area (validates the sample_bilinear port).
// Estimates must sit within 4 standard errors (see line_sampling.h on SE).

#include "bilinear_patch.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "ks/rng.h"
#include "texture_grid.h"
#include <cmath>
#include <cstdlib>
#include <vector>

using namespace ks;

namespace
{

dmap::BilinearPatchMesh make_patch_mesh(const vec3d &p00, const vec3d &p10, const vec3d &p01, const vec3d &p11)
{
    dmap::BilinearPatchMesh mesh;
    mesh.p = {p00, p10, p01, p11};
    mesh.indices = {0, 1, 2, 3};
    return mesh;
}

struct MeanSE
{
    double mean, se;
};

template <typename F>
MeanSE monte_carlo(const dmap::BilinearPatch &patch, RNG &rng, int64_t n, F &&value_of_sample)
{
    double sum = 0.0, sum_sq = 0.0;
    for (int64_t k = 0; k < n; ++k) {
        dmap::PatchSample s = patch.sample(vec2d(rng.next(), rng.next()));
        double x = value_of_sample(s);
        sum += x;
        sum_sq += x * x;
    }
    double mean = sum / n;
    double var = std::max(0.0, sum_sq / n - mean * mean) / (n - 1);
    return {mean, std::sqrt(var)};
}

bool check(const char *label, const MeanSE &e, double ref, bool &pass)
{
    // The absolute epsilon covers the zero-variance case: when the table
    // matches the integrand exactly (a perfect importance sampler), every
    // sample returns the same value, se is 0, and only rounding separates
    // est from ref.
    bool ok = std::abs(e.mean - ref) <= 4.0 * e.se + 1e-9 * std::abs(ref) && e.se <= 0.02 * std::abs(ref);
    pass &= ok;
    if (e.se > 0.0) {
        get_default_logger().info("{}: est {:.6f} +- {:.6f}, ref {:.6f} ({:.2f} se) {}", label, e.mean, e.se, ref,
                                  std::abs(e.mean - ref) / e.se, ok ? "ok" : "FAIL");
    } else {
        get_default_logger().info("{}: est {:.6f} (zero variance), ref {:.6f} {}", label, e.mean, ref,
                                  ok ? "ok" : "FAIL");
    }
    return ok;
}

// integral of |dpdu x dpdv| over one texel by an m x m midpoint rule.
double texel_jacobian_integral(const dmap::BilinearPatch &patch, int i, int j, int W, int H, int m)
{
    double sum = 0.0;
    for (int b = 0; b < m; ++b)
        for (int a = 0; a < m; ++a) {
            double u = (i + (a + 0.5) / m) / W;
            double v = (j + (b + 0.5) / m) / H;
            sum += patch.dpdu(u, v).cross(patch.dpdv(u, v)).norm();
        }
    return sum / (m * m) / ((double)W * H);
}

} // namespace

void validate_bilinear_patch(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    int tex_n = args.load_integer("tex_n", 64);
    int64_t n_samples = args.load_integer("n_samples", 2000000);
    double tv_threshold = (double)args.load_float("tv_threshold", 0.05f);
    uint64_t seed = args.load_integer("seed", 2027);

    RNG rng(seed);
    bool pass = true;

    // Tilted planar rectangle: dpdu x dpdv is constant, |.| = area.
    vec3d r00(0.1, -0.2, 0.3), ru(0.8, 0.1, 0.05), rv(-0.06, 0.5, 0.12);
    double rect_area = ru.cross(rv).norm();

    // (1) Constant emission on the rectangle: pdf_area == 1/area.
    {
        dmap::BilinearPatchMesh mesh = make_patch_mesh(r00, r00 + ru, r00 + rv, r00 + ru + rv);
        mesh.set_emission(dmap::checkerboard(tex_n, 1, 1.0, 1.0)); // constant
        dmap::BilinearPatch patch(mesh, 0);
        double worst = 0.0;
        for (int k = 0; k < 1000; ++k) {
            dmap::PatchSample s = patch.sample(vec2d(rng.next(), rng.next()));
            worst = std::max(worst, std::abs(s.pdf_area * rect_area - 1.0));
        }
        bool ok = worst < 1e-5; // float32 table
        pass &= ok;
        get_default_logger().info("(1) constant emission: max |pdf * area - 1| = {:.3e} {}", worst, ok ? "ok" : "FAIL");
    }

    // (2) Checkerboard on the rectangle: histogram vs table, and integral E dA.
    {
        dmap::BilinearPatchMesh mesh = make_patch_mesh(r00, r00 + ru, r00 + rv, r00 + ru + rv);
        mesh.set_emission(dmap::checkerboard(tex_n, 8, 0.2, 1.0));
        dmap::BilinearPatch patch(mesh, 0);

        double e_sum = 0.0;
        for (double e : mesh.emission.values)
            e_sum += e;
        double ref = rect_area * e_sum / ((double)tex_n * tex_n);

        std::vector<int64_t> counts((size_t)tex_n * tex_n, 0);
        double sum = 0.0, sum_sq = 0.0;
        for (int64_t k = 0; k < n_samples; ++k) {
            dmap::PatchSample s = patch.sample(vec2d(rng.next(), rng.next()));
            int i = std::min((int)(s.u * tex_n), tex_n - 1);
            int j = std::min((int)(s.v * tex_n), tex_n - 1);
            ++counts[(size_t)j * tex_n + i];
            double x = mesh.emission_at(s.u, s.v) / s.pdf_area;
            sum += x;
            sum_sq += x * x;
        }
        double mean = sum / n_samples;
        double se = std::sqrt(std::max(0.0, sum_sq / n_samples - mean * mean) / (n_samples - 1));
        check("(2) checkerboard integral E dA", {mean, se}, ref, pass);

        double tv = 0.0;
        for (size_t t = 0; t < counts.size(); ++t)
            tv += std::abs((double)counts[t] / n_samples - mesh.emission.values[t] / e_sum);
        tv *= 0.5;
        bool ok = tv <= tv_threshold;
        pass &= ok;
        get_default_logger().info("(2) checkerboard histogram: TV distance {:.4f} over {} texels "
                                  "(threshold {:.2f}) {}",
                                  tv, counts.size(), tv_threshold, ok ? "ok" : "FAIL");
    }

    // (3) Non-planar patch + gaussian spot: varying Jacobian, and pdf
    // sample/query consistency.
    {
        dmap::BilinearPatchMesh mesh = make_patch_mesh(r00, r00 + ru, r00 + rv, r00 + ru + rv + vec3d(0.0, 0.15, 0.4));
        mesh.set_emission(dmap::gaussian_spot(tex_n, 0.7, 0.3, 0.15, 4.0, 0.1));
        dmap::BilinearPatch patch(mesh, 0);

        double ref = 0.0;
        for (int j = 0; j < tex_n; ++j)
            for (int i = 0; i < tex_n; ++i)
                ref +=
                    mesh.emission.values[(size_t)j * tex_n + i] * texel_jacobian_integral(patch, i, j, tex_n, tex_n, 4);

        int64_t pdf_mismatch = 0;
        double sum = 0.0, sum_sq = 0.0;
        for (int64_t k = 0; k < n_samples; ++k) {
            dmap::PatchSample s = patch.sample(vec2d(rng.next(), rng.next()));
            double x = mesh.emission_at(s.u, s.v) / s.pdf_area;
            sum += x;
            sum_sq += x * x;
            double q = patch.pdf_area(s.u, s.v);
            if (std::abs(q - s.pdf_area) > 1e-5 * s.pdf_area)
                ++pdf_mismatch;
        }
        double mean = sum / n_samples;
        double se = std::sqrt(std::max(0.0, sum_sq / n_samples - mean * mean) / (n_samples - 1));
        check("(3) gaussian spot on non-planar patch, integral E dA", {mean, se}, ref, pass);

        // Boundary texel landings can re-quantize to the neighbor texel;
        // anything beyond a vanishing fraction is a real inconsistency.
        bool ok = (double)pdf_mismatch / n_samples < 1e-4;
        pass &= ok;
        get_default_logger().info("(3) pdf sample/query consistency: {} mismatches in {} samples {}", pdf_mismatch,
                                  n_samples, ok ? "ok" : "FAIL");
    }

    // (4) Corner-weight sampler (no image): E[1/pdf] = patch area.
    {
        dmap::BilinearPatchMesh mesh = make_patch_mesh(r00, r00 + ru, r00 + rv, r00 + ru + rv + vec3d(0.0, 0.15, 0.4));
        dmap::BilinearPatch patch(mesh, 0);
        double ref = 0.0;
        for (int j = 0; j < tex_n; ++j)
            for (int i = 0; i < tex_n; ++i)
                ref += texel_jacobian_integral(patch, i, j, tex_n, tex_n, 4);
        MeanSE e = monte_carlo(patch, rng, n_samples, [](const dmap::PatchSample &s) { return 1.0 / s.pdf_area; });
        check("(4) corner-weight sampler, E[1/pdf] = area", e, ref, pass);
    }

    get_default_logger().info("VERDICT: {} — bilinear patch sampling (pbrt-v4 port): exact area pdf via the "
                              "pointwise Jacobian under image and corner-weight distributions",
                              pass ? "PASS" : "FAIL");
    if (!pass)
        std::exit(1);
}
