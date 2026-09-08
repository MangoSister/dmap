// Task validate_node_bounds ("Plan — Path tracer with displaced surfaces",
// T3): the per-node tests of the traversal on the displaced objects of a
// scene. For sampled triangles, for every cell of every level from the
// footprint's roots to the leaves:
// (1) dense surface samples in the cell satisfy the box and the slab
//     inequalities, and every sample of the triangle lies in its
//     object-space AABB; 0 violations;
// (2) for random rays through those samples, the box, slab and both
//     intervals contain the sample's t; 0 violations;
// (3) a tightness table per level: the median interval length of box,
//     slab and both in object units, and the crossover level, the
//     coarsest level at and below which the slab is the shorter interval.
// Verdict: conservative, and the crossover reported (checked against
// expected_crossover_level when given).
//
// Config: scene, n_triangles (0 = all), samples_per_cell (16), seed,
// tolerance (1e-9), expected_crossover_level (optional).

#include "descent_sampler.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include "ks/rng.h"
#include "node_bounds.h"
#include "scene_spec.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

using namespace ks;

namespace
{

struct LevelStats
{
    int64_t cells = 0, samples = 0, rays = 0, slab_shorter = 0;
    std::vector<double> box, slab, both;
};

double median(std::vector<double> v)
{
    if (v.empty())
        return NAN;
    size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    return v[mid];
}

vec3d uniform_direction(RNG &rng)
{
    double z = 1.0 - 2.0 * rng.next();
    constexpr double pi = 3.14159265358979323846;
    double phi = 2.0 * pi * rng.next();
    double s = std::sqrt(std::max(0.0, 1.0 - z * z));
    return vec3d(s * std::cos(phi), s * std::sin(phi), z);
}

} // namespace

