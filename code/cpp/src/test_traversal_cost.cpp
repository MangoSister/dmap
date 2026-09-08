// Task test_traversal_cost ("Plan — Path tracer with displaced surfaces",
// T5, the gating measurement): the cost of the traversal on the displaced
// objects of a scene, in object space, over the two ray distributions of
// validate_traversal (camera-like rays that reach the object's bound, rays
// from origins near the surface in random directions), for the node tests
// box, slab at every level, and both with the level policy k = 0..L (L the
// coarsest root level), all with the Newton leaf, and for the scene's
// options with the two-triangle leaf; against embree over the texel-aligned
// mesh with one sub-cell per texel (D4, m = 1), the surface of the
// two-triangle leaf.
// Counts come from a parallel pass with statistics; times from serial
// passes over the same rays on one thread, the best of `repetitions` runs.
// Memory: the pyramid and its node grid and the object's BVH, against the
// mesh arrays and the mesh BVH (embree's memory monitor).
// Along the box walk on every stride-th ray, each node test also records
// the box and the slab intervals: which of the two prunes with the walk's
// maximum distance, and their lengths without it, T3's tightness measure
// on the walk's own nodes.
// The tests a walk makes after its final hit are counted: they bound what
// a front-to-back child order with an early exit could save (the plan's
// open decision).
// Verdict, three parts: (1) both with any k never tests more nodes than
// the box at any level, on any ray; (2) the crossover level by T3's rule,
// the slab's median interval shorter than the box's at every level up to
// it, equals expected_crossover when that is given (T3's value where it
// transfers to the walk's nodes, the measured value otherwise); (3) the
// scene's options are within time_tolerance of the fastest policy. The
// ratio of the time per ray to embree over the mesh is recorded whatever
// it is. A precondition: (0) the BVH over the triangle boxes gives the
// flat loop's hits.
//
// Config: scene, n_rays (per distribution, 50000), seed, repetitions (3),
// n_record_rays (10000), expected_crossover (none), time_tolerance (0.05).

#include "displaced_intersector.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/embree_util.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include "ks/parallel.h"
#include "ks/rng.h"
#include "scene_spec.h"
#include "traversal_test_util.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <spdlog/fmt/fmt.h>
#include <string>
#include <vector>

using namespace ks;

namespace
{

struct Policy
{
    std::string name;
    dmap::TraversalOptions options;
};

// The level policy matters only when the slab is read.
bool same_policy(const dmap::TraversalOptions &a, const dmap::TraversalOptions &b)
{
    return a.bound == b.bound && a.leaf == b.leaf &&
           (a.bound == dmap::BoundMode::Box || a.slab_max_level == b.slab_max_level);
}

// box; slab at every level; both with k = 0..L; the scene's options when
// they are none of these; the scene's options with the two-triangle leaf.
std::vector<Policy> make_policies(const dmap::TraversalOptions &scene_options, int L)
{
    std::vector<Policy> out;
    dmap::TraversalOptions o;
    o.leaf = dmap::LeafMode::Newton;
    o.slab_max_level = L;
    o.bound = dmap::BoundMode::Box;
    out.push_back({"box", o});
    o.bound = dmap::BoundMode::Slab;
    out.push_back({"slab", o});
    for (int k = 0; k <= L; ++k) {
        o.bound = dmap::BoundMode::Both;
        o.slab_max_level = k;
        out.push_back({"both k=" + std::to_string(k), o});
    }
    dmap::TraversalOptions scene = scene_options;
    scene.leaf = dmap::LeafMode::Newton;
    bool listed = std::any_of(out.begin(), out.end(), [&](const Policy &p) { return same_policy(p.options, scene); });
    if (!listed)
        out.push_back({"scene options", scene});
    scene.leaf = dmap::LeafMode::TwoTriangle;
    out.push_back({"two-triangle leaf", scene});
    return out;
}

bool newton_policy(const Policy &p) { return p.options.leaf == dmap::LeafMode::Newton; }

// The counters of one policy over one ray distribution.
struct Cost
{
    int64_t rays = 0, hits = 0;
    dmap::TraversalStats stats;
    double seconds = 0.0; // the best of the repetitions, one thread

