// Phase S1 validation ("Plan — A-MVP sampling implementation"): does the
// C++ pointwise evaluation reproduce the numpy reference? Loads the golden
// .npy cases written by export_golden_pointwise.py and compares every
// quantity at every sample point. One question, one verdict.

#include "displaced_surface.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
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

// Max abs difference, reported relative to the max abs golden value
// (a scale-aware error: quantities like lam_min sit near 1, n components
// near 0, and a pointwise relative error would blow up at zero crossings).
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

void validate_pointwise(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path golden_dir = args.load_path("golden_dir");
    double tol = (double)args.load_float("tolerance", 1e-9f);

    std::vector<fs::path> cases;
    for (const auto &entry : fs::directory_iterator(golden_dir))
        if (entry.is_directory())
            cases.push_back(entry.path());
    std::sort(cases.begin(), cases.end());
    ASSERT(!cases.empty(), "No golden cases in [%s].", golden_dir.string().c_str());

    const std::vector<std::string> scalar_names = {"h",   "hu",  "hv",       "G00",     "G01",
                                                   "G11", "det", "sqrt_det", "lam_min", "lam_max"};
    std::map<std::string, double> worst;

    for (const fs::path &dir : cases) {
        auto q = load_npy(dir, "q");
        auto m = load_npy(dir, "m");
        auto values = load_npy(dir, "values");
        auto scale = load_npy(dir, "scale");
        auto X = load_npy(dir, "X");
        auto Y = load_npy(dir, "Y");

        auto row3 = [](const npy::tensor<double> &t, size_t r) {
            return vec3d(t.values()[3 * r], t.values()[3 * r + 1], t.values()[3 * r + 2]);
        };
        dmap::BaseTriangle tri(row3(q, 0), row3(q, 1), row3(q, 2), row3(m, 0), row3(m, 1), row3(m, 2));
        dmap::HeightGrid field;
        field.H = (int)values.shape()[0];
        field.W = (int)values.shape()[1];
        field.scale = scale.values()[0];
        field.values = values.values().data();

        size_t n_pts = X.size();
        std::map<std::string, std::vector<double>> got;
        for (const auto &name : scalar_names)
            got[name].resize(n_pts);
        std::vector<double> got_n(3 * n_pts);

        for (size_t k = 0; k < n_pts; ++k) {
            dmap::PointwiseFields f = dmap::pointwise_fields(tri, field, X.values()[k], Y.values()[k]);
            got["h"][k] = f.h;
            got["hu"][k] = f.hu;
            got["hv"][k] = f.hv;
            got["G00"][k] = f.G00;
            got["G01"][k] = f.G01;
            got["G11"][k] = f.G11;
            got["det"][k] = f.det;
            got["sqrt_det"][k] = f.sqrt_det;
            got["lam_min"][k] = f.lam_min;
            got["lam_max"][k] = f.lam_max;
            for (int c = 0; c < 3; ++c)
                got_n[3 * k + c] = f.n[c];
        }

        double case_worst = 0.0;
        for (const auto &name : scalar_names) {
            double err = scaled_max_error(got[name], load_npy(dir, name).values());
            worst[name] = std::max(worst[name], err);
            case_worst = std::max(case_worst, err);
        }
        double err_n = scaled_max_error(got_n, load_npy(dir, "n").values());
        worst["n"] = std::max(worst["n"], err_n);
        case_worst = std::max(case_worst, err_n);

        get_default_logger().info("[{}] {} points, worst scaled error {:.3e}", dir.filename().string(), n_pts,
                                  case_worst);
    }

    double overall = 0.0;
    for (const auto &[name, err] : worst) {
        get_default_logger().info("{:>10}: worst scaled error {:.3e}", name, err);
        overall = std::max(overall, err);
    }

    bool pass = overall <= tol;
    get_default_logger().info("VERDICT: {} — C++ pointwise evaluation vs numpy golden over {} cases: "
                              "worst scaled error {:.3e} (tolerance {:.1e})",
                              pass ? "PASS" : "FAIL", cases.size(), overall, tol);
    if (!pass)
        std::exit(1);
}