void validate_node_bounds(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    std::string scene_name = args.load_string("scene");
    const dmap::SceneSpec *spec = args.asset_table().get<dmap::SceneSpec>(scene_name);
    ASSERT(spec, "no scene named [%s]", scene_name.c_str());
    spec->require_valid();
    int n_triangles = args.load_integer("n_triangles", 0);
    int samples_per_cell = args.load_integer("samples_per_cell", 16);
    int seed = args.load_integer("seed", 2027);
    double tolerance = (double)args.load_float("tolerance", 1e-9f);
    int expected_crossover = args.load_integer("expected_crossover_level", -100);

    RNG rng(seed);
    int64_t bad_box_point = 0, bad_slab_point = 0, bad_slab_exact = 0, bad_tri_aabb = 0;
    int64_t bad_box_ray = 0, bad_slab_ray = 0, bad_both_ray = 0;
    int64_t n_points = 0, n_rays = 0, n_triangles_done = 0;
    std::map<int, LevelStats> levels;

    for (const dmap::DisplacedObject &d : spec->displaced) {
        const dmap::TaylorPyramid &pyramid = *d.asset->pyramid;
        int n = (int)d.triangles.size();
        int count = n_triangles <= 0 ? n : std::min(n, n_triangles);
        for (int k = 0; k < count; ++k) {
            int t = count == n ? k : (int)(rng.next() * n) % n;
            const dmap::BaseTriangle &tri = d.triangles[t];
            dmap::TangentFrame frame = dmap::tangent_frame(tri);
            dmap::UvTriangle domain(tri.t0, tri.t1, tri.t2);
            double scale = dmap::mean_edge(tri);
            double h_min, h_max;
            dmap::triangle_height_range(tri, pyramid, h_min, h_max);
            dmap::Aabb3 tri_box = dmap::triangle_aabb(tri, h_min, h_max);
            ++n_triangles_done;

            int root_level;
            int64_t root_i0, root_j0;
            dmap::footprint_roots(domain, pyramid.n_leaf, root_level, root_i0, root_j0);
            for (int level = root_level; level >= 0; --level) {
                double w = pyramid.cell_width(level);
                int64_t i_lo = (int64_t)std::floor(domain.lo[0] / w), i_hi = (int64_t)std::ceil(domain.hi[0] / w) - 1;
                int64_t j_lo = (int64_t)std::floor(domain.lo[1] / w), j_hi = (int64_t)std::ceil(domain.hi[1] / w) - 1;
                LevelStats &stats = levels[level];
                for (int64_t j = j_lo; j <= j_hi; ++j) {
                    for (int64_t i = i_lo; i <= i_hi; ++i) {
                        vec2d c((i + 0.5) * w, (j + 0.5) * w);
                        if (dmap::classify_square(domain, c, 0.5 * w) == dmap::Overlap::Outside)
                            continue;
                        dmap::NodeBound nb = dmap::node_bound(tri, frame, pyramid, level, i, j);
                        dmap::SlabForm slab = dmap::slab_form(nb);
                        ++stats.cells;
                        double box_scale = (nb.box.hi - nb.box.lo).norm() + scale;

                        int found = 0;
                        for (int tries = 0; tries < 8 * samples_per_cell && found < samples_per_cell; ++tries) {
                            double u = (i + rng.next()) * w, v = (j + rng.next()) * w;
                            if (!domain.inside(vec2d(u, v)))
                                continue;
                            ++found;
                            ++n_points;
                            ++stats.samples;

                            // (1) the point.
                            dmap::PointwiseFields f = dmap::pointwise_fields(tri, d.field, u, v);
                            vec3d N_hat = dmap::normal_frame_at(tri, u, v).N;
                            vec3d S = tri.P(u, v) + f.h * N_hat;
                            vec3d p = frame.point(S);
                            if (!nb.box.contains(p, tolerance * box_scale))
                                ++bad_box_point;
                            dmap::Interval F = slab.eval(p);
                            double slab_tol = tolerance * (std::abs(slab.R) + F.magnitude() + box_scale);
                            if (F.lo > slab.R + slab_tol || F.hi < -slab.R - slab_tol)
                                ++bad_slab_point;
                            // The identity with the exact normal image: F = m_z ρ, |ρ| <= r.
                            vec3d m = frame.to_tangent * N_hat;
                            const dmap::TaylorNode &node = nb.node;
                            double F_exact =
                                p[2] * (1.0 + node.gu * m[0] + node.gv * m[1]) -
                                m[2] * (node.h0 + node.gu * (p[0] - nb.centre[0]) + node.gv * (p[1] - nb.centre[1]));
                            if (std::abs(F_exact) > std::abs(m[2]) * node.r + slab_tol)
                                ++bad_slab_exact;
                            if (!tri_box.contains(S, tolerance * scale))
                                ++bad_tri_aabb;

                            // (2) a ray through the point, from a random origin.
                            vec3d dir = uniform_direction(rng);
                            double t_s = scale * std::pow(10.0, -1.5 + 2.0 * rng.next());
                            vec3d o = S - t_s * dir;
                            dmap::TangentRay ray{frame.point(o), frame.direction(dir), 0.0, 1e3 * t_s};
                            ++n_rays;
                            ++stats.rays;
                            double t_tol = tolerance * t_s;
                            double b0, b1, s0, s1, a0, a1;
                            bool hit_box = dmap::box_interval(nb, ray, b0, b1);
                            bool hit_slab = dmap::slab_interval(nb, ray, s0, s1);
                            bool hit_both = dmap::both_interval(nb, ray, a0, a1);
                            if (!hit_box || t_s < b0 - t_tol || t_s > b1 + t_tol)
                                ++bad_box_ray;
                            if (!hit_slab || t_s < s0 - t_tol || t_s > s1 + t_tol)
                                ++bad_slab_ray;
                            if (!hit_both || t_s < a0 - t_tol || t_s > a1 + t_tol)
                                ++bad_both_ray;
                            if (hit_box && hit_slab && hit_both) {
                                stats.box.push_back(b1 - b0);
                                stats.slab.push_back(s1 - s0);
                                stats.both.push_back(a1 - a0);
                                if (s1 - s0 < b1 - b0)
                                    ++stats.slab_shorter;
                            }
                        }
                    }
                }
            }
        }
    }

    // (3) the tightness table and the crossover.
    get_default_logger().info("validate_node_bounds on {}: {} triangles, {} points, {} rays", scene_name,
                              n_triangles_done, n_points, n_rays);
    get_default_logger().info("{:>5s} {:>7s} {:>8s} {:>8s} {:>12s} {:>12s} {:>12s} {:>9s} {:>12s}", "level", "cells",
                              "samples", "rays", "median box", "median slab", "median both", "slab/box", "slab<box");
    std::map<int, double> ratio;
    for (auto &[level, stats] : levels) {
        double mb = median(stats.box), ms = median(stats.slab), ma = median(stats.both);
        ratio[level] = ms / mb;
        get_default_logger().info("{:5d} {:7d} {:8d} {:8d} {:12.4e} {:12.4e} {:12.4e} {:9.3f} {:12.3f}", level,
                                  stats.cells, stats.samples, stats.rays, mb, ms, ma, ms / mb,
                                  stats.rays ? (double)stats.slab_shorter / (double)stats.box.size() : 0.0);
    }
    int crossover = -1;
    for (auto &[level, r] : ratio) {
        if (r < 1.0 && level == crossover + 1)
            crossover = level;
        else
            break;
    }
    int64_t n_bad =
        bad_box_point + bad_slab_point + bad_slab_exact + bad_tri_aabb + bad_box_ray + bad_slab_ray + bad_both_ray;
    get_default_logger().info("(1) point violations: box {}, slab {}, slab exact identity {}, triangle AABB {}",
                              bad_box_point, bad_slab_point, bad_slab_exact, bad_tri_aabb);
    get_default_logger().info("(2) ray violations: box {}, slab {}, both {}", bad_box_ray, bad_slab_ray, bad_both_ray);
    get_default_logger().info("(3) crossover level (slab shorter at and below): {}{}", crossover,
                              crossover < 0 ? " (never)" : "");
    bool pass = n_bad == 0;
    if (expected_crossover != -100) {
        bool ok = crossover == expected_crossover;
        get_default_logger().info("    expected crossover level {}: {}", expected_crossover, ok ? "yes" : "NO");
        pass = pass && ok;
    }
    get_default_logger().info(
        "VERDICT: {} — node bounds on {}: {} violations in {} points and {} rays; crossover level {}",
        pass ? "PASS" : "FAIL", scene_name, n_bad, n_points, n_rays, crossover);
}
