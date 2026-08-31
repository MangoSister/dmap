// The C++ half of "Result — Pyramid channel study". One question: what do
// the stored min-max height channel and the direct construction buy, and
// what do they cost, at renderer resolutions?
//
// Two things are being priced, and they are independent.
//
// The channel. A node bounds the height twice: as the Taylor slab, of
// thickness 2r = O(s^2) around a tilted plane, and as the axis-aligned box
// [h_min, h_max], of height O(s). Recovering the box from the slab costs
// the plane's excursion across the cell (note S5), which is why the channel
// is stored. Table A prices that recovery and shows where each bound is the
// smaller one: the slab at fine cells, the box at coarse cells, which is
// where a ray traversal starts.
//
// The construction. Direct enumeration gives the smallest r the chosen
// slope admits, while the fold compounds the remainders of its children.
// Table B prices that, and the cost table prices the build. The two
// constructions agree exactly on gu, gv, ru, rv, h_min and h_max, so the
// direct column of table A moves only through r: tighter construction alone
// does not close the recovery gap, which is the point.
//
// Correctness lives in validate_pyramid; this task measures.

#include "base_mesh.h"
#include "displaced_surface.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "taylor_pyramid.h"
#include "texture_grid.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

using namespace ks;

namespace
{

double now_ms()
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double median(std::vector<double> v)
{
    if (v.empty())
        return 0.0;
    size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    return v[mid];
}

double time_min(int reps, const std::function<void()> &fn)
{
    double best = 1e300;
    for (int k = 0; k < reps; ++k) {
        double t0 = now_ms();
        fn();
        best = std::min(best, now_ms() - t0);
    }
    return best;
}

} // namespace

