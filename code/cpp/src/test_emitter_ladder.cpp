// Task test_emitter_ladder ("Plan — Path tracer with displaced surfaces",
// T9): the S8 ladder in a scene with multi-triangle displaced emitters.
// One scene per task block. Every sampler kind (`samplers`) renders direct
// lighting under every strategy (`strategies`, next-event estimation only
// and MIS by default) at `spp_ladder` samples per pixel, plus the uniform
// line-casting baseline (`uniform`, next-event only), against an MIS
// reference at `spp_ref` with the scene's own kind. Measured per image:
// relative MSE and mean luminance over the whole frame, over the receivers
// (the pixels whose first hit is not an emitter) and over the far
// receivers (those at least `gate_distance` from every emitter's base
// mesh),
// the render time, and the draw cost per light sample on the receivers
// (selection plus sample, no shadow ray; a line with all its crossings for
// the uniform baseline). Per kind, the cost of building the lights and the
// bytes the triangles own against the bytes shared through the asset (D5).
// Results in ladder.csv; FLIP is added by
// code/python/poc/experiments/flip_t9.py.
//
// The unbiasedness gate. Next-event estimation alone has infinite variance
// where an emitter lights itself or a receiver at close range (T8), and
// its mean then converges from below, so it is judged at `spp_gate`
// samples per pixel on the far receivers alone (one more render per kind
// when spp_gate differs from spp_ladder); MIS is judged on the whole frame
// at spp_ladder. Verdict: every judged mean within `tolerance` of the
// reference plus two standard errors of its own noise, estimated from
// the relative MSE over the judged pixels as 2 sqrt(rel MSE / pixels).
//
// With `density_check` (default true) every kind's area density is also
// compared with the product table's, the exact-Jacobian density over the
// same leaf lattice, at random points of the first emitting triangle:
// percentiles of the ratio (the receiver-aware kind at a receiver two
// units in front of the triangle's centre). A wide spread means the
// descent's per-level weights compose to a density far from the target,
// which shows in the ladder as heavy tails.
//
// Keys: scene, spp_ladder (64), spp_ref (4096), spp_gate (1024),
// gate_distance (0.25), tolerance (0.02), strategies, samplers, uniform
// (true), draws_per_receiver (32), max_receivers (4096), density_check
// (true), plus the path_trace keys.

#include "direct_lighting.h"
#include "displaced_area_light.h"
#include "ks/assertion.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include "ks/parallel.h"
#include "path_trace.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace ks;
using dmap::Strategy;

