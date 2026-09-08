// Path tracer plan T1 validation: does the general chart reduce to the
// identity chart exactly, obey the chain rule, and clip correctly?
// (1) The general constructor with identity texture coordinates reproduces
//     the old parameter-space members bit for bit (the goldens themselves
//     are checked by validate_pointwise, and the S6 checks by
//     validate_descent, both run separately).
// (2) Under random charts with a linear height field, the identity-chart
//     triangle and the chart triangle describe the same surface: equal h,
//     equal displaced position, equal unit normal, equal stretch
//     eigenvalues, and sqrt(det G) scaled by |det J|, at every point.
//     Also the identity |Su x Sv| = sqrt(det G) on the chart triangle.
// (3) Clipping, on the mesh's own UV layouts and on random tiled charts:
//     exact clipped areas over the texel box sum to the domain area; the
//     overlap classification agrees with the exact area at every cell of
//     every level; on the identity chart the closed form equals the exact
//     clip; and a footprint's totals equal an independent per-texel sum.

#include "base_mesh.h"
#include "descent_sampler.h"
#include "displaced_surface.h"
#include "emission_tile.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "ks/rng.h"
#include "texture_grid.h"
#include "uv_clip.h"
#include <cmath>
#include <cstdlib>
#include <vector>

using namespace ks;

namespace
{

// A seeded triangle in the texture plane with bounded stretch, inside the
// unit tile; the identity chart when identity is set.
void random_chart(RNG &rng, vec2d t[3])
{
    for (;;) {
        for (int k = 0; k < 3; ++k)
            t[k] = vec2d(0.05 + 0.9 * rng.next(), 0.05 + 0.9 * rng.next());
        Eigen::Matrix2d T;
        T.col(0) = t[1] - t[0];
        T.col(1) = t[2] - t[0];
        double area = 0.5 * std::abs(T.determinant());
        Eigen::JacobiSVD<Eigen::Matrix2d> svd(T);
        double cond = svd.singularValues()[0] / svd.singularValues()[1];
        if (area >= 0.05 && cond <= 4.0)
            return;
    }
}

// Node grid of a linear function of the tile coordinates.
std::vector<double> linear_grid(int nodes, double a, double b, double c,
                                const std::function<vec2d(double, double)> &map)
{
    std::vector<double> g((size_t)nodes * nodes);
    for (int j = 0; j < nodes; ++j)
        for (int i = 0; i < nodes; ++i) {
            vec2d st = map((double)i / (nodes - 1), (double)j / (nodes - 1));
            g[(size_t)j * nodes + i] = a + b * st[0] + c * st[1];
        }
    return g;
}

double rel(double got, double ref) { return std::abs(got - ref) / std::max(std::abs(ref), 1e-300); }

} // namespace