    double node_tests() const { return rays ? (double)stats.node_tests / (double)rays : 0.0; }
    double leaf_tests() const { return rays ? (double)stats.leaf_tests / (double)rays : 0.0; }
    double sub_squares() const { return stats.leaf_tests ? (double)stats.sub_squares / (double)stats.leaf_tests : 0.0; }
    double hit_fraction() const { return rays ? (double)hits / (double)rays : 0.0; }
    double after_hit(int64_t after, int64_t all) const { return all ? (double)after / (double)all : 0.0; }
    double us_per_ray() const { return rays ? 1e6 * seconds / (double)rays : 0.0; }
};

// The serial time over rays[begin, end), the best of `repetitions` runs;
// the checksum keeps the compiler from dropping the work.
template <typename Intersect>
double serial_seconds(int begin, int end, int repetitions, const Intersect &intersect, double &checksum)
{
    if (end <= begin)
        return 0.0;
    double best = INFINITY;
    for (int rep = 0; rep < repetitions; ++rep) {
        double sum = 0.0;
        auto t0 = std::chrono::steady_clock::now();
        for (int r = begin; r < end; ++r)
            sum += intersect(r);
        auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double>(t1 - t0).count());
        checksum = sum;
    }
    return best;
}

// embree reports every allocation of a device before it is made and every
// release after it, bytes signed.
struct MemoryCounter
{
    std::atomic<int64_t> current{0}, peak{0};
};

bool memory_monitor(void *ptr, ssize_t bytes, bool)
{
    MemoryCounter *c = (MemoryCounter *)ptr;
    int64_t now = c->current.fetch_add((int64_t)bytes) + (int64_t)bytes;
    int64_t peak = c->peak.load();
    while (now > peak && !c->peak.compare_exchange_weak(peak, now)) {
    }
    return true;
}

double megabytes(double bytes) { return bytes / (1024.0 * 1024.0); }

double median(std::vector<double> v)
{
    if (v.empty())
        return 0.0;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

// The node-test record of one level.
struct LevelRecord
{
    int64_t tests = 0;
    int64_t slab_prunes = 0; // the box passes, the slab prunes
    int64_t box_prunes = 0;  // the slab passes, the box prunes
    int64_t both_prune = 0;
    int64_t slab_shorter = 0;
    std::vector<double> box, slab, both; // lengths without the maximum distance where all three pass
};

size_t pyramid_bytes(const dmap::TaylorPyramid &p)
{
    size_t n = 0;
    for (const dmap::TaylorLevel &l : p.levels)
        n += 8 * (size_t)l.m * (size_t)l.m * sizeof(double);
    return n;
}

} // namespace