namespace
{

struct Masks
{
    std::vector<uint8_t> receivers, far_receivers;
    size_t n_receivers = 0, n_far = 0;
};

struct Row
{
    std::string strategy, sampler;
    int spp = 0;
    double rel_mse_frame = 0.0, rel_mse_receivers = 0.0, rel_mse_far = 0.0;
    double mean_frame = 0.0, mean_receivers = 0.0, mean_far = 0.0;
    double rel_diff_frame = 0.0, rel_diff_receivers = 0.0, rel_diff_far = 0.0;
    double seconds = 0.0, ns_per_draw = 0.0;
    double allowance = 0.0; // two standard errors of the judged mean
    bool judged = false, ok = true;
};

struct KindCost
{
    std::string sampler;
    uint32_t n_lights = 0;
    double build_ms = 0.0;
    size_t sampler_bytes = 0;
};

std::vector<std::string> load_string_list(const ConfigArgs &args, const char *key,
                                          const std::vector<std::string> &fallback)
{
    if (!args.contains(key))
        return fallback;
    ConfigArgs list = args[key];
    std::vector<std::string> out;
    for (int i = 0; i < (int)list.array_size(); ++i)
        out.push_back(list.load_string(i));
    return out;
}

// Rebuilds the scene's area lights with every displaced geometry set to
// the kind, and the light sampler over them. Returns the build time in
// milliseconds.
double set_sampler_kind(dmap::PathTraceSetup &setup, dmap::EmitterSamplerKind kind)
{
    Scene &scene = setup.built->scene;
    for (dmap::DisplacedGeometry *g : setup.built->displaced)
        if (g)
            g->emitter_sampler = kind;
    auto start = std::chrono::steady_clock::now();
    scene.area_lights.clear();
    scene.build_area_lights();
    double ms = 1e3 * std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    LightPointers light_ptrs;
    if (setup.sky)
        light_ptrs.lights.push_back(setup.sky.get());
    else if (setup.spec->sky)
        light_ptrs.lights.push_back(setup.spec->sky.get());
    if (!setup.lights.empty())
        for (const auto &light : setup.lights)
            light_ptrs.lights.push_back(light.get());
    else
        for (const auto &light : setup.spec->lights)
            light_ptrs.lights.push_back(light.get());
    for (const auto &al : scene.area_lights)
        light_ptrs.area_lights.push_back(al.get());
    setup.light_sampler = std::make_unique<PowerLightSampler>(scene.bound());
    setup.light_sampler->build(light_ptrs);
    setup.input.light_sampler = setup.light_sampler.get();
    return ms;
}

std::vector<const dmap::DisplacedAreaLightShared *> displaced_emitters(const dmap::PathTraceSetup &setup)
{
    std::vector<const dmap::DisplacedAreaLightShared *> out;
    for (const auto &al : setup.built->scene.area_lights)
        if (const auto *dl = dynamic_cast<const dmap::DisplacedAreaLightShared *>(al.get()))
            out.push_back(dl);
    return out;
}

// Closest-point distance from p to the triangle (a, b, c) (Ericson,
// Real-Time Collision Detection, 5.1.5).
double distance_to_triangle(const vec3d &p, const vec3d &a, const vec3d &b, const vec3d &c)
{
    vec3d ab = b - a, ac = c - a, ap = p - a;
    double d1 = ab.dot(ap), d2 = ac.dot(ap);
    if (d1 <= 0.0 && d2 <= 0.0)
        return ap.norm();
    vec3d bp = p - b;
    double d3 = ab.dot(bp), d4 = ac.dot(bp);
    if (d3 >= 0.0 && d4 <= d3)
        return bp.norm();
    double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
        return (p - (a + d1 / (d1 - d3) * ab)).norm();
    vec3d cp = p - c;
    double d5 = ab.dot(cp), d6 = ac.dot(cp);
    if (d6 >= 0.0 && d5 <= d6)
        return cp.norm();
    double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
        return (p - (a + d2 / (d2 - d6) * ac)).norm();
    double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && d4 - d3 >= 0.0 && d5 - d6 >= 0.0)
        return (p - (b + (d4 - d3) / ((d4 - d3) + (d5 - d6)) * (c - b))).norm();
    double denom = 1.0 / (va + vb + vc);
    return (p - (a + ab * (vb * denom) + ac * (vc * denom))).norm();
}

// The base triangles of every displaced emitter in world space, the
// reference for the distance of a receiver from the emitters (the
// displacement moves the surface by less than the gate distance).
struct WorldTriangle
{
    vec3d a, b, c;
};

std::vector<WorldTriangle> emitter_base_triangles(const dmap::PathTraceSetup &setup)
{
    std::vector<WorldTriangle> tris;
    for (const dmap::DisplacedAreaLightShared *dl : displaced_emitters(setup)) {
        const dmap::BaseMesh &base = dl->object->base;
        Eigen::Matrix4d m = dl->transform.m.cast<double>();
        auto world = [&](int v) { return (m * base.positions[v].homogeneous()).head<3>().eval(); };
        for (const Eigen::Vector3i &f : base.faces)
            tris.push_back({world(f[0]), world(f[1]), world(f[2])});
    }
    return tris;
}

