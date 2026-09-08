// Task validate_traversal ("Plan — Path tracer with displaced surfaces",
// T4): the traversal and the leaf intersectors on the displaced objects of
// a scene, in object space, over two ray distributions: camera-like rays
// from the scene's camera that reach the object's bounds, and rays from
// random origins near the surface in random directions.
// (1) On identity-chart objects, the two-triangle leaf against embree over
//     the pre-tessellated mesh at texel resolution (D4): hit and miss
//     agreement and t within rounding. A disagreement counts only when it
//     cannot be rounding: the nearer hit is farther than edge_margin (in
//     barycentric units) from every micro-triangle edge and from the domain
//     boundary, and its incidence is not grazing.
// (2) The Newton leaf: residual |S(s, t) - ray(t)| below twice the
//     intersector's tolerance, (s, t) in the leaf texel and in the domain;
//     and agreement with embree over the texel-aligned mesh at
//     fine_subdivision sub-cells per texel within its chord error, measured
//     per base triangle at the micro-edge midpoints and centroids of its
//     interior faces. The two surfaces may legitimately differ where the
//     nearer hit is grazing, where either hit lies within that error of the
//     domain boundary (its sub-cell straddles the boundary, or its distance
//     to it is below twice the error over the incidence cosine), and where the
//     nearer hit lies within that error of the ray origin; those rays are
//     counted and skipped.
// (3) box, slab, both and every level policy give the same hits as the
//     scene's options, t within the Newton tolerance.
// (4) Never-miss: rays aimed at sampled surface points hit at or before
//     that distance, the Newton leaf on the true surface, the two-triangle
//     leaf on its own surface.
// Verdict: PASS only if all four hold.
//
// Config: scene, n_rays (per distribution, 50000), n_mode_rays (per
// distribution, 20000), seed, fine_subdivision (4; 0 skips the mesh
// comparison of (2)), edge_margin (1e-3).

#include "displaced_intersector.h"
#include "displaced_tessellation.h"
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
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace ks;

namespace
{

double domain_margin(const dmap::BaseTriangle &tri, const vec2d &st)
{
    vec2d b = tri.bary(st[0], st[1]);
    return std::min({1.0 - b[0] - b[1], b[0], b[1]});
}

// Distance of (s, t) to the nearest micro-triangle edge of its leaf, in
// barycentric units.
double micro_margin(const dmap::DisplacedIntersector &ti, const vec2d &st)
{
    double w = ti.pyramid->cell_width(0);
    double a = st[0] / w - std::floor(st[0] / w), b = st[1] / w - std::floor(st[1] / w);
    if (a + b <= 1.0)
        return std::min({a, b, 1.0 - a - b});
    return std::min({1.0 - a, 1.0 - b, a + b - 1.0});
}

// Distance of the base point at (s, t) to the domain boundary, object
// units: the least barycentric weight times its altitude.
double boundary_distance(const dmap::BaseTriangle &tri, const vec2d &st)
{
    vec2d b = tri.bary(st[0], st[1]);
    double weight[3] = {1.0 - b[0] - b[1], b[0], b[1]};
    const vec3d p[3] = {tri.p0, tri.p1, tri.p2};
    double twice_area = (tri.p1 - tri.p0).cross(tri.p2 - tri.p0).norm();
    double best = INFINITY;
    for (int k = 0; k < 3; ++k) {
        double edge = (p[(k + 2) % 3] - p[(k + 1) % 3]).norm();
        best = std::min(best, weight[k] * twice_area / edge);
    }
    return best;
}

// Whether the sub-cell of the texel-aligned mesh (m per texel) under
// (s, t) straddles the domain boundary.
bool sub_cell_straddles(const dmap::DisplacedIntersector &ti, const vec2d &st, int m)
{
    double w = ti.pyramid->cell_width(0) / m;
    vec2d c((std::floor(st[0] / w) + 0.5) * w, (std::floor(st[1] / w) + 0.5) * w);
    return dmap::classify_square(ti.domain, c, 0.5 * w) != dmap::Overlap::Inside;
}

double incidence(const vec3d &d, const vec3d &ng) { return std::abs(d.normalized().dot(ng)); }

} // namespace