void test_pyramid_channels(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path mesh_path = args.load_path("mesh");
    fs::path tex_path = args.load_path("texture");
    int tex_nodes = args.load_integer("tex_nodes", 257);
    int triangle_index = args.load_integer("triangle_index", -1);
    int n_reps = args.load_integer("n_reps", 10);

    std::vector<double> amplitudes;
    for (int k = 0; k < (int)args["amplitudes"].array_size(); ++k)
        amplitudes.push_back((double)args["amplitudes"].load_float(k));
    std::vector<int> tile_sizes;
    for (int k = 0; k < (int)args["tile_sizes"].array_size(); ++k)
        tile_sizes.push_back(args["tile_sizes"].load_integer(k));

    dmap::BaseMesh mesh = dmap::load_base_obj(mesh_path);
    dmap::TextureGrid full = dmap::load_height_texture(tex_path);
    dmap::TextureGrid tex = dmap::downsample_box(full, tex_nodes);
    if (triangle_index < 0)
        triangle_index = mesh.n_triangles() / 3;
    dmap::BaseTriangle tri = mesh.triangle(triangle_index);
    double me = dmap::mean_edge(tri);

    bool recovery_contains = true;   // [h_min, h_max] inside the recovered interval
    bool direct_never_looser = true; // r_direct <= r_fold everywhere
    bool slab_wins_at_leaf = true, box_wins_at_root = true;

    for (double amplitude : amplitudes) {
        double scale = amplitude * me;
        dmap::TaylorPyramid fold(tex.values.data(), tex.W, scale, dmap::PyramidBuild::Fold);
        dmap::TaylorPyramid direct(tex.values.data(), tex.W, scale, dmap::PyramidBuild::Direct);

        get_default_logger().info(
            "=== {} on {} triangle {}, amplitude {:.2f} x mean edge, {} leaf cells ===", tex_path.filename().string(),
            mesh_path.filename().string(), triangle_index, amplitude, fold.n_leaf);
        get_default_logger().info("  A. the two height bounds (lengths in mean-edge units)");
        get_default_logger().info("  {:>5} {:>10} {:>10} {:>9} {:>12} {:>14}", "cell", "box", "slab", "thinner",
                                  "rec/stored", "rec dir/stored");

        int crossover = -1; // finest cell size at which the box is the thinner bound
        for (int k = 0; k < fold.n_levels; ++k) {
            const dmap::TaylorLevel &f = fold.levels[k];
            const dmap::TaylorLevel &d = direct.levels[k];
            double s = fold.half_extent(k);
            std::vector<double> lo_f, hi_f, lo_d, hi_d;
            dmap::minmax_from_taylor(f, s, lo_f, hi_f);
            dmap::minmax_from_taylor(d, s, lo_d, hi_d);

            std::vector<double> box, slab, ratio_f, ratio_d;
            for (size_t q = 0; q < f.h0.size(); ++q) {
                double stored_half = 0.5 * (f.h_max[q] - f.h_min[q]);
                box.push_back((f.h_max[q] - f.h_min[q]) / me);
                slab.push_back(2.0 * f.r[q] / me);
                if (stored_half > 1e-12 * std::max(1.0, std::abs(f.h0[q]))) {
                    ratio_f.push_back((hi_f[q] - lo_f[q]) * 0.5 / stored_half);
                    ratio_d.push_back((hi_d[q] - lo_d[q]) * 0.5 / stored_half);
                }
                double slack = 1e-12 * (1.0 + std::abs(f.h0[q]) + f.r[q]);
                if (lo_f[q] > f.h_min[q] + slack || hi_f[q] < f.h_max[q] - slack || lo_d[q] > d.h_min[q] + slack ||
                    hi_d[q] < d.h_max[q] - slack)
                    recovery_contains = false;
                if (d.r[q] > f.r[q] + slack)
                    direct_never_looser = false;
            }
            double med_box = median(box), med_slab = median(slab);
            if (crossover < 0 && med_slab > med_box)
                crossover = 1 << k;
            if (k == 0 && med_slab >= med_box)
                slab_wins_at_leaf = false;
            if (k == fold.n_levels - 1 && med_slab <= med_box)
                box_wins_at_root = false;
            get_default_logger().info("  {:>5} {:>10.5f} {:>10.5f} {:>9} {:>12.2f} {:>14.2f}", 1 << k, med_box,
                                      med_slab, med_slab < med_box ? "slab" : "box", median(ratio_f), median(ratio_d));
        }
        if (crossover > 0)
            get_default_logger().info("  the box becomes the thinner bound at {}-texel cells", crossover);

        get_default_logger().info("  B. the two constructions (r in mean-edge units)");
        get_default_logger().info("  {:>5} {:>10} {:>10} {:>12} {:>12}", "cell", "r fold", "r direct", "fold/direct",
                                  "|h0 shift|");
        for (int k = 0; k < fold.n_levels; ++k) {
            const dmap::TaylorLevel &f = fold.levels[k];
            const dmap::TaylorLevel &d = direct.levels[k];
            std::vector<double> rf, rd, ratio, shift;
            for (size_t q = 0; q < f.h0.size(); ++q) {
                rf.push_back(f.r[q] / me);
                rd.push_back(d.r[q] / me);
                shift.push_back(std::abs(f.h0[q] - d.h0[q]) / me);
                if (d.r[q] > 0.0)
                    ratio.push_back(f.r[q] / d.r[q]);
            }
            get_default_logger().info("  {:>5} {:>10.5f} {:>10.5f} {:>12.2f} {:>12.5f}", 1 << k, median(rf), median(rd),
                                      median(ratio), median(shift));
        }
    }

    // Build cost and memory. Both are content-independent, so one amplitude
    // and one texture suffice; the tile size is what matters.
    get_default_logger().info("=== build cost and memory ===");
    get_default_logger().info("  {:>6} {:>11} {:>13} {:>13} {:>10} {:>10} {:>10}", "tile", "fold", "direct",
                              "direct/fold", "8-ch KB", "6-ch KB", "2-ch KB");
    for (int nodes : tile_sizes) {
        dmap::TextureGrid t = dmap::downsample_box(full, nodes);
        double t_fold =
            time_min(n_reps, [&]() { dmap::TaylorPyramid p(t.values.data(), t.W, 1.0, dmap::PyramidBuild::Fold); });
        double t_direct =
            time_min(n_reps, [&]() { dmap::TaylorPyramid p(t.values.data(), t.W, 1.0, dmap::PyramidBuild::Direct); });
        dmap::TaylorPyramid p(t.values.data(), t.W, 1.0);
        size_t cells = 0;
        for (const dmap::TaylorLevel &lvl : p.levels)
            cells += lvl.h0.size();
        get_default_logger().info("  {:>5}c {:>9.2f}ms {:>11.2f}ms {:>13.2f} {:>10} {:>10} {:>10}", nodes - 1, t_fold,
                                  t_direct, t_direct / std::max(t_fold, 1e-9), 8 * cells * sizeof(double) / 1024,
                                  6 * cells * sizeof(double) / 1024, 2 * cells * sizeof(double) / 1024);
    }

    bool pass = recovery_contains && direct_never_looser && slab_wins_at_leaf && box_wins_at_root;
    get_default_logger().info(
        "VERDICT: {} — both height bounds earn their place: the slab is the thinner one at texel cells and the "
        "min-max box at the root ({}, {}), and the recovered interval always contains the stored one ({}); direct "
        "construction never loosens r ({}) but leaves the recovery gap, since it cannot change the slope",
        pass ? "PASS" : "FAIL", slab_wins_at_leaf ? "ok" : "FAILED", box_wins_at_root ? "ok" : "FAILED",
        recovery_contains ? "ok" : "FAILED", direct_never_looser ? "ok" : "FAILED");
    if (!pass)
        std::exit(1);
}
