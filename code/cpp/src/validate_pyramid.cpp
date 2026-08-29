// Phase S3 validation ("Plan — A-MVP sampling implementation"), two checks:
// (1) the C++ TaylorPyramid reproduces the numpy reference node-for-node at
// every level (golden .npy cases from export_golden_pyramid.py);
// (2) conservativeness spot check: dense samples inside cells at every
// level stay within the node's plane and gradient bounds. Plus a sanity
// check of the mean/max mip pyramid. One question, one verdict.

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

npy::tensor<double> load_npy(const fs::path &dir, const std::string &name)
{
    return npy::tensor<double>((dir / (name + ".npy")).string());
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

} // namespace

void validate_pyramid(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path golden_dir = args.load_path("golden_dir");
    double tol = (double)args.load_float("tolerance", 1e-12f);
    int cells_per_level = args.load_integer("cells_per_level", 4);
    int n_samples = args.load_integer("n_samples", 33); // per axis per cell

    std::vector<fs::path> cases;
    for (const auto &entry : fs::directory_iterator(golden_dir))
        if (entry.is_directory())
            cases.push_back(entry.path());
    std::sort(cases.begin(), cases.end());
    ASSERT(!cases.empty(), "No golden cases in [%s].", golden_dir.string().c_str());

    const std::vector<std::string> keys = {"h0", "gu", "gv", "r", "ru", "rv"};
    double golden_worst = 0.0;
    int64_t checks = 0, violations = 0;

    for (const fs::path &dir : cases) {
        auto values = load_npy(dir, "values");
        double scale = load_npy(dir, "scale").values()[0];
        int nodes = (int)values.shape()[0];
        dmap::TaylorPyramid pyr(values.values().data(), nodes, scale);

        // (1) Node arrays against the reference, every level.
        double case_worst = 0.0;
        for (int k = 0; k < pyr.n_levels; ++k) {
            const dmap::TaylorLevel &lvl = pyr.levels[k];
            const std::vector<double> *arrays[6] = {&lvl.h0, &lvl.gu, &lvl.gv, &lvl.r, &lvl.ru, &lvl.rv};
            for (int q = 0; q < 6; ++q) {
                auto ref = load_npy(dir, keys[q] + "_L" + std::to_string(k));
                ASSERT((int)ref.shape()[0] == lvl.m, "level %d shape mismatch", k);
                case_worst = std::max(case_worst, scaled_max_error(*arrays[q], ref.values()));
            }
        }
        golden_worst = std::max(golden_worst, case_worst);

        // (2) Conservativeness spot check against the bilinear field.
        dmap::HeightGrid field{nodes, nodes, scale, values.values().data()};
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
                                std::abs(g[1] - lvl.gv[kn]) > lvl.rv[kn] + slack)
                                ++violations;
                        }
                    }
                }
            }
        }

        // Mip pyramid sanity on the leaf h0 grid: the root mean is the
        // global mean, the root max the global max.
        {
            const std::vector<double> &grid = pyr.levels[0].h0;
            dmap::MipPyramid mip(grid.data(), pyr.n_leaf);
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

        get_default_logger().info("[{}] {} levels, golden worst {:.3e}", dir.filename().string(), pyr.n_levels,
                                  case_worst);
    }

    bool pass = golden_worst <= tol && violations == 0;
    get_default_logger().info("VERDICT: {} — TaylorPyramid vs numpy golden over {} cases: worst scaled error "
                              "{:.3e} (tolerance {:.1e}); conservativeness {} violations in {} checks",
                              pass ? "PASS" : "FAIL", cases.size(), golden_worst, tol, violations, checks);
    if (!pass)
        std::exit(1);
}