void validate_traversal(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    std::string scene_name = args.load_string("scene");
    const dmap::SceneSpec *spec = args.asset_table().get<dmap::SceneSpec>(scene_name);
    ASSERT(spec, "no scene named [%s]", scene_name.c_str());
    spec->require_valid();
    int n_rays = args.load_integer("n_rays", 50000);
    int n_mode_rays = args.load_integer("n_mode_rays", 20000);
    int seed = args.load_integer("seed", 2027);
    int fine_subdivision = args.load_integer("fine_subdivision", 4);
    double edge_margin = (double)args.load_float("edge_margin", 1e-3f);
    // Tolerances of "t within rounding": the double traversal against the
    // float mesh, the double traversal against itself.
    constexpr double float_rounding = 1e-5, double_rounding = 1e-12;
    constexpr double grazing = 0.1; // incidence cosine below which check (2) skips a ray

    auto &log = get_default_logger();
    EmbreeDevice device;
    bool pass = true;

    for (size_t obj_index = 0; obj_index < spec->displaced.size(); ++obj_index) {
        const dmap::DisplacedObject &d = spec->displaced[obj_index];
        const dmap::TaylorPyramid &pyramid = *d.asset->pyramid;
        int n_leaf = pyramid.n_leaf;
        double w_leaf = pyramid.cell_width(0);
        dmap::ObjectIntersector obj(d.triangles, pyramid, d.field);
        int n_tri = (int)d.triangles.size();
        dmap::ObjectSummary summary = dmap::summarize_object(d, obj);
        const std::vector<double> &edges = summary.edges;
        int max_root = summary.max_root_level;
        bool identity = summary.identity;
        double scale = (obj.bound.hi - obj.bound.lo).norm();
        dmap::log_object("validate_traversal", scene_name, *spec, d, summary);

        // The two ray distributions, in object space.
        RNG rng((uint64_t)seed + 17 * obj_index);
        dmap::ObjectRays distributions = dmap::object_rays(*spec, d, obj, summary, n_rays, rng);
        std::vector<dmap::RayQuery> &rays = distributions.rays;
        int n_camera = distributions.n_camera;
        int n_total = (int)rays.size();
        log.info("  rays: {} camera, {} near the surface", n_camera, n_total - n_camera);

        dmap::TraversalOptions newton = d.traversal, two_triangle = d.traversal;
        newton.leaf = dmap::LeafMode::Newton;
        two_triangle.leaf = dmap::LeafMode::TwoTriangle;

        // (1) The two-triangle leaf against embree over the matched mesh.
        if (identity) {
            std::unique_ptr<dmap::MeshOracle> oracle = dmap::MeshOracle::barycentric(device, *spec, d, n_leaf);
            std::vector<dmap::TraceResult> ours(n_total);
            std::vector<dmap::MeshOracle::Hit> theirs(n_total);
            parallel_for(n_total, [&](int r) {
                ours[r] = dmap::trace(obj, rays[r], two_triangle);
                theirs[r] = oracle->intersect(rays[r]);
            });
            int64_t agree_hit = 0, agree_miss = 0, explained = 0, unexplained = 0, t_bad = 0;
            double worst_dt = 0.0;
            for (int r = 0; r < n_total; ++r) {
                const dmap::TraceResult &a = ours[r];
                const dmap::MeshOracle::Hit &b = theirs[r];
                if (!a.hit && !b.hit) {
                    ++agree_miss;
                    continue;
                }
                bool disagree = a.hit != b.hit;
                if (a.hit && b.hit) {
                    double cos_a = incidence(rays[r].d, a.h.ng);
                    double dt = std::abs(a.h.t - b.t) * std::max(cos_a, 1e-2) / (a.h.t + scale);
                    worst_dt = std::max(worst_dt, dt);
                    disagree = dt > float_rounding;
                }
                if (!disagree) {
                    ++agree_hit;
                    continue;
                }
                // The nearer hit decides whether rounding can explain it.
                bool ours_nearer = a.hit && (!b.hit || a.h.t <= b.t);
                double margin, cos_hit;
                if (ours_nearer) {
                    const dmap::DisplacedIntersector &ti = obj.triangles[a.triangle];
                    margin = std::min(micro_margin(ti, a.h.st), domain_margin(*ti.tri, a.h.st));
                    cos_hit = incidence(rays[r].d, a.h.ng);
                } else {
                    margin = std::min({b.u, b.v, 1.0 - b.u - b.v});
                    cos_hit = incidence(rays[r].d, b.ng);
                }
                if (margin < edge_margin || cos_hit < 1e-3) {
                    ++explained;
                } else {
                    ++unexplained;
                    if (a.hit && b.hit)
                        ++t_bad;
                    if (unexplained <= 5)
                        log.info("    disagreement: ours {} t={:.9g}, embree {} t={:.9g}, margin {:.2e}, cos {:.3f}",
                                 a.hit ? "hit" : "miss", a.hit ? a.h.t : 0.0, b.hit ? "hit" : "miss", b.hit ? b.t : 0.0,
                                 margin, cos_hit);
                }
            }
            log.info("(1) two-triangle leaf vs embree at {} texels: {} hits and {} misses agree, {} rounding cases "
                     "(edge margin < {:.0e} or grazing), {} unexplained ({} in t); worst normalized |dt| {:.2e} "
                     "(tolerance {:.0e})",
                     n_leaf, agree_hit, agree_miss, explained, edge_margin, unexplained, t_bad, worst_dt,
                     float_rounding);
            pass = pass && unexplained == 0;
        } else {
            log.info("(1) skipped: not an identity-chart object");
        }

        // (2) The Newton leaf: residual, leaf and domain membership, and
        // the texel-aligned fine mesh.
        {
            std::vector<dmap::TraceResult> ours(n_total);
            parallel_for(n_total, [&](int r) { ours[r] = dmap::trace(obj, rays[r], newton); });
            int64_t hits = 0, bad_residual = 0, bad_leaf = 0, bad_domain = 0;
            double worst_residual = 0.0;
            for (int r = 0; r < n_total; ++r) {
                const dmap::TraceResult &a = ours[r];
                if (!a.hit)
                    continue;
                ++hits;
                const dmap::DisplacedIntersector &ti = obj.triangles[a.triangle];
                vec3d S = dmap::displaced_position(*ti.tri, d.field, a.h.st[0], a.h.st[1]);
                double residual = (S - (rays[r].o + a.h.t * rays[r].d)).norm() / ti.residual_tolerance;
                worst_residual = std::max(worst_residual, residual);
                if (residual > 2.0)
                    ++bad_residual;
                vec2d lo(a.h.i * w_leaf, a.h.j * w_leaf), hi((a.h.i + 1) * w_leaf, (a.h.j + 1) * w_leaf);
                if (a.h.st[0] < lo[0] || a.h.st[0] > hi[0] || a.h.st[1] < lo[1] || a.h.st[1] > hi[1])
                    ++bad_leaf;
                if (!ti.domain.inside(a.h.st))
                    ++bad_domain;
            }
            log.info("(2) Newton leaf: {} hits of {} rays; residual over tolerance: worst {:.3f}, {} above 2; "
                     "{} outside the leaf, {} outside the domain",
                     hits, n_total, worst_residual, bad_residual, bad_leaf, bad_domain);
            pass = pass && bad_residual == 0 && bad_leaf == 0 && bad_domain == 0;

            if (fine_subdivision > 0) {
                std::unique_ptr<dmap::MeshOracle> oracle =
                    dmap::MeshOracle::texel_aligned(device, *spec, d, fine_subdivision);
                std::vector<double> eps = oracle->chord_errors(d);
                std::vector<double> eps_sorted = eps;
                std::nth_element(eps_sorted.begin(), eps_sorted.begin() + n_tri / 2, eps_sorted.end());
                double eps_max = *std::max_element(eps.begin(), eps.end());
                std::vector<dmap::MeshOracle::Hit> theirs(n_total);
                parallel_for(n_total, [&](int r) { theirs[r] = oracle->intersect(rays[r]); });
                int64_t agree = 0, agree_miss = 0, skipped_grazing = 0, skipped_boundary = 0, skipped_origin = 0,
                        unexplained = 0, t_bad = 0;
                double worst_ratio = 0.0;
                for (int r = 0; r < n_total; ++r) {
                    const dmap::TraceResult &a = ours[r];
                    const dmap::MeshOracle::Hit &b = theirs[r];
                    if (!a.hit && !b.hit) {
                        ++agree_miss;
                        continue;
                    }
                    double cos_a = a.hit ? incidence(rays[r].d, a.h.ng) : 0.0;
                    double cos_b = b.hit ? incidence(rays[r].d, b.ng) : 0.0;
                    // The nearer hit decides: a grazing crossing of either
                    // surface need not exist on the other.
                    bool a_nearer = a.hit && (!b.hit || a.h.t <= b.t);
                    bool graze = (a_nearer ? cos_a : cos_b) < grazing;
                    // Within the band where the two surfaces' boundaries can
                    // differ: the leaf straddles the domain boundary, or the
                    // hit is closer to it than the chord error allows.
                    auto near_boundary = [&](int k, const vec2d &st, double cos_hit) {
                        return sub_cell_straddles(obj.triangles[k], st, fine_subdivision) ||
                               boundary_distance(d.triangles[k], st) < 2.0 * eps[k] / std::max(cos_hit, grazing);
                    };
                    bool boundary = (a.hit && near_boundary(a.triangle, a.h.st, cos_a)) ||
                                    (b.hit && near_boundary(b.triangle, b.st, cos_b));
                    // The origin within the chord error of the surface: the
                    // other surface's crossing may lie behind it.
                    bool near_origin = a_nearer ? a.h.t < 2.0 * eps[a.triangle] / std::max(cos_a, grazing)
                                                : b.t < 2.0 * eps[b.triangle] / std::max(cos_b, grazing);
                    if (a.hit && b.hit) {
                        double e = std::max(eps[a.triangle], eps[b.triangle]);
                        double tol = 2.0 * e / std::max(cos_a, grazing) + float_rounding * (a.h.t + scale);
                        double dt = std::abs(a.h.t - b.t);
                        if (!boundary && !graze && !near_origin)
                            worst_ratio = std::max(worst_ratio, dt / tol);
                        if (dt <= tol) {
                            ++agree;
                            continue;
                        }
                    }
                    if (boundary) {
                        ++skipped_boundary;
                    } else if (near_origin) {
                        ++skipped_origin;
                    } else if (graze) {
                        ++skipped_grazing;
                    } else {
                        ++unexplained;
                        if (a.hit && b.hit)
                            ++t_bad;
                        if (unexplained <= 5) {
                            log.info("    disagreement: newton {} t={:.9g} cos {:.3f} (triangle {}, eps {:.2e}, "
                                     "boundary distance {:.2e}), mesh {} t={:.9g} cos {:.3f} (triangle {})",
                                     a.hit ? "hit" : "miss", a.hit ? a.h.t : 0.0, cos_a, a.triangle,
                                     a.hit ? eps[a.triangle] : 0.0,
                                     a.hit ? boundary_distance(d.triangles[a.triangle], a.h.st) : 0.0,
                                     b.hit ? "hit" : "miss", b.hit ? b.t : 0.0, cos_b, b.triangle);
                            if (a.hit)
                                log.info("      the mesh of the newton triangle alone: nearest t {:.9g}",
                                         oracle->nearest_on_triangle(rays[r], a.triangle));
                        }
                    }
                }
                log.info("    vs the texel-aligned mesh at {} sub-cells per texel (chord error median {:.2e}, up to "
                         "{:.2e}): {} hits and {} misses agree, {} skipped near the domain boundary, {} near the "
                         "origin, {} grazing (cos < {}), {} unexplained ({} in t); worst |dt| over tolerance {:.3f}",
                         fine_subdivision, eps_sorted[n_tri / 2], eps_max, agree, agree_miss, skipped_boundary,
                         skipped_origin, skipped_grazing, grazing, unexplained, t_bad, worst_ratio);
                pass = pass && unexplained == 0;
            }
        }

        // (3) Every node test and level policy gives the same hits.
        {
            std::vector<int> subset;
            for (int r = 0; r < std::min(n_camera, n_mode_rays); ++r)
                subset.push_back(r);
            for (int r = n_camera; r < std::min(n_total, n_camera + n_mode_rays); ++r)
                subset.push_back(r);
            int n_sub = (int)subset.size();
            std::vector<dmap::TraversalOptions> variants;
            std::vector<std::string> names;
            auto add = [&](dmap::BoundMode bound, int k, const std::string &name) {
                dmap::TraversalOptions v = d.traversal;
                v.bound = bound;
                v.slab_max_level = k;
                variants.push_back(v);
                names.push_back(name);
            };
            add(dmap::BoundMode::Box, max_root, "box");
            add(dmap::BoundMode::Slab, max_root, "slab");
            add(dmap::BoundMode::Both, max_root, "both");
            for (int k = 0; k < max_root; ++k)
                add(dmap::BoundMode::Both, k, "both k=" + std::to_string(k));

            std::vector<dmap::TraceResult> reference(n_sub);
            dmap::TraversalStats ref_stats;
            {
                std::vector<dmap::TraversalStats> per_ray(n_sub);
                parallel_for(
                    n_sub, [&](int r) { reference[r] = dmap::trace(obj, rays[subset[r]], d.traversal, &per_ray[r]); });
                for (auto &s : per_ray)
                    ref_stats.add(s);
            }
            log.info("(3) node tests per ray, {} rays: {:>14s} {:10.2f} node tests, {:8.3f} leaf tests (reference; "
                     "{:.1f} sub-squares per leaf test)",
                     n_sub, dmap::describe(d.traversal), (double)ref_stats.node_tests / n_sub,
                     (double)ref_stats.leaf_tests / n_sub,
                     ref_stats.leaf_tests ? (double)ref_stats.sub_squares / ref_stats.leaf_tests : 0.0);
            int64_t mismatches = 0;
            double worst_dt = 0.0;
            for (size_t v = 0; v < variants.size(); ++v) {
                std::vector<dmap::TraceResult> got(n_sub);
                std::vector<dmap::TraversalStats> per_ray(n_sub);
                parallel_for(n_sub,
                             [&](int r) { got[r] = dmap::trace(obj, rays[subset[r]], variants[v], &per_ray[r]); });
                dmap::TraversalStats stats;
                for (auto &s : per_ray)
                    stats.add(s);
                int64_t bad_hit = 0, bad_t = 0;
                for (int r = 0; r < n_sub; ++r) {
                    const dmap::TraceResult &a = reference[r], &b = got[r];
                    if (a.hit != b.hit) {
                        ++bad_hit;
                        continue;
                    }
                    if (!a.hit)
                        continue;
                    // Newton leaves agree to their residual tolerance.
                    double tol = 4.0 * obj.triangles[a.triangle].residual_tolerance + double_rounding * (a.h.t + scale);
                    double dt = std::abs(a.h.t - b.h.t);
                    worst_dt = std::max(worst_dt, dt / tol);
                    if (dt > tol)
                        ++bad_t;
                }
                mismatches += bad_hit + bad_t;
                log.info("    {:>14s} {:10.2f} node tests, {:8.3f} leaf tests, {} hit/miss and {} t mismatches",
                         names[v], (double)stats.node_tests / n_sub, (double)stats.leaf_tests / n_sub, bad_hit, bad_t);
            }
            log.info("    {} mismatches over {} variants; worst |dt| over tolerance {:.3f}", mismatches,
                     variants.size(), worst_dt);
            pass = pass && mismatches == 0;
        }

        // (4) Never-miss on each leaf's own surface.
        for (int mode = 0; mode < 2; ++mode) {
            const dmap::TraversalOptions &options = mode == 0 ? newton : two_triangle;
            std::vector<dmap::RayQuery> aimed(n_rays);
            std::vector<double> t_s(n_rays);
            std::vector<int> aimed_triangle(n_rays);
            std::vector<vec2d> aimed_st(n_rays);
            for (int k = 0; k < n_rays; ++k) {
                int t = (int)(rng.next() * n_tri) % n_tri;
                vec2d b = dmap::uniform_bary(rng);
                vec2d st = d.triangles[t].param(b[0], b[1]);
                aimed_triangle[k] = t;
                aimed_st[k] = st;
                vec3d S = mode == 0 ? dmap::displaced_position(d.triangles[t], d.field, st[0], st[1])
                                    : obj.triangles[t].two_triangle_position(st[0], st[1]);
                vec3d dir = dmap::uniform_direction(rng);
                t_s[k] = edges[t] * std::pow(10.0, -1.5 + 2.0 * rng.next());
                aimed[k] = dmap::RayQuery{S - t_s[k] * dir, dir};
            }
            std::vector<dmap::TraceResult> got(n_rays);
            parallel_for(n_rays, [&](int r) { got[r] = dmap::trace(obj, aimed[r], options); });
            int64_t misses = 0, farther = 0;
            for (int k = 0; k < n_rays; ++k) {
                // The Newton root sits within the residual tolerance of the
                // ray, which along a ray with incidence cosine c on the
                // surface is a shift of up to tolerance / c.
                double cos_hit = got[k].hit ? incidence(aimed[k].d, got[k].h.ng) : 1.0;
                double tol =
                    1e-9 * (t_s[k] + scale) +
                    (got[k].hit ? 4.0 * obj.triangles[got[k].triangle].residual_tolerance / std::max(cos_hit, 1e-3)
                                : 0.0);
                bool miss = !got[k].hit, far = !miss && got[k].h.t > t_s[k] + tol;
                if (miss)
                    ++misses;
                if (far)
                    ++farther;
                if ((miss || far) && misses + farther <= 4) {
                    // Diagnose: the aimed point's leaf on its own, and the
                    // node tests along the ray at every level above it.
                    const dmap::RayQuery &q = aimed[k];
                    const dmap::DisplacedIntersector &ti = obj.triangles[aimed_triangle[k]];
                    const vec2d &st = aimed_st[k];
                    int64_t li = (int64_t)std::floor(st[0] / w_leaf), lj = (int64_t)std::floor(st[1] / w_leaf);
                    log.info("    failure: {} t_s={:.6g} got t={:.6g} (triangle {}) st=({:.6f}, {:.6f}) leaf ({}, {})",
                             miss ? "miss" : "farther", t_s[k], miss ? 0.0 : got[k].h.t, got[k].triangle, st[0], st[1],
                             li, lj);
                    dmap::DisplacedHit h;
                    bool tt = ti.intersect_two_triangle(q.o, q.d, 0.0, INFINITY, li, lj, h);
                    log.info("      two-triangle on the leaf: {} t={:.6g}", tt, tt ? h.t : 0.0);
                    bool nw = ti.intersect_newton(q.o, q.d, 0.0, INFINITY, li, lj, h);
                    log.info("      newton on the leaf: {} t={:.6g}", nw, nw ? h.t : 0.0);
                    bool ex = ti.newton_from(q.o, q.d, 0.0, INFINITY, li, lj, st, h);
                    log.info("      newton from the exact point: {} t={:.6g}", ex, ex ? h.t : 0.0);
                    dmap::TangentRay tray{ti.frame.point(q.o), ti.frame.direction(q.d), 0.0, INFINITY};
                    for (int level = ti.root_level; level >= 0; --level) {
                        dmap::Cell c{0, li, lj};
                        for (int up = 0; up < level; ++up)
                            dmap::cell_up(c);
                        dmap::NodeBound nb = dmap::node_bound(*ti.tri, ti.frame, *ti.pyramid, level, c.i, c.j);
                        double b0, b1, s0, s1;
                        bool bb = dmap::box_interval(nb, tray, b0, b1), ss = dmap::slab_interval(nb, tray, s0, s1);
                        log.info("      level {} cell ({}, {}): box {} [{:.6g}, {:.6g}] slab {} [{:.6g}, {:.6g}]",
                                 level, c.i, c.j, bb, bb ? b0 : 0.0, bb ? b1 : 0.0, ss, ss ? s0 : 0.0, ss ? s1 : 0.0);
                    }
                }
            }
            log.info("(4) never-miss, {} leaf: {} rays at surface points, {} missed, {} hit farther than the point",
                     dmap::leaf_mode_name(options.leaf), n_rays, misses, farther);
            pass = pass && misses == 0 && farther == 0;
        }
    }

    log.info("VERDICT: {} — traversal on {}", pass ? "PASS" : "FAIL", scene_name);
}