// The receivers: the pixels whose centre ray hits a non-delta surface that
// is not an emitter; the far receivers among them lie at least
// gate_distance from every emitter's base mesh. Keeps a subset of the receivers'
// hits for the draw benchmark.
Masks receiver_masks(const dmap::PathTraceSetup &setup, float gate_distance, int max_receivers,
                     std::vector<SceneHit> &receivers)
{
    const SmallPTInput &in = setup.input;
    int width = in.render_width, height = in.render_height;
    std::vector<WorldTriangle> emitter_tris = emitter_base_triangles(setup);
    Masks masks;
    masks.receivers.assign((size_t)width * height, 0);
    masks.far_receivers.assign((size_t)width * height, 0);
    std::vector<SceneHit> hits((size_t)width * height);
    parallel_tile_2d(width, height, [&](int x, int y) {
        vec2 film = vec2(x + 0.5f, y + 0.5f).cwiseQuotient(vec2(width, height));
        Ray ray = in.camera->spawn_ray(film, vec2i(width, height), 1);
        size_t k = (size_t)y * width + x;
        SceneHit &hit = hits[k];
        if (!in.scene->intersect1(ray, hit))
            return;
        bool emitter =
            hit.material->emission &&
            setup.light_sampler->get_area_light(MeshTriIndex{hit.inst_id, hit.geom_id, hit.prim_id}).first != nullptr;
        if (emitter || hit.material->bsdf->delta())
            return;
        masks.receivers[k] = 1;
        vec3d p = hit.it.p.cast<double>();
        double d = INFINITY;
        for (const WorldTriangle &tri : emitter_tris)
            d = std::min(d, distance_to_triangle(p, tri.a, tri.b, tri.c));
        masks.far_receivers[k] = d >= gate_distance ? 1 : 0;
    });
    for (size_t k = 0; k < masks.receivers.size(); ++k) {
        masks.n_receivers += masks.receivers[k];
        masks.n_far += masks.far_receivers[k];
    }
    size_t stride = std::max<size_t>(1, masks.n_receivers / std::max(1, max_receivers));
    size_t seen = 0;
    for (size_t k = 0; k < masks.receivers.size(); ++k) {
        if (!masks.receivers[k])
            continue;
        if (seen++ % stride == 0 && (int)receivers.size() < max_receivers)
            receivers.push_back(hits[k]);
    }
    return masks;
}

// Nanoseconds per light draw on the receivers: light selection plus the
// light's sample, one thread, no shadow ray.
double draw_cost_ns(const dmap::PathTraceSetup &setup, const std::vector<SceneHit> &receivers, int draws, int seed)
{
    const SmallPTInput &in = setup.input;
    arr2u res(in.render_width, in.render_height);
    volatile float sink = 0.0f;
    auto start = std::chrono::steady_clock::now();
    for (size_t r = 0; r < receivers.size(); ++r) {
        PTRenderSampler sampler(res, arr2u((uint32_t)r, 0), draws, 0, seed);
        for (int k = 0; k < draws; ++k) {
            float pr_light;
            auto [light_index, light] = setup.light_sampler->sample(sampler.sobol.next(), pr_light);
            vec3 wi;
            float wi_dist, pdf_light;
            color3 Le_over_pdf = light->sample(receivers[r].it, sampler, wi, wi_dist, pdf_light);
            sink = sink + Le_over_pdf[0] + pdf_light;
        }
    }
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return 1e9 * seconds / ((double)receivers.size() * draws);
}

// Nanoseconds per line: the draw and every crossing along it.
double line_cost_ns(const dmap::PathTraceSetup &setup, const dmap::LineEstimator &lines, size_t n_lines, int seed)
{
    RNG rng(seed);
    std::vector<SceneHit> hits;
    volatile float sink = 0.0f;
    auto start = std::chrono::steady_clock::now();
    for (size_t l = 0; l < n_lines; ++l) {
        dmap::line_emitter_hits(setup, lines, rng, hits);
        sink = sink + (float)hits.size();
    }
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return 1e9 * seconds / n_lines;
}

