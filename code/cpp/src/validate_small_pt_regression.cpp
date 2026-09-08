// Task validate_small_pt_regression ("Plan — Path tracer with displaced
// surfaces", T7): the regression gate of the ks interface changes. The
// task table carries the keys of a `small_pt` task, which renders the
// regression scene into the task directory as ks's `small_pt` does; the
// image is then compared pixel by pixel with the golden. The golden was
// rendered by the unchanged ks before the T7 interface changes, then
// re-rendered once after T8's fix of the next-event estimation MIS weight
// in nee.cpp, which changes every image with more than one light (log
// 2026-09-07); the earlier golden is kept next to it. Verdict: PASS only
// when no pixel differs.
// The render must run on one thread (dmap -n 1): with several, two runs
// of the same binary differ in more than half of the pixels (log
// 2026-09-07), and the golden is a single-thread render too.
//
// Config: the `small_pt` keys, plus golden (a path under the asset root,
// the golden EXR), and image (the rendered file name, default
// small_pt_spp<spp>.exr).

#include "ks/config.h"
#include "ks/file_util.h"
#include "ks/image_util.h"
#include "ks/log_util.h"
#include "ks/parallel.h"
#include <algorithm>
#include <cmath>
#include <string>

using namespace ks;

namespace ks
{
void small_pt(const ConfigArgs &args, const fs::path &task_dir, int task_id);
}

void validate_small_pt_regression(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    auto &log = get_default_logger();
    fs::path golden = args.load_path("golden");
    std::string image = args.load_string("image", string_format("small_pt_spp%d.exr", args.load_integer("spp")));

    if (parallel_num_threads() != 1) {
        log.info("VERDICT: FAIL — small_pt regression: {} threads; run dmap with -n 1, the render is only "
                 "reproducible on one thread",
                 parallel_num_threads());
        return;
    }
    small_pt(args, task_dir, task_id);

    int w0, h0, w1, h1;
    std::unique_ptr<color3[]> a = load_from_exr<3>(golden, w0, h0);
    std::unique_ptr<color3[]> b = load_from_exr<3>(task_dir / image, w1, h1);
    ASSERT(a && b, "could not load the golden or the rendered image");
    if (w0 != w1 || h0 != h1) {
        log.info("VERDICT: FAIL — small_pt regression: golden {}x{}, rendered {}x{}", w0, h0, w1, h1);
        return;
    }
    int64_t n = (int64_t)w0 * h0, differing = 0, nonfinite = 0;
    float worst = 0.0f;
    for (int64_t i = 0; i < n; ++i) {
        if (!a[i].allFinite() || !b[i].allFinite()) {
            ++nonfinite;
            continue;
        }
        float d = (a[i] - b[i]).abs().maxCoeff();
        if (d > 0.0f) {
            ++differing;
            worst = std::max(worst, d);
        }
    }
    log.info("small_pt regression against {}: {} of {} pixels differ, {} non-finite, largest difference {:.3e}",
             golden.string(), differing, n, nonfinite, worst);
    log.info("VERDICT: {} — small_pt regression", differing == 0 && nonfinite == 0 ? "PASS" : "FAIL");
}
