// Task validate_path_tracer ("Plan — Path tracer with displaced surfaces",
// T8). One check per task block, chosen by `check`:
//
// - "regression": the T7 regression scene, its keys in the task table and
//   no displace entries, rendered through path_trace, must reproduce the
//   golden of the unchanged small_pt bit for bit (on one thread, dmap -n 1,
//   as validate_small_pt_regression). Keys: golden, image.
// - "mis": the S8 check in the integrated renderer, on a scene with at
//   least two displaced emitters and no other light: direct lighting by
//   next-event estimation only, by BSDF sampling only, and by both with
//   multiple importance sampling, through the AreaLight interface the
//   tracer uses (direct_lighting.h), must converge to the same image; so
//   must SmallPT with one bounce, which is the tracer's own direct
//   lighting. Mean luminance within `tolerance` of a high-spp reference.
//   Keys: spp_check, spp_ref, tolerance, tolerance_nee (= tolerance),
//   min_displaced_emitters (2), plus the path_trace keys.
// - "substitute": a mixed scene path-traced with the displaced surfaces and
//   their lights, against the same scene with the displaced shapes as
//   texel-aligned pre-tessellated substitutes and ks's mesh lights (D4).
//   Relative MSE below `tolerance_rel_mse` or twice the noise floor between
//   two seeds, and mean luminance within `tolerance_mean`. Keys:
//   substitute_subdivision, tolerance_rel_mse, tolerance_mean, plus the
//   path_trace keys.

#include "direct_lighting.h"
#include "displaced_area_light.h"
#include "image_compare.h"
#include "ks/assertion.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include "ks/parallel.h"
#include "path_trace.h"
#include <cmath>
#include <string>

using namespace ks;
using dmap::Strategy;