void test_traversal_cost(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    std::string scene_name = args.load_string("scene");
    const dmap::SceneSpec *spec = args.asset_table().get<dmap::SceneSpec>(scene_name);
    ASSERT(spec, "no scene named [%s]", scene_name.c_str());
    spec->require_valid();
    int n_rays = args.load_integer("n_rays", 50000);
    int seed = args.load_integer("seed", 2027);
    int repetitions = args.load_integer("repetitions", 3);
    int n_record_rays = args.load_integer("n_record_rays", 10000);
    int expected_crossover = args.load_integer("expected_crossover", -100);
    double time_tolerance = (double)args.load_float("time_tolerance", 0.05f);
    constexpr double double_rounding = 1e-9; // the BVH against the flat loop, relative to t + scale

    auto &log = get_default_logger();
    std::ofstream tables(task_dir / "tables.md");
    auto emit = [&](const std::string &line) {
        log.info("{}", line);
        tables << line << "\n";
    };
    bool pass = true;

    for (size_t obj_index = 0; obj_index < spec->displaced.size(); ++obj_index) {
        const dmap::DisplacedObject &d = spec->displaced[obj_index];
        const dmap::SceneShape &shape = spec->shapes[d.shape];
        const dmap::TaylorPyramid &pyramid = *d.asset->pyramid;
        MemoryCounter object_memory, mesh_memory;
        EmbreeDevice object_device, mesh_device;
        rtcSetDeviceMemoryMonitorFunction(object_device, memory_monitor, &object_memory);
        rtcSetDeviceMemoryMonitorFunction(mesh_device, memory_monitor, &mesh_memory);

        dmap::ObjectIntersector obj(d.triangles, pyramid, d.field);
        obj.build_bvh(object_device);
        dmap::ObjectSummary summary = dmap::summarize_object(d, obj);
        int L = summary.max_root_level;
        double scale = (obj.bound.hi - obj.bound.lo).norm();
        dmap::log_object("test_traversal_cost", scene_name, *spec, d, summary);

        RNG rng((uint64_t)seed + 17 * obj_index);
        dmap::ObjectRays distributions = dmap::object_rays(*spec, d, obj, summary, n_rays, rng);
        const std::vector<dmap::RayQuery> &rays = distributions.rays;
        int n_camera = distributions.n_camera;
        int n_total = (int)rays.size();
        const int begin[2] = {0, n_camera}, end[2] = {n_camera, n_total};
        const char *distribution_name[2] = {"camera rays", "rays near the surface"};
        log.info("  rays: {} camera, {} near the surface", n_camera, n_total - n_camera);

        std::unique_ptr<dmap::MeshOracle> mesh = dmap::MeshOracle::texel_aligned(mesh_device, *spec, d, 1);
        std::vector<Policy> policies = make_policies(d.traversal, L);
        dmap::TraversalOptions scene_newton = d.traversal;
        scene_newton.leaf = dmap::LeafMode::Newton;
        int scene_policy = -1;
        for (size_t p = 0; p < policies.size(); ++p)
            if (newton_policy(policies[p]) && same_policy(policies[p].options, scene_newton))
                scene_policy = (int)p;
        ASSERT(scene_policy >= 0, "the scene's options are not among the policies");

        // (0) The BVH over the triangle boxes against the flat loop.
        {
            std::vector<dmap::TraceResult> bvh(n_total), flat(n_total);
            parallel_for(n_total, [&](int r) {
                bvh[r] = dmap::trace(obj, rays[r], scene_newton);
                int tri;
                flat[r].hit = obj.intersect_flat(rays[r].o, rays[r].d, 0.0, INFINITY, scene_newton, flat[r].h, tri);
            });
            int64_t bad = 0;
            for (int r = 0; r < n_total; ++r) {
                bool differ = bvh[r].hit != flat[r].hit;
                if (bvh[r].hit && flat[r].hit)
                    differ = std::abs(bvh[r].h.t - flat[r].h.t) > double_rounding * (flat[r].h.t + scale);
                if (differ) {
                    ++bad;
                    if (bad <= 5)
                        log.info("    BVH {} t={:.9g}, flat loop {} t={:.9g}", bvh[r].hit ? "hit" : "miss",
                                 bvh[r].hit ? bvh[r].h.t : 0.0, flat[r].hit ? "hit" : "miss",
                                 flat[r].hit ? flat[r].h.t : 0.0);
                }
            }
            log.info("(0) BVH over the triangle boxes vs the flat loop, scene options: {} of {} rays differ", bad,
                     n_total);
            pass = pass && bad == 0;
        }

        // Counts per policy and distribution, and check (1) per ray and level.
        std::vector<std::array<Cost, 2>> costs(policies.size());
        std::vector<dmap::TraversalStats> per_ray(n_total), box_per_ray;
        std::vector<dmap::TraceResult> results(n_total);
        int64_t level_violations = 0;
        for (size_t p = 0; p < policies.size(); ++p) {
            const dmap::TraversalOptions &options = policies[p].options;
            parallel_for(n_total, [&](int r) {
                per_ray[r] = dmap::TraversalStats();
                results[r] = dmap::trace(obj, rays[r], options, &per_ray[r]);
            });
            for (int dist = 0; dist < 2; ++dist) {
                Cost &c = costs[p][dist];
                for (int r = begin[dist]; r < end[dist]; ++r) {
                    c.stats.add(per_ray[r]);
                    ++c.rays;
                    c.hits += results[r].hit ? 1 : 0;
                }
            }
            if (p == 0) {
                box_per_ray = per_ray;
            } else if (options.bound == dmap::BoundMode::Both && newton_policy(policies[p])) {
                for (int r = 0; r < n_total; ++r)
                    for (int l = 0; l <= L; ++l)
                        if (per_ray[r].node_tests_per_level[l] > box_per_ray[r].node_tests_per_level[l])
                            ++level_violations;
            }
        }
        log.info("(1) both with k = 0..{} vs box, node tests per ray and level: {} violations", L, level_violations);
        pass = pass && level_violations == 0;

        // Times, one thread, and the mesh baseline.
        double checksum = 0.0;
        std::array<Cost, 2> mesh_cost;
        for (int dist = 0; dist < 2; ++dist) {
            for (size_t p = 0; p < policies.size(); ++p) {
                const dmap::TraversalOptions &options = policies[p].options;
                costs[p][dist].seconds = serial_seconds(
                    begin[dist], end[dist], repetitions,
                    [&](int r) {
                        dmap::DisplacedHit h;
                        int tri;
                        return obj.intersect(rays[r].o, rays[r].d, 0.0, INFINITY, options, h, tri) ? h.t : 0.0;
                    },
                    checksum);
            }
            Cost &m = mesh_cost[dist];
            m.rays = end[dist] - begin[dist];
            for (int r = begin[dist]; r < end[dist]; ++r)
                m.hits += mesh->intersect(rays[r]).hit ? 1 : 0;
            m.seconds = serial_seconds(
                begin[dist], end[dist], repetitions,
                [&](int r) {
                    dmap::MeshOracle::Hit h = mesh->intersect(rays[r]);
                    return h.hit ? h.t : 0.0;
                },
                checksum);
        }
        log.info("  timing checksum {:.6g}", checksum);

        // The tables: per distribution, then node tests per level over all rays.
        for (int dist = 0; dist < 2; ++dist) {
            if (end[dist] <= begin[dist])
                continue;
            emit("");
            emit(fmt::format("{} object [{}], {}, {} rays, one thread:", scene_name, shape.name,
                             distribution_name[dist], end[dist] - begin[dist]));
            emit("");
            emit("| policy | node tests | leaf tests | sub-squares per leaf | hit fraction | tests after the "
                 "hit (node, leaf) | µs per ray | vs embree |");
            emit("|---|---|---|---|---|---|---|---|");
            double embree_us = mesh_cost[dist].us_per_ray();
            for (size_t p = 0; p < policies.size(); ++p) {
                const Cost &c = costs[p][dist];
                emit(fmt::format("| {} | {:.2f} | {:.3f} | {:.1f} | {:.3f} | {:.0f}%, {:.0f}% | {:.2f} | {:.2f} |",
                                 policies[p].name, c.node_tests(), c.leaf_tests(), c.sub_squares(), c.hit_fraction(),
                                 100.0 * c.after_hit(c.stats.node_tests_after_hit, c.stats.node_tests),
                                 100.0 * c.after_hit(c.stats.leaf_tests_after_hit, c.stats.leaf_tests), c.us_per_ray(),
                                 embree_us > 0.0 ? c.us_per_ray() / embree_us : 0.0));
            }
            emit(fmt::format("| embree, texel-aligned mesh m = 1 | | | | {:.3f} | | {:.2f} | 1 |",
                             mesh_cost[dist].hit_fraction(), embree_us));
        }
        {
            emit("");
            emit(fmt::format("{} object [{}], node tests per ray by level, all {} rays:", scene_name, shape.name,
                             n_total));
            emit("");
            std::string head = "| policy |", rule = "|---|";
            for (int l = 0; l <= L; ++l) {
                head += fmt::format(" {} |", l);
                rule += "---|";
            }
            emit(head + " total |");
            emit(rule + "---|");
            for (size_t p = 0; p < policies.size(); ++p) {
                if (!newton_policy(policies[p]))
                    continue;
                std::string row = "| " + policies[p].name + " |";
                int64_t total = 0;
                for (int l = 0; l <= L; ++l) {
                    int64_t n = costs[p][0].stats.node_tests_per_level[l] + costs[p][1].stats.node_tests_per_level[l];
                    total += n;
                    row += fmt::format(" {:.2f} |", (double)n / (double)n_total);
                }
                emit(row + fmt::format(" {:.2f} |", (double)total / (double)n_total));
            }
        }

        // The node-test record along the box walk, and check (2).
        std::vector<dmap::NodeTestRecord> records;
        int stride = std::max(1, n_total / std::max(1, n_record_rays));
        int n_recorded = 0;
        for (int r = 0; r < n_total; r += stride) {
            dmap::TraversalStats s;
            s.records = &records;
            dmap::trace(obj, rays[r], policies[0].options, &s);
            ++n_recorded;
        }
        std::vector<LevelRecord> levels(L + 1);
        for (const dmap::NodeTestRecord &rec : records) {
            LevelRecord &lr = levels[std::min(rec.level, L)];
            ++lr.tests;
            if (rec.box_pass && !rec.slab_pass)
                ++lr.slab_prunes;
            if (!rec.box_pass && rec.slab_pass)
                ++lr.box_prunes;
            if (!rec.box_pass && !rec.slab_pass)
                ++lr.both_prune;
            if (rec.box_length >= 0.0 && rec.slab_length >= 0.0 && rec.both_length >= 0.0) {
                lr.box.push_back(rec.box_length);
                lr.slab.push_back(rec.slab_length);
                lr.both.push_back(rec.both_length);
                if (rec.slab_length < rec.box_length)
                    ++lr.slab_shorter;
            }
        }
        emit("");
        emit(fmt::format("{} object [{}], the box walk on {} rays (every {}th), the box and the slab at every "
                         "node test:",
                         scene_name, shape.name, n_recorded, stride));
        emit("");
        emit("| level | tests | slab prunes, box passes | box prunes, slab passes | both prune | median box | "
             "median slab | median both | slab/box | slab<box |");
        emit("|---|---|---|---|---|---|---|---|---|---|");
        int crossover = -1, pruning_crossover = -1;
        for (int l = 0; l <= L; ++l) {
            const LevelRecord &lr = levels[l];
            double mb = median(lr.box), ms = median(lr.slab), ma = median(lr.both);
            emit(fmt::format("| {} | {} | {} | {} | {} | {:.3e} | {:.3e} | {:.3e} | {:.2f} | {:.2f} |", l, lr.tests,
                             lr.slab_prunes, lr.box_prunes, lr.both_prune, mb, ms, ma, mb > 0.0 ? ms / mb : 0.0,
                             lr.box.empty() ? 0.0 : (double)lr.slab_shorter / (double)lr.box.size()));
            if (!lr.box.empty() && ms < mb && crossover == l - 1)
                crossover = l;
            if (lr.slab_prunes > lr.box_prunes && pruning_crossover == l - 1)
                pruning_crossover = l;
        }
        log.info("(2) crossover level by T3's rule (the slab's median interval shorter at and below): {}{}; the "
                 "slab prunes more nodes than the box at and below level {}{}",
                 crossover, crossover < 0 ? " (never)" : "", pruning_crossover,
                 pruning_crossover < 0 ? " (never)" : "");
        if (expected_crossover != -100) {
            log.info("    expected {}: {}", expected_crossover, crossover == expected_crossover ? "match" : "MISMATCH");
            pass = pass && crossover == expected_crossover;
        }

        // Memory.
        size_t grid_bytes = d.asset->nodes.values.size() * sizeof(double);
        emit("");
        emit(fmt::format("{} object [{}], memory: pyramid {:.2f} MB in double ({:.2f} MB as float), node grid "
                         "{:.2f} MB, BVH over {} triangle boxes {:.2f} MB; mesh {} faces, arrays {:.2f} MB, "
                         "BVH {:.2f} MB (peak {:.2f} MB)",
                         scene_name, shape.name, megabytes((double)pyramid_bytes(pyramid)),
                         0.5 * megabytes((double)pyramid_bytes(pyramid)), megabytes((double)grid_bytes),
                         obj.triangles.size(), megabytes((double)object_memory.current.load()), mesh->mesh.tri_count(),
                         megabytes((double)mesh->array_bytes()), megabytes((double)mesh_memory.current.load()),
                         megabytes((double)mesh_memory.peak.load())));

        // (3) The fastest policy over all rays, and the ratio to embree.
        auto seconds_all = [&](size_t p) { return costs[p][0].seconds + costs[p][1].seconds; };
        size_t best = scene_policy;
        for (size_t p = 0; p < policies.size(); ++p)
            if (newton_policy(policies[p]) && seconds_all(p) < seconds_all(best))
                best = p;
        double mesh_seconds = mesh_cost[0].seconds + mesh_cost[1].seconds;
        double scene_us = 1e6 * seconds_all(scene_policy) / n_total, best_us = 1e6 * seconds_all(best) / n_total;
        double mesh_us = 1e6 * mesh_seconds / n_total, box_us = 1e6 * seconds_all(0) / n_total;
        log.info("(3) fastest policy over all rays: {} at {:.2f} µs per ray; scene options ({}) at {:.2f} µs "
                 "({:+.1f}%, tolerance {:.0f}%); box (TFDM) {:.2f} µs; embree over the mesh {:.2f} µs; ratios to "
                 "embree: scene options {:.2f}, box {:.2f}",
                 policies[best].name, best_us, policies[scene_policy].name, scene_us,
                 100.0 * (scene_us / best_us - 1.0), 100.0 * time_tolerance, box_us, mesh_us, scene_us / mesh_us,
                 box_us / mesh_us);
        pass = pass && scene_us <= (1.0 + time_tolerance) * best_us;
    }

    log.info("VERDICT: {} — traversal cost on {}", pass ? "PASS" : "FAIL", scene_name);
}
