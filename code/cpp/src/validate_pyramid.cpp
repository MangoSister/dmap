// Phase S3 validation ("Plan — A-MVP sampling implementation"), extended on
// 2026-08-30 for the eight-channel node and the two constructions. One
// question: are the eight channels correct and conservative, in both build
// modes? Four checks:
// (1) both C++ pyramids reproduce the numpy reference node-for-node at
//     every level (golden .npy cases from export_golden_pyramid.py);
// (2) conservativeness spot check: dense samples inside cells at every
//     level stay inside the node's slab, its gradient intervals, and its
//     stored height range, in both constructions;
// (3) the two constructions agree where they must — the fold already gets
//     the gradient hulls and the height range exactly, so only h0 and r may
//     move, and the direct r may only shrink; at level 0 the enumeration
//     reduces to the four texel corners, so it must reproduce the
//     closed-form leaf;
// (4) the stored height range always sits inside the interval recovered
//     from the Taylor node, so the dedicated channel never loses to
//     recovery (note S5).
// Plus a sanity check of the mean/max mip pyramid. One verdict.

#include "displaced_surface.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "taylor_pyramid.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <npy/npy.h>
#include <npy/tensor.h>
#include <string>
#include <vector>
namespace fs = std::filesystem;

using namespace ks;

namespace
{

const std::vector<std::string> channel_keys = {"h0", "gu", "gv", "r", "ru", "rv", "h_min", "h_max"};
// The channels the fold already computes exactly, so the two constructions
// must agree on them: indices of gu, gv, ru, rv, h_min, h_max.
const std::vector<int> shared_channels = {1, 2, 4, 5, 6, 7};

npy::tensor<double> load_npy(const fs::path &dir, const std::string &name)
{
    return npy::tensor<double>((dir / (name + ".npy")).string());
}

const std::vector<double> &channel(const dmap::TaylorLevel &lvl, int q)
{
    const std::vector<double> *arrays[8] = {&lvl.h0, &lvl.gu, &lvl.gv,    &lvl.r,
                                            &lvl.ru, &lvl.rv, &lvl.h_min, &lvl.h_max};
    return *arrays[q];
}

double scaled_max_error(const std::vector<double> &got, const std::vector<double> &ref)
{
    double max_err = 0.0, max_ref = 0.0;
    for (size_t k = 0; k < ref.size(); ++k) {
        max_err = std::max(max_err, std::abs(got[k] - ref[k]));
        max_ref = std::max(max_ref, std::abs(ref[k]));
    }
    return max_err / std::max(max_ref, 1e-300);
}

double max_abs(const std::vector<double> &a)
{
    double m = 0.0;
    for (double v : a)
        m = std::max(m, std::abs(v));
    return m;
}

} // namespace