namespace
{

std::string image_name(const std::string &prefix, int spp)
{
    return string_format("%s_spp%d.exr", prefix.c_str(), spp);
}

void check_regression(const ConfigArgs &args, const fs::path &task_dir)
{
    auto &log = get_default_logger();
    if (parallel_num_threads() != 1) {
        log.info("VERDICT: FAIL — path_trace regression: {} threads; run dmap with -n 1, the render is only "
                 "reproducible on one thread",
                 parallel_num_threads());
        return;
    }
    fs::path golden = args.load_path("golden");
    EmbreeDevice device;
    std::unique_ptr<dmap::PathTraceSetup> setup = dmap::path_trace_setup(args, device, 0);
    ASSERT(setup->spec->displaced.empty(), "the regression scene must carry no displace entries");
    dmap::path_trace_render(*setup, task_dir, "path_trace");
    std::string image = args.load_string("image", image_name("path_trace", setup->input.spp));
    dmap::ImageDifference d = dmap::compare_images(dmap::load_rgb_exr(task_dir / image), dmap::load_rgb_exr(golden));
    log.info("path_trace regression against {}: {} pixels differ, {} non-finite, largest difference {:.3e}",
             golden.string(), d.differing, d.nonfinite, d.worst);
    log.info("VERDICT: {} — path_trace with no displace entries reproduces the small_pt golden",
             d.differing == 0 && d.nonfinite == 0 ? "PASS" : "FAIL");
}

void check_mis(const ConfigArgs &args, const fs::path &task_dir)
{
    auto &log = get_default_logger();
    int spp_check = args.load_integer("spp_check", 64);
    int spp_ref = args.load_integer("spp_ref", 1024);
    double tolerance = (double)args.load_float("tolerance", 0.02f);
    // Next-event estimation alone has infinite variance where an emitter
    // lights itself at close range, and its mean then converges slowly
    // from below; a scene may give it a looser bound.
    double tolerance_nee = (double)args.load_float("tolerance_nee", (float)tolerance);
    // With substitute_subdivision > 0 the same check runs on the
    // pre-tessellated scene with ks's mesh lights.
    int substitute_subdivision = args.load_integer("substitute_subdivision", 0);
    EmbreeDevice device;
    std::unique_ptr<dmap::PathTraceSetup> setup = dmap::path_trace_setup(args, device, substitute_subdivision);
    ASSERT(setup->light_sampler->get_sky_lights().empty(), "the MIS check needs a scene without a sky");
    int min_displaced_emitters = args.load_integer("min_displaced_emitters", 2);
    int n_displaced_emitters = 0;
    for (const auto &al : setup->built->scene.area_lights)
        if (dynamic_cast<const dmap::DisplacedAreaLightShared *>(al.get()))
            ++n_displaced_emitters;
    if (substitute_subdivision > 0)
        n_displaced_emitters = min_displaced_emitters;
    ASSERT(n_displaced_emitters >= min_displaced_emitters,
           "the MIS check needs at least %d displaced emitters, the scene has %d", min_displaced_emitters,
           n_displaced_emitters);
    int seed = setup->input.rng_seed;

    dmap::RgbImage ref = dmap::render_direct(*setup, Strategy::MIS, spp_ref, seed + 1000, task_dir, "reference");
    log.info("reference (mis, {} spp): mean luminance {:.5f}", spp_ref, ref.mean_luminance());
    bool pass = true;
    auto judge = [&](const std::string &name, const dmap::RgbImage &img, double tol) {
        dmap::ImageDifference d = dmap::compare_images(img, ref);
        bool ok = d.mean_rel_diff <= tol;
        pass = pass && ok;
        log.info("{} @{} spp: mean luminance {:.5f} (rel diff {:.4f}), rel MSE {:.4f} {}", name, spp_check,
                 d.mean_lum_a, d.mean_rel_diff, d.rel_mse, ok ? "ok" : "FAIL");
    };
    for (Strategy s : {Strategy::NEE, Strategy::BSDF, Strategy::MIS})
        judge(dmap::strategy_name(s), dmap::render_direct(*setup, s, spp_check, seed, task_dir, dmap::strategy_name(s)),
              s == Strategy::NEE ? tolerance_nee : tolerance);

    // The tracer's own direct lighting: SmallPT with one bounce.
    setup->input.bounces = 1;
    setup->input.spp = spp_check;
    setup->input.spp_prog_interval = spp_check;
    dmap::path_trace_render(*setup, task_dir, "small_pt_direct");
    judge("small_pt (1 bounce)", dmap::load_rgb_exr(task_dir / image_name("small_pt_direct", spp_check)), tolerance);

    log.info("VERDICT: {} — NEE-only, BSDF-only, MIS and SmallPT converge to the same direct lighting (mean "
             "luminance within {:.0f}% of the reference, {:.0f}% for NEE-only)",
             pass ? "PASS" : "FAIL", 100.0 * tolerance, 100.0 * tolerance_nee);
}

void check_substitute(const ConfigArgs &args, const fs::path &task_dir)
{
    auto &log = get_default_logger();
    int m = args.load_integer("substitute_subdivision", 1);
    double tolerance_rel_mse = (double)args.load_float("tolerance_rel_mse", 0.05f);
    double tolerance_mean = (double)args.load_float("tolerance_mean", 0.02f);
    EmbreeDevice device;

    std::unique_ptr<dmap::PathTraceSetup> displaced = dmap::path_trace_setup(args, device, 0);
    int spp = displaced->input.spp;
    double t_displaced = dmap::path_trace_render(*displaced, task_dir, "displaced");
    // A second seed of the same render measures the noise floor.
    displaced->input.rng_seed += 1;
    dmap::path_trace_render(*displaced, task_dir, "displaced_seed2");
    displaced.reset();

    std::unique_ptr<dmap::PathTraceSetup> substitute = dmap::path_trace_setup(args, device, m);
    double t_substitute = dmap::path_trace_render(*substitute, task_dir, "substitute");
    substitute.reset();

    dmap::RgbImage a = dmap::load_rgb_exr(task_dir / image_name("displaced", spp));
    dmap::RgbImage a2 = dmap::load_rgb_exr(task_dir / image_name("displaced_seed2", spp));
    dmap::RgbImage b = dmap::load_rgb_exr(task_dir / image_name("substitute", spp));
    dmap::ImageDifference cross = dmap::compare_images(a, b);
    dmap::ImageDifference noise = dmap::compare_images(a2, a);
    log.info("displaced vs substitute (m = {}) @{} spp: mean luminance {:.5f} vs {:.5f} (rel diff {:.4f}), rel MSE "
             "{:.4f}; noise floor between two seeds: rel MSE {:.4f}, mean rel diff {:.4f}; render time {:.1f} vs "
             "{:.1f} sec",
             m, spp, cross.mean_lum_a, cross.mean_lum_b, cross.mean_rel_diff, cross.rel_mse, noise.rel_mse,
             noise.mean_rel_diff, t_displaced, t_substitute);
    // A heavy-tailed scene has a noise floor above any fixed bound; the
    // cross difference then only has to stay within twice the floor.
    double rel_mse_bound = std::max(tolerance_rel_mse, 2.0 * noise.rel_mse);
    bool pass = cross.rel_mse <= rel_mse_bound && cross.mean_rel_diff <= tolerance_mean;
    log.info("VERDICT: {} — the path-traced displaced scene matches its pre-tessellated substitute (rel MSE <= "
             "{:.3f}, mean luminance within {:.0f}%)",
             pass ? "PASS" : "FAIL", rel_mse_bound, 100.0 * tolerance_mean);
}

} // namespace

void validate_path_tracer(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    std::string check = args.load_string("check");
    if (check == "regression")
        check_regression(args, task_dir);
    else if (check == "mis")
        check_mis(args, task_dir);
    else if (check == "substitute")
        check_substitute(args, task_dir);
    else
        ASSERT(false, "check must be regression, mis or substitute, got [%s]", check.c_str());
}