void validate_chart(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path mesh_path = args.load_path("mesh");
    int n_triangles = args.load_integer("n_triangles", 20);
    int n_charts = args.load_integer("n_charts", 5);
    int n_points = args.load_integer("n_points", 200);
    int tex_nodes = args.load_integer("tex_nodes", 65);
    double tol_chain = (double)args.load_float("tolerance_chain", 1e-9f);
    double tol_clip = (double)args.load_float("tolerance_clip", 1e-11f);
    uint64_t seed = args.load_integer("seed", 2027);

    dmap::BaseMesh mesh = dmap::load_base_obj(mesh_path);
    ASSERT(mesh.has_texcoords(), "[%s] has no texture coordinates", mesh_path.string().c_str());
    RNG rng(seed);
    bool pass = true;
    int n_leaf = tex_nodes - 1;

    std::vector<int> picks;
    for (int k = 0; k < n_triangles; ++k)
        picks.push_back((int)(rng.next() * mesh.n_triangles()) % mesh.n_triangles());

    // (1) Identity equivalence, bit for bit.
    {
        int mismatches = 0;
        for (int t : picks) {
            dmap::BaseTriangle a = mesh.triangle(t);
            const Eigen::Vector3i &vi = mesh.faces[t];
            const Eigen::Vector3i &ni = mesh.face_normal_indices[t];
            vec3d p0 = mesh.positions[vi[0]], p1 = mesh.positions[vi[1]], p2 = mesh.positions[vi[2]];
            vec3d m0 = mesh.normals[ni[0]], m1 = mesh.normals[ni[1]], m2 = mesh.normals[ni[2]];
            bool same = (a.q0 == p0) && (a.e1 == (p1 - p0).eval()) && (a.e2 == (p2 - p0).eval()) && (a.m0 == m0) &&
                        (a.Mu == (m1 - m0).eval()) && (a.Mv == (m2 - m0).eval()) &&
                        a.bary(0.3, 0.6) == vec2d(0.3, 0.6) && a.param(0.3, 0.6) == vec2d(0.3, 0.6);
            if (!same)
                ++mismatches;
        }
        bool ok = mismatches == 0;
        pass &= ok;
        get_default_logger().info("(1) identity chart: {} of {} triangles differ from the barycentric convention {}",
                                  mismatches, picks.size(), ok ? "ok" : "FAIL");
    }

    // (2) Chain rule under random charts with a linear height field.
    {
        double worst_h = 0.0, worst_S = 0.0, worst_n = 0.0, worst_lam = 0.0, worst_det = 0.0, worst_cross = 0.0;
        for (int t : picks) {
            dmap::BaseTriangle a = mesh.triangle(t);
            for (int c = 0; c < n_charts; ++c) {
                vec2d tc[3];
                random_chart(rng, tc);
                dmap::BaseTriangle b(a.p0, a.p1, a.p2, a.m0, a.m0 + a.Mu, a.m0 + a.Mv, tc[0], tc[1], tc[2]);
                double la = 0.3 * rng.next(), lb = 0.4 * (rng.next() - 0.5), lc = 0.4 * (rng.next() - 0.5);
                double scale = 0.1 * dmap::mean_edge(a);
                // The same linear function on both tiles: on the chart tile
                // in (s, t), on the identity tile pulled back through the chart.
                std::vector<double> grid_b =
                    linear_grid(tex_nodes, la, lb, lc, [](double s, double t) { return vec2d(s, t); });
                std::vector<double> grid_a =
                    linear_grid(tex_nodes, la, lb, lc, [&](double u, double v) { return b.param(u, v); });
                dmap::HeightGrid fa{tex_nodes, tex_nodes, scale, grid_a.data()};
                dmap::HeightGrid fb{tex_nodes, tex_nodes, scale, grid_b.data()};
                double detJ = std::abs(b.J.determinant());
                for (int k = 0; k < n_points; ++k) {
                    double u = rng.next(), v = rng.next();
                    if (u + v > 1.0) {
                        u = 1.0 - u;
                        v = 1.0 - v;
                    }
                    vec2d st = b.param(u, v);
                    dmap::PointwiseFields A = dmap::pointwise_fields(a, fa, u, v);
                    dmap::PointwiseFields B = dmap::pointwise_fields(b, fb, st[0], st[1]);
                    worst_h = std::max(worst_h, rel(B.h, A.h));
                    vec3d SA = dmap::displaced_position(a, fa, u, v),
                          SB = dmap::displaced_position(b, fb, st[0], st[1]);
                    worst_S = std::max(worst_S, (SA - SB).norm() / dmap::mean_edge(a));
                    worst_n = std::max(worst_n, 1.0 - std::abs(A.n.normalized().dot(B.n.normalized())));
                    worst_lam = std::max(worst_lam, std::max(rel(B.lam_max, A.lam_max), rel(B.lam_min, A.lam_min)));
                    worst_det = std::max(worst_det, rel(B.sqrt_det, detJ * A.sqrt_det));
                    worst_cross = std::max(worst_cross, rel(B.n.norm(), B.sqrt_det));
                }
            }
        }
        double worst = std::max({worst_h, worst_S, worst_n, worst_lam, worst_det, worst_cross});
        bool ok = worst <= tol_chain;
        pass &= ok;
        get_default_logger().info("(2) chain rule over {} triangles x {} charts x {} points: h {:.2e}, position "
                                  "{:.2e}, normal {:.2e}, eigenvalues {:.2e}, sqrt(det G) vs |det J| {:.2e}, "
                                  "|Su x Sv| vs sqrt(det G) {:.2e} (tolerance {:.0e}) {}",
                                  picks.size(), n_charts, n_points, worst_h, worst_S, worst_n, worst_lam, worst_det,
                                  worst_cross, tol_chain, ok ? "ok" : "FAIL");
    }

    // (3) Clipping.
    {
        dmap::EmissionTile em(dmap::gaussian_spot(n_leaf, 0.4, 0.3, 0.12, 4.0, 0.1));
        double worst_sum = 0.0, worst_total = 0.0, worst_closed = 0.0;
        int64_t class_disagreements = 0, cells_checked = 0;
        double wl = 1.0 / n_leaf;

        auto check_domain = [&](const dmap::BaseTriangle &tri, bool identity) {
            dmap::UvTriangle domain(tri.t0, tri.t1, tri.t2);
            int64_t i0 = (int64_t)std::floor(domain.lo[0] / wl), j0 = (int64_t)std::floor(domain.lo[1] / wl);
            int64_t i1 = (int64_t)std::floor(domain.hi[0] / wl), j1 = (int64_t)std::floor(domain.hi[1] / wl);
            // Leaf sums, and the independent per-texel mass.
            double area_sum = 0.0, mass_sum = 0.0;
            for (int64_t j = j0; j <= j1; ++j)
                for (int64_t i = i0; i <= i1; ++i) {
                    vec2d c((i + 0.5) * wl, (j + 0.5) * wl);
                    double a = dmap::clip_square(domain, c, 0.5 * wl).area();
                    area_sum += a;
                    mass_sum += em.texel(i, j) * a;
                }
            worst_sum = std::max(worst_sum, rel(area_sum, tri.param_area()));
            dmap::Footprint fp = dmap::build_footprint(tri, em);
            worst_total = std::max(worst_total, std::max(rel(fp.total_area, area_sum), rel(fp.total_mass, mass_sum)));
            // Classification against the exact area, every cell of every
            // level from the roots down.
            for (int level = fp.root_level; level >= 0; --level) {
                double w = std::ldexp(1.0, level) / n_leaf;
                int64_t ci0 = (int64_t)std::floor(domain.lo[0] / w), cj0 = (int64_t)std::floor(domain.lo[1] / w);
                int64_t ci1 = (int64_t)std::floor(domain.hi[0] / w), cj1 = (int64_t)std::floor(domain.hi[1] / w);
                for (int64_t j = cj0 - 1; j <= cj1 + 1; ++j)
                    for (int64_t i = ci0 - 1; i <= ci1 + 1; ++i) {
                        vec2d c((i + 0.5) * w, (j + 0.5) * w);
                        double a = dmap::clip_square(domain, c, 0.5 * w).area();
                        dmap::Overlap cls = dmap::classify_square(domain, c, 0.5 * w);
                        double full = w * w, eps = 1e-12 * full;
                        bool agree = (cls == dmap::Overlap::Inside && a >= full - eps) ||
                                     (cls == dmap::Overlap::Outside && a <= eps) ||
                                     (cls == dmap::Overlap::Straddle && a > eps && a < full - eps);
                        // A measure-zero touch may classify either way.
                        if (!agree && !(a <= eps || a >= full - eps))
                            ++class_disagreements;
                        else if (!agree && cls == dmap::Overlap::Straddle)
                            ; // touching cell reported as straddling: harmless
                        else if (!agree)
                            ++class_disagreements;
                        ++cells_checked;
                        // The closed form clips against the hypotenuse only,
                        // so it applies to cells inside the unit tile.
                        if (identity && i >= 0 && j >= 0 && (i + 1) * w <= 1.0 + eps && (j + 1) * w <= 1.0 + eps)
                            worst_closed =
                                std::max(worst_closed, std::abs(a - dmap::clipped_cell_area(i * w, j * w, w)) / full);
                    }
            }
        };

        for (int t : picks) {
            check_domain(mesh.triangle(t), true);
            check_domain(mesh.triangle_chart(t), false);
            dmap::BaseTriangle a = mesh.triangle(t);
            vec2d tc[3];
            random_chart(rng, tc);
            double s = 3.0;
            vec2d off(1.7, -0.4);
            dmap::BaseTriangle tiled(a.p0, a.p1, a.p2, a.m0, a.m0 + a.Mu, a.m0 + a.Mv, tc[0] * s + off, tc[1] * s + off,
                                     tc[2] * s + off);
            check_domain(tiled, false);
        }
        bool ok =
            worst_sum <= tol_clip && worst_total <= tol_clip && worst_closed <= tol_clip && class_disagreements == 0;
        pass &= ok;
        get_default_logger().info("(3) clipping over {} triangles x 3 charts: leaf areas vs domain area {:.2e}, "
                                  "footprint totals vs per-texel sums {:.2e}, identity closed form vs exact clip "
                                  "{:.2e}, {} classification disagreements in {} cells {}",
                                  picks.size(), worst_sum, worst_total, worst_closed, class_disagreements,
                                  cells_checked, ok ? "ok" : "FAIL");
    }

    get_default_logger().info("VERDICT: {} — the general chart reduces to the identity chart exactly, obeys the "
                              "chain rule under random charts, and clips exactly on real and tiled layouts",
                              pass ? "PASS" : "FAIL");
    if (!pass)
        std::exit(1);
}