void validate_pyramid(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path golden_dir = args.load_path("golden_dir");
    double tol = (double)args.load_float("tolerance", 1e-12f);
    double tol_cross = (double)args.load_float("tolerance_cross", 1e-12f);
    int cells_per_level = args.load_integer("cells_per_level", 4);
    int n_samples = args.load_integer("n_samples", 33); // per axis per cell

    std::vector<fs::path> cases;
    for (const auto &entry : fs::directory_iterator(golden_dir))
        if (entry.is_directory())
            cases.push_back(entry.path());
    std::sort(cases.begin(), cases.end());
    ASSERT(!cases.empty(), "No golden cases in [%s].", golden_dir.string().c_str());

    const dmap::PyramidBuild builds[2] = {dmap::PyramidBuild::Fold, dmap::PyramidBuild::Direct};
    double golden_worst = 0.0, cross_worst = 0.0, leaf_worst = 0.0;
    int64_t checks = 0, violations = 0;
    int64_t r_violations = 0, recovery_violations = 0;

    for (const fs::path &dir : cases) {
        auto values = load_npy(dir, "values");
        double scale = load_npy(dir, "scale").values()[0];
        int nodes = (int)values.shape()[0];
        dmap::TaylorPyramid pyr_fold(values.values().data(), nodes, scale, builds[0]);
        dmap::TaylorPyramid pyr_direct(values.values().data(), nodes, scale, builds[1]);
        const dmap::TaylorPyramid *pyrs[2] = {&pyr_fold, &pyr_direct};
        dmap::HeightGrid field{nodes, nodes, scale, values.values().data()};

        double case_worst = 0.0;
        for (int mode = 0; mode < 2; ++mode) {
            const dmap::TaylorPyramid &pyr = *pyrs[mode];
            fs::path mode_dir = dir / dmap::pyramid_build_name(builds[mode]);

            // (1) Node arrays against the reference, every level.
            for (int k = 0; k < pyr.n_levels; ++k) {
                const dmap::TaylorLevel &lvl = pyr.levels[k];
                for (int q = 0; q < (int)channel_keys.size(); ++q) {
                    auto ref = load_npy(mode_dir, channel_keys[q] + "_L" + std::to_string(k));
                    ASSERT((int)ref.shape()[0] == lvl.m, "level %d shape mismatch", k);
                    case_worst = std::max(case_worst, scaled_max_error(channel(lvl, q), ref.values()));
                }
            }

            // (2) Conservativeness spot check against the bilinear field.
            for (int k = 0; k < pyr.n_levels; ++k) {
                const dmap::TaylorLevel &lvl = pyr.levels[k];
                double wc = pyr.cell_width(k);
                int stride = std::max(1, lvl.m / cells_per_level);
                for (int J = 0; J < lvl.m; J += stride) {
                    for (int I = 0; I < lvl.m; I += stride) {
                        size_t kn = (size_t)J * lvl.m + I;
                        double u0 = (I + 0.5) * wc, v0 = (J + 0.5) * wc;
                        // Slack for the two rounding routes (the pyramid scales
                        // node values, the field scales the interpolant).
                        double slack = 1e-12 * (1.0 + std::abs(lvl.h0[kn]) + lvl.r[kn]);
                        for (int sj = 0; sj < n_samples; ++sj) {
                            for (int si = 0; si < n_samples; ++si) {
                                double fu = (si + 0.5) / n_samples; // inset by construction
                                double fv = (sj + 0.5) / n_samples;
                                double u = (I + fu) * wc, v = (J + fv) * wc;
                                double h = field.h(u, v);
                                ks::vec2d g = field.grad(u, v);
                                double plane = lvl.h0[kn] + lvl.gu[kn] * (u - u0) + lvl.gv[kn] * (v - v0);
                                ++checks;
                                if (std::abs(h - plane) > lvl.r[kn] + slack ||
                                    std::abs(g[0] - lvl.gu[kn]) > lvl.ru[kn] + slack ||
                                    std::abs(g[1] - lvl.gv[kn]) > lvl.rv[kn] + slack || h < lvl.h_min[kn] - slack ||
                                    h > lvl.h_max[kn] + slack)
                                    ++violations;
                            }
                        }
                    }
                }
            }

            // (4) The stored height range sits inside the recovered one.
            std::vector<double> lo, hi;
            for (int k = 0; k < pyr.n_levels; ++k) {
                const dmap::TaylorLevel &lvl = pyr.levels[k];
                dmap::minmax_from_taylor(lvl, pyr.half_extent(k), lo, hi);
                for (size_t q = 0; q < lo.size(); ++q) {
                    double slack = tol_cross * (1.0 + std::abs(lvl.h0[q]) + lvl.r[q]);
                    if (lo[q] > lvl.h_min[q] + slack || hi[q] < lvl.h_max[q] - slack)
                        ++recovery_violations;
                }
            }
        }
        golden_worst = std::max(golden_worst, case_worst);

        // (3) What the two constructions must share, and where they may
        // differ. The fold gets the gradient hulls and the height range
        // exactly, so only h0 and r move; the direct r may only shrink.
        for (int k = 0; k < pyr_fold.n_levels; ++k) {
            const dmap::TaylorLevel &f = pyr_fold.levels[k];
            const dmap::TaylorLevel &d = pyr_direct.levels[k];
            for (int q : shared_channels)
                cross_worst = std::max(cross_worst, scaled_max_error(channel(d, q), channel(f, q)));
            double r_mag = std::max(max_abs(f.r), 1e-300);
            for (size_t q = 0; q < f.r.size(); ++q)
                if (d.r[q] > f.r[q] + tol_cross * r_mag)
                    ++r_violations;
        }
        // At level 0 the enumeration is the four texel corners, so direct
        // must reproduce the closed-form leaf on all eight channels.
        for (int q = 0; q < (int)channel_keys.size(); ++q)
            leaf_worst = std::max(leaf_worst,
                                  scaled_max_error(channel(pyr_direct.levels[0], q), channel(pyr_fold.levels[0], q)));

        // Mip pyramid sanity on the leaf h0 grid: the root mean is the
        // global mean, the root max the global max.
        {
            const std::vector<double> &grid = pyr_fold.levels[0].h0;
            dmap::MipPyramid mip(grid.data(), pyr_fold.n_leaf);
            double mean = 0.0, mx = -INFINITY;
            for (double v : grid) {
                mean += v;
                mx = std::max(mx, v);
            }
            mean /= (double)grid.size();
            double root_mean = mip.levels.back().mean[0];
            double root_max = mip.levels.back().max[0];
            ASSERT(std::abs(root_mean - mean) <= 1e-12 * (1.0 + std::abs(mean)) && root_max == mx,
                   "mip pyramid root mismatch");
        }

        get_default_logger().info("[{}] {} levels, golden worst {:.3e} (both builds)", dir.filename().string(),
                                  pyr_fold.n_levels, case_worst);
    }

    bool pass = golden_worst <= tol && violations == 0 && cross_worst <= tol_cross && leaf_worst <= tol_cross &&
                r_violations == 0 && recovery_violations == 0;
    get_default_logger().info(
        "VERDICT: {} — TaylorPyramid vs numpy golden over {} cases x 2 builds x 8 channels: worst scaled error "
        "{:.3e} (tolerance {:.1e}); conservativeness {} violations in {} checks; the two builds agree on the "
        "gradient and min-max channels to {:.3e} and on the whole leaf to {:.3e}, with {} nodes where the direct "
        "r exceeds the folded one; {} nodes where the stored height range escapes the recovered interval",
        pass ? "PASS" : "FAIL", cases.size(), golden_worst, tol, violations, checks, cross_worst, leaf_worst,
        r_violations, recovery_violations);
    if (!pass)
        std::exit(1);
}