// Percentiles of the ratio of a kind's area density to the product table's
// over random points of one triangle's domain, printed per kind.
void density_check(const dmap::DisplacedAreaLightShared &emitter, const std::vector<std::string> &samplers,
                   const std::string &scene_name, int seed)
{
    auto &log = get_default_logger();
    const dmap::DisplacedObject &object = *emitter.object;
    const dmap::BaseTriangle &tri = object.triangles[emitter.prim_id(0)];
    constexpr double beta = 0.05;
    dmap::DisplacedEmitterLight table(tri, object.field, *object.asset->pyramid, *object.emission,
                                      dmap::EmitterSamplerKind::ProductTable, beta);
    // A receiver two units in front of the triangle's centre, facing it.
    vec2d c = (tri.t0 + tri.t1 + tri.t2) / 3.0;
    dmap::NormalFrame nf = dmap::normal_frame_at(tri, c[0], c[1]);
    vec3d p_recv = tri.P(c[0], c[1]) + 2.0 * nf.N, n_recv = -nf.N;
    constexpr int n_points = 20000;
    for (const std::string &name : samplers) {
        dmap::DisplacedEmitterLight kind(tri, object.field, *object.asset->pyramid, *object.emission,
                                         dmap::emitter_sampler_from_string(name), beta);
        RNG rng(seed);
        std::vector<double> ratios;
        for (int k = 0; k < n_points; ++k) {
            double a = rng.next(), b = rng.next();
            if (a + b > 1.0) {
                a = 1.0 - a;
                b = 1.0 - b;
            }
            vec2d t = tri.t0 + a * (tri.t1 - tri.t0) + b * (tri.t2 - tri.t0);
            double p_table = table.pdf_area(p_recv, n_recv, t[0], t[1]);
            double p_kind = kind.pdf_area(p_recv, n_recv, t[0], t[1]);
            if (p_table > 0.0 && p_kind > 0.0)
                ratios.push_back(p_kind / p_table);
        }
        std::sort(ratios.begin(), ratios.end());
        auto pct = [&](double q) {
            return ratios.empty() ? 0.0 : ratios[std::min(ratios.size() - 1, (size_t)(q * ratios.size()))];
        };
        log.info("[{}] density of {} over the product table's on triangle {}: {} of {} points with mass; ratio "
                 "percentiles 1% {:.3f}, 10% {:.3f}, 50% {:.3f}, 90% {:.3f}, 99% {:.3f}, max {:.3f}",
                 scene_name, name, emitter.prim_id(0), ratios.size(), n_points, pct(0.01), pct(0.10), pct(0.50),
                 pct(0.90), pct(0.99), pct(1.0));
    }
}

std::string format_bytes(size_t bytes)
{
    if (bytes >= (size_t)1 << 20)
        return string_format("%.1f MB", bytes / 1048576.0);
    return string_format("%.1f KB", bytes / 1024.0);
}

} // namespace

void test_emitter_ladder(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    auto &log = get_default_logger();
    int spp_ladder = args.load_integer("spp_ladder", 64);
    int spp_ref = args.load_integer("spp_ref", 4096);
    int spp_gate = args.load_integer("spp_gate", 1024);
    float gate_distance = args.load_float("gate_distance", 0.25f);
    double tolerance = (double)args.load_float("tolerance", 0.02f);
    bool with_uniform = args.load_bool("uniform", true);
    int draws_per_receiver = args.load_integer("draws_per_receiver", 32);
    int max_receivers = args.load_integer("max_receivers", 4096);
    std::vector<std::string> strategies = load_string_list(args, "strategies", {"nee", "mis"});
    std::vector<std::string> samplers = load_string_list(
        args, "samplers", {"emission_table", "product_table", "area_descent", "product_descent", "receiver_descent"});
    bool with_nee = std::find(strategies.begin(), strategies.end(), "nee") != strategies.end();

    EmbreeDevice device;
    std::unique_ptr<dmap::PathTraceSetup> setup = dmap::path_trace_setup(args, device, 0);
    ASSERT(setup->light_sampler->get_sky_lights().empty(), "the ladder needs a scene without a sky");
    std::vector<const dmap::DisplacedAreaLightShared *> emitters = displaced_emitters(*setup);
    ASSERT(!emitters.empty(), "the ladder needs a displaced emitter");
    ASSERT(emitters.size() == setup->built->scene.area_lights.size(),
           "the uniform baseline covers displaced emitters only; the scene has other area lights");
    int seed = setup->input.rng_seed;
    std::string scene_name = args.load_string("scene", "inline");

    // The data shared through the asset (D5): the pyramid and the node grid
    // per displacement asset, the emission statistics per emitting object.
    size_t shared_bytes = 0;
    std::set<const dmap::DisplacementAsset *> assets;
    size_t n_triangles = 0;
    for (const auto *dl : emitters) {
        const dmap::DisplacedObject &object = *dl->object;
        n_triangles += object.triangles.size();
        if (assets.insert(object.asset).second)
            shared_bytes +=
                object.asset->pyramid->memory_bytes() + (size_t)object.field.W * object.field.H * sizeof(double);
        shared_bytes += object.emission->memory_bytes();
    }

    std::vector<SceneHit> receivers;
    Masks masks = receiver_masks(*setup, gate_distance, max_receivers, receivers);
    ASSERT(masks.n_far > 0, "no receiver lies %.2f from every emitter; the gate would judge nothing", gate_distance);
    log.info("[{}] {} displaced emitters, {} base triangles, {} receiver pixels of {} ({} at least {:.2f} from every "
             "emitter's base mesh), {} shared bytes",
             scene_name, emitters.size(), n_triangles, masks.n_receivers, masks.receivers.size(), masks.n_far,
             gate_distance, format_bytes(shared_bytes));

    if (args.load_bool("density_check", true))
        density_check(*emitters[0], samplers, scene_name, seed);

    double t_ref;
    dmap::RgbImage ref =
        dmap::render_direct(*setup, Strategy::MIS, spp_ref, seed + 1000, task_dir, "reference", nullptr, &t_ref);
    double ref_frame = ref.mean_luminance(), ref_receivers = ref.mean_luminance(&masks.receivers),
           ref_far = ref.mean_luminance(&masks.far_receivers);
    log.info("[{}] reference (mis, {} spp, {}): mean luminance {:.5f} frame, {:.5f} receivers, {:.5f} far "
             "receivers, {:.1f} sec",
             scene_name, spp_ref, dmap::emitter_sampler_name(emitters[0]->geometry->emitter_sampler), ref_frame,
             ref_receivers, ref_far, t_ref);

    std::vector<Row> rows;
    std::vector<KindCost> costs;
    auto measure = [&](Row row, const dmap::RgbImage &img) {
        dmap::ImageDifference frame = dmap::compare_images(img, ref);
        dmap::ImageDifference recv = dmap::compare_images(img, ref, &masks.receivers);
        dmap::ImageDifference far = dmap::compare_images(img, ref, &masks.far_receivers);
        row.rel_mse_frame = frame.rel_mse;
        row.rel_mse_receivers = recv.rel_mse;
        row.rel_mse_far = far.rel_mse;
        row.mean_frame = frame.mean_lum_a;
        row.mean_receivers = recv.mean_lum_a;
        row.mean_far = far.mean_lum_a;
        row.rel_diff_frame = frame.mean_rel_diff;
        row.rel_diff_receivers = recv.mean_rel_diff;
        row.rel_diff_far = far.mean_rel_diff;
        if (row.strategy == "mis") {
            row.judged = true;
            row.allowance = 2.0 * std::sqrt(row.rel_mse_frame / masks.receivers.size());
            row.ok = row.rel_diff_frame <= tolerance + row.allowance;
        } else if (row.strategy == "nee" && row.spp == spp_gate) {
            row.judged = true;
            row.allowance = 2.0 * std::sqrt(row.rel_mse_far / masks.n_far);
            row.ok = row.rel_diff_far <= tolerance + row.allowance;
        }
        log.info(
            "[{}] {:4s} {:18s} @{} spp: rel MSE {:.4f} frame / {:.4f} receivers / {:.4f} far, mean {:+.2f}% / "
            "{:+.2f}% / {:+.2f}%, {:.1f} sec, {:.0f} ns per draw{}",
            scene_name, row.strategy, row.sampler, row.spp, row.rel_mse_frame, row.rel_mse_receivers, row.rel_mse_far,
            100.0 * (row.mean_frame / ref_frame - 1.0), 100.0 * (row.mean_receivers / ref_receivers - 1.0),
            100.0 * (row.mean_far / ref_far - 1.0), row.seconds, row.ns_per_draw,
            row.judged ? string_format(" (allowance %.2f%%) %s", 100.0 * row.allowance, row.ok ? "ok" : "FAIL") : "");
        rows.push_back(row);
    };
    // The ladder rows of one kind (the uniform baseline is next-event
    // only), then its gate render when the gate count differs from the
    // ladder count.
    auto ladder = [&](const std::string &name, double ns, const dmap::LineEstimator *lines) {
        bool is_uniform = lines != nullptr;
        std::vector<std::string> kind_strategies = is_uniform ? std::vector<std::string>{"nee"} : strategies;
        for (const std::string &strategy : kind_strategies) {
            Row row;
            row.strategy = strategy;
            row.sampler = name;
            row.spp = spp_ladder;
            row.ns_per_draw = ns;
            Strategy s = is_uniform ? Strategy::Uniform : dmap::strategy_from_string(strategy);
            dmap::RgbImage img =
                dmap::render_direct(*setup, s, spp_ladder, seed, task_dir, strategy + "_" + name, lines, &row.seconds);
            measure(row, img);
        }
        if (with_nee && spp_gate != spp_ladder) {
            Row row;
            row.strategy = "nee";
            row.sampler = name;
            row.spp = spp_gate;
            row.ns_per_draw = ns;
            Strategy s = is_uniform ? Strategy::Uniform : Strategy::NEE;
            dmap::RgbImage img =
                dmap::render_direct(*setup, s, spp_gate, seed + 1, task_dir, "gate_nee_" + name, lines, &row.seconds);
            measure(row, img);
        }
    };

    for (const std::string &name : samplers) {
        dmap::EmitterSamplerKind kind = dmap::emitter_sampler_from_string(name);
        KindCost cost;
        cost.sampler = name;
        cost.build_ms = set_sampler_kind(*setup, kind);
        for (const auto *dl : displaced_emitters(*setup)) {
            cost.n_lights += dl->light_count();
            for (const auto &light : dl->lights)
                cost.sampler_bytes += light->sampler->memory_bytes();
        }
        log.info("[{}] {}: {} lights built in {:.1f} ms, {} owned by the triangles ({} per light), {} shared",
                 scene_name, name, cost.n_lights, cost.build_ms, format_bytes(cost.sampler_bytes),
                 format_bytes(cost.n_lights ? cost.sampler_bytes / cost.n_lights : 0), format_bytes(shared_bytes));
        costs.push_back(cost);
        ladder(name, draw_cost_ns(*setup, receivers, draws_per_receiver, seed), nullptr);
    }

    if (with_uniform) {
        dmap::LineEstimator lines(dmap::displaced_emitter_bound(*setup));
        log.info("[{}] uniform baseline: lines through the emitters' box [{:.2f} {:.2f} {:.2f}] to [{:.2f} {:.2f} "
                 "{:.2f}], offset area {:.3f}",
                 scene_name, lines.lines.box.min[0], lines.lines.box.min[1], lines.lines.box.min[2],
                 lines.lines.box.max[0], lines.lines.box.max[1], lines.lines.box.max[2], lines.lines.offset_area());
        ladder("uniform", line_cost_ns(*setup, lines, (size_t)receivers.size() * draws_per_receiver, seed), &lines);
    }

    std::ofstream csv(task_dir / "ladder.csv");
    csv << "scene,strategy,sampler,spp,rel_mse_frame,rel_mse_receivers,rel_mse_far,mean_frame,mean_receivers,"
           "mean_far,rel_diff_frame,rel_diff_receivers,rel_diff_far,render_seconds,ns_per_draw,allowance,judged,ok\n";
    for (const Row &r : rows)
        csv << scene_name << "," << r.strategy << "," << r.sampler << "," << r.spp << "," << r.rel_mse_frame << ","
            << r.rel_mse_receivers << "," << r.rel_mse_far << "," << r.mean_frame << "," << r.mean_receivers << ","
            << r.mean_far << "," << r.rel_diff_frame << "," << r.rel_diff_receivers << "," << r.rel_diff_far << ","
            << r.seconds << "," << r.ns_per_draw << "," << r.allowance << "," << (r.judged ? 1 : 0) << ","
            << (r.ok ? 1 : 0) << "\n";
    csv << "\nscene,sampler,n_lights,build_ms,sampler_bytes,shared_bytes,n_triangles\n";
    for (const KindCost &c : costs)
        csv << scene_name << "," << c.sampler << "," << c.n_lights << "," << c.build_ms << "," << c.sampler_bytes << ","
            << shared_bytes << "," << n_triangles << "\n";
    csv << "\nreference_mean_frame," << ref_frame << "\nreference_mean_receivers," << ref_receivers
        << "\nreference_mean_far," << ref_far << "\nreference_seconds," << t_ref << "\nn_receivers,"
        << masks.n_receivers << "\nn_far_receivers," << masks.n_far << "\n";

    bool pass = true;
    for (const Row &r : rows)
        pass = pass && r.ok;
    log.info("VERDICT: {} — emitter sampling ladder on {}: every sampler's mean luminance within {:.0f}% of the "
             "reference plus two standard errors of its noise (whole frame under MIS at {} spp, far receivers under "
             "next-event estimation at {} spp)",
             pass ? "PASS" : "FAIL", scene_name, 100.0 * tolerance, spp_ladder, spp_gate);
}
