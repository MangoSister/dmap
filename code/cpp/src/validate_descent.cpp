// Phase S6 validation ("Plan — A-MVP sampling implementation"), four checks,
// under any chart (path tracer plan T1):
// (1) unbiasedness: Monte Carlo integrals over the displaced triangle
//     through each sampler's pdf agree with dense per-texel quadrature —
//     area-only descent, product descent, receiver-aware descent at a near
//     and a far receiver, the emission-only table, the product table, and
//     product descent on an emission map with exact-zero cells (which also
//     proves the certified-zero pruning never draws a zero-emission point);
// (2) sample histogram at leaf resolution matches the discrete path
//     probabilities (TV distance), for product and receiver-aware descent;
// (3) on a flat patch the product descent with beta = 0 collapses to the
//     normalized emission masses (the S5 image-table distribution), and
//     its pdf matches the product table on interior texels;
// (4) the MIS contract: the sample-side pdf equals the query-side pdf
//     re-walked from (u, v), bit-exactly, for every variant.
// Estimates must sit within 4 standard errors (see line_sampling.h on SE).
//
// The chart is a config choice: "identity" (the experiments' convention) or
// "random", a seeded triangle in the texture plane, optionally scaled and
// offset so the domain spans several repeats of the tile.

#include "base_mesh.h"
#include "descent_sampler.h"
#include "displaced_surface.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "ks/rng.h"
#include "texture_grid.h"
#include "uv_clip.h"
#include <cmath>
#include <cstdlib>
#include <functional>
#include <vector>

using namespace ks;

namespace
{

struct MeanSE
{
    double mean, se;
};

bool check(const char *label, const MeanSE &e, double ref, bool &pass)
{
    // Absolute epsilon for the zero-variance (perfect importance) case.
    bool ok = std::abs(e.mean - ref) <= 4.0 * e.se + 1e-9 * std::abs(ref) && e.se <= 0.02 * std::abs(ref);
    pass &= ok;
    if (e.se > 0.0) {
        get_default_logger().info("{}: est {:.6f} +- {:.6f}, ref {:.6f} ({:.2f} se) {}", label, e.mean, e.se, ref,
                                  std::abs(e.mean - ref) / e.se, ok ? "ok" : "FAIL");
    } else {
        get_default_logger().info("{}: est {:.6f} (zero variance), ref {:.6f} {}", label, e.mean, ref,
                                  ok ? "ok" : "FAIL");
    }
    return ok;
}

// Texel box of a domain: origin and size in texel units.
struct TexelBox
{
    int64_t i0, j0;
    int ni, nj;
};

TexelBox texel_box(const dmap::UvTriangle &domain, int n)
{
    double wl = 1.0 / n;
    TexelBox b;
    b.i0 = (int64_t)std::floor(domain.lo[0] / wl);
    b.j0 = (int64_t)std::floor(domain.lo[1] / wl);
    b.ni = (int)((int64_t)std::ceil(domain.hi[0] / wl) - 1 - b.i0 + 1);
    b.nj = (int)((int64_t)std::ceil(domain.hi[1] / wl) - 1 - b.j0 + 1);
    return b;
}

// Midpoint rule on the triangle A + x (B - A) + y (C - A): the m x m
// square midpoints with x + y < 1, the diagonal at half weight (the
// diagonal cuts its cells exactly in half), each cell worth 2 area / m^2.
template <typename F>
double triangle_rule(const vec2d &A, const vec2d &B, const vec2d &C, int m, const F &g)
{
    double area = 0.5 * std::abs((B - A)[0] * (C - A)[1] - (B - A)[1] * (C - A)[0]);
    double cell = 2.0 * area / ((double)m * m);
    double sum = 0.0;
    for (int b = 0; b < m; ++b)
        for (int a = 0; a < m; ++a) {
            double x = (a + 0.5) / m, y = (b + 0.5) / m;
            double s = x + y;
            double wgt = s < 1.0 ? 1.0 : (s == 1.0 ? 0.5 : 0.0);
            if (wgt == 0.0)
                continue;
            vec2d p = A + x * (B - A) + y * (C - A);
            sum += wgt * g(p[0], p[1]);
        }
    return sum * cell;
}

// Integral over the domain of f(u, v) sqrt(det G) du dv: per texel, a
// square midpoint rule inside, and the clipped polygon's fan triangles on
// the boundary, so a piecewise-constant emission is integrated exactly.
double quad_integral(const dmap::BaseTriangle &tri, const dmap::HeightGrid &field, const dmap::UvTriangle &domain,
                     int n, int m_interior, int m_edge, const std::function<double(double, double)> &f)
{
    double wl = 1.0 / n;
    TexelBox box = texel_box(domain, n);
    auto g = [&](double u, double v) { return f(u, v) * dmap::pointwise_fields(tri, field, u, v).sqrt_det; };
    double sum = 0.0;
    for (int b = 0; b < box.nj; ++b)
        for (int a = 0; a < box.ni; ++a) {
            int64_t i = box.i0 + a, j = box.j0 + b;
            vec2d c((i + 0.5) * wl, (j + 0.5) * wl);
            dmap::Overlap cls = dmap::classify_square(domain, c, 0.5 * wl);
            if (cls == dmap::Overlap::Outside)
                continue;
            if (cls == dmap::Overlap::Inside) {
                int m = m_interior;
                double cell = 0.0;
                for (int y = 0; y < m; ++y)
                    for (int x = 0; x < m; ++x)
                        cell += g((i + (x + 0.5) / m) * wl, (j + (y + 0.5) / m) * wl);
                sum += cell * (wl * wl) / ((double)m * m);
                continue;
            }
            dmap::ClipPolygon poly = dmap::clip_square(domain, c, 0.5 * wl);
            for (int k = 1; k + 1 < poly.n; ++k)
                sum += triangle_rule(poly.p[0], poly.p[k], poly.p[k + 1], m_edge, g);
        }
    return sum;
}

MeanSE mc_descent(const dmap::DescentSampler &sampler, RNG &rng, int64_t n_samples, const dmap::Receiver *receiver,
                  const std::function<double(double, double)> &f)
{
    double sum = 0.0, sum_sq = 0.0;
    for (int64_t k = 0; k < n_samples; ++k) {
        dmap::DescentSample s = sampler.sample(rng, receiver);
        double x = f(s.u, s.v) / s.pdf_area;
        sum += x;
        sum_sq += x * x;
    }
    double mean = sum / n_samples;
    double var = std::max(0.0, sum_sq / n_samples - mean * mean) / (n_samples - 1);
    return {mean, std::sqrt(var)};
}

MeanSE mc_table(const dmap::TexelTableSampler &sampler, RNG &rng, int64_t n_samples,
                const std::function<double(double, double)> &f)
{
    double sum = 0.0, sum_sq = 0.0;
    for (int64_t k = 0; k < n_samples; ++k) {
        dmap::TableSample s = sampler.sample(rng);
        double x = s.in_domain ? f(s.u, s.v) / s.pdf_area : 0.0;
        sum += x;
        sum_sq += x * x;
    }
    double mean = sum / n_samples;
    double var = std::max(0.0, sum_sq / n_samples - mean * mean) / (n_samples - 1);
    return {mean, std::sqrt(var)};
}

// A seeded chart inside the unit tile with bounded stretch, then the
// object's tiling: t -> t * uv_scale + uv_offset.
void random_chart(RNG &rng, double uv_scale, const vec2d &uv_offset, vec2d t[3])
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
            break;
    }
    for (int k = 0; k < 3; ++k)
        t[k] = t[k] * uv_scale + uv_offset;
}

} // namespace

void validate_descent(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path mesh_path = args.load_path("mesh");
    fs::path tex_path = args.load_path("texture");
    int tex_nodes = args.load_integer("tex_nodes", 65);
    double amplitude = (double)args.load_float("amplitude", 0.2f);
    int triangle_index = args.load_integer("triangle_index", -1);
    int64_t n_samples = args.load_integer("n_samples", 500000);
    int64_t n_samples_hist = args.load_integer("n_samples_hist", 2000000);
    int64_t n_samples_pdf = args.load_integer("n_samples_pdf", 200000);
    double beta = (double)args.load_float("beta", 0.05f);
    dmap::PyramidBuild build = dmap::pyramid_build_from_string(args.load_string("pyramid_build", "fold"));
    double tv_threshold = (double)args.load_float("tv_threshold", 0.05f);
    uint64_t seed = args.load_integer("seed", 2027);
    std::string chart = args.load_string("chart", "identity");
    uint64_t chart_seed = args.load_integer("chart_seed", 7);
    double uv_scale = (double)args.load_float("uv_scale", 1.0f);
    vec2d uv_offset = args.load_vec2("uv_offset", false, vec2::Zero()).cast<double>();
    int m_interior = 8, m_edge = 32; // quadrature points per texel side, per fan triangle side

    dmap::BaseMesh mesh = dmap::load_base_obj(mesh_path);
    dmap::TextureGrid tex = dmap::downsample_box(dmap::load_height_texture(tex_path), tex_nodes);
    int n_leaf = tex_nodes - 1;
    if (triangle_index < 0)
        triangle_index = mesh.n_triangles() / 3;
    dmap::BaseTriangle tri_id = mesh.triangle(triangle_index);
    vec2d t[3] = {tri_id.t0, tri_id.t1, tri_id.t2};
    if (chart == "random") {
        RNG crng(chart_seed);
        random_chart(crng, uv_scale, uv_offset, t);
    } else {
        ASSERT(chart == "identity", "chart must be identity or random, got [%s]", chart.c_str());
    }
    dmap::BaseTriangle tri(tri_id.p0, tri_id.p1, tri_id.p2, tri_id.m0, tri_id.m0 + tri_id.Mu, tri_id.m0 + tri_id.Mv,
                           t[0], t[1], t[2]);
    dmap::HeightGrid field{tex.W, tex.H, amplitude * dmap::mean_edge(tri), tex.values.data()};
    field.repeat = chart != "identity";
    dmap::TaylorPyramid pyramid(field, build);
    dmap::UvTriangle domain(tri.t0, tri.t1, tri.t2);
    get_default_logger().info("{} triangle {} + {} ({} nodes), amplitude {:.2f}, beta {:.2f}, chart {} "
                              "(t0 ({:.3f}, {:.3f}) t1 ({:.3f}, {:.3f}) t2 ({:.3f}, {:.3f}), area {:.4f})",
                              mesh_path.filename().string(), triangle_index, tex_path.filename().string(), tex_nodes,
                              amplitude, beta, chart, t[0][0], t[0][1], t[1][0], t[1][1], t[2][0], t[2][1],
                              tri.param_area());

    dmap::EmissionTile em_spot(dmap::gaussian_spot(n_leaf, 0.4, 0.3, 0.12, 4.0, 0.1));
    dmap::EmissionTile em_zero(dmap::checkerboard(n_leaf, 8, 0.0, 1.0));
    auto texel_of = [&](const dmap::EmissionTile &em, double u, double v) {
        return em.texel((int64_t)std::floor(u * n_leaf), (int64_t)std::floor(v * n_leaf));
    };
    auto E_spot = [&](double u, double v) { return texel_of(em_spot, u, v); };
    auto E_zero = [&](double u, double v) { return texel_of(em_zero, u, v); };

    // Receivers: on the centre surface normal, near field and far field.
    vec2d sc = tri.param(1.0 / 3.0, 1.0 / 3.0);
    dmap::PointwiseFields fc = dmap::pointwise_fields(tri, field, sc[0], sc[1]);
    vec3d nc = fc.n.normalized();
    vec3d yc = tri.P(sc[0], sc[1]) + fc.h * dmap::normal_frame_at(tri, sc[0], sc[1]).N;
    double me = dmap::mean_edge(tri);
    dmap::Receiver recv_near{yc + 0.35 * me * nc, -nc};
    dmap::Receiver recv_far{yc + 5.0 * me * nc, -nc};
    auto geom = [&](const dmap::Receiver &recv, double u, double v) {
        dmap::PointwiseFields f = dmap::pointwise_fields(tri, field, u, v);
        vec3d y = tri.P(u, v) + f.h * dmap::normal_frame_at(tri, u, v).N;
        vec3d d = y - recv.x;
        double r2 = std::max(d.squaredNorm(), 1e-12);
        vec3d omega = d / std::sqrt(r2);
        return std::max(recv.n.dot(omega), 0.0) * std::abs(f.n.normalized().dot(omega)) / r2;
    };

    dmap::DescentSampler samp_area(tri, field, pyramid, em_spot, dmap::DescentWeight::AreaOnly, beta);
    dmap::DescentSampler samp_prod(tri, field, pyramid, em_spot, dmap::DescentWeight::Product, beta);
    dmap::DescentSampler samp_geom(tri, field, pyramid, em_spot, dmap::DescentWeight::ProductGeometry, beta);
    dmap::DescentSampler samp_zero(tri, field, pyramid, em_zero, dmap::DescentWeight::Product, beta);
    dmap::TexelTableSampler table_em(tri, field, em_spot, /*with_metric*/ false);
    dmap::TexelTableSampler table_prod(tri, field, em_spot, /*with_metric*/ true);
    get_default_logger().info("footprint: roots at level {} ({} straddling nodes), clipped area {:.6f} vs domain "
                              "{:.6f}",
                              samp_prod.footprint.root_level, samp_prod.footprint.nodes.size(),
                              samp_prod.footprint.total_area, tri.param_area());

    RNG rng(seed);
    bool pass = true;
    auto one = [](double, double) { return 1.0; };

    // (1) Unbiasedness against dense quadrature.
    {
        double ref_area = quad_integral(tri, field, domain, n_leaf, m_interior, m_edge, one);
        double ref_E = quad_integral(tri, field, domain, n_leaf, m_interior, m_edge, E_spot);
        check("(1) area descent, integral dA", mc_descent(samp_area, rng, n_samples, nullptr, one), ref_area, pass);
        check("(1) area descent, integral E dA", mc_descent(samp_area, rng, n_samples, nullptr, E_spot), ref_E, pass);
        check("(1) product descent, integral dA", mc_descent(samp_prod, rng, n_samples, nullptr, one), ref_area, pass);
        check("(1) product descent, integral E dA", mc_descent(samp_prod, rng, n_samples, nullptr, E_spot), ref_E,
              pass);
        check("(1) emission table, integral dA", mc_table(table_em, rng, n_samples, one), ref_area, pass);
        check("(1) emission table, integral E dA", mc_table(table_em, rng, n_samples, E_spot), ref_E, pass);
        check("(1) product table, integral dA", mc_table(table_prod, rng, n_samples, one), ref_area, pass);
        check("(1) product table, integral E dA", mc_table(table_prod, rng, n_samples, E_spot), ref_E, pass);

        for (auto [recv, name] : {std::pair{&recv_near, "near"}, std::pair{&recv_far, "far"}}) {
            auto f = [&](double u, double v) { return E_spot(u, v) * geom(*recv, u, v); };
            double ref = quad_integral(tri, field, domain, n_leaf, m_interior, m_edge, f);
            std::string label = std::string("(1) receiver-aware descent (") + name + "), integral E G dA";
            check(label.c_str(), mc_descent(samp_geom, rng, n_samples, recv, f), ref, pass);
        }

        // Zero-cell emission: pruning must be exact and never draw E == 0.
        double ref_Ez = quad_integral(tri, field, domain, n_leaf, m_interior, m_edge, E_zero);
        int64_t zero_draws = 0;
        double sum = 0.0, sum_sq = 0.0;
        for (int64_t k = 0; k < n_samples; ++k) {
            dmap::DescentSample s = samp_zero.sample(rng, nullptr);
            double e = E_zero(s.u, s.v);
            if (e == 0.0)
                ++zero_draws;
            double x = e / s.pdf_area;
            sum += x;
            sum_sq += x * x;
        }
        double mean = sum / n_samples;
        double se = std::sqrt(std::max(0.0, sum_sq / n_samples - mean * mean) / (n_samples - 1));
        check("(1) product descent, zero-cell checkerboard, integral E dA", {mean, se}, ref_Ez, pass);
        bool ok = zero_draws == 0;
        pass &= ok;
        get_default_logger().info("(1) certified-zero pruning: {} zero-emission draws in {} samples {}", zero_draws,
                                  n_samples, ok ? "ok" : "FAIL");
    }

    // (2) Leaf-resolution histogram against the discrete path probabilities,
    // over the domain's texel box.
    TexelBox box = texel_box(domain, n_leaf);
    for (auto [sampler, recv, ns, name] :
         {std::tuple{&samp_prod, (const dmap::Receiver *)nullptr, n_samples_hist, "product"},
          std::tuple{&samp_geom, (const dmap::Receiver *)&recv_near, n_samples_hist / 2, "receiver-aware near"}}) {
        std::vector<int64_t> counts((size_t)box.ni * box.nj, 0);
        for (int64_t k = 0; k < ns; ++k) {
            dmap::DescentSample s = sampler->sample(rng, recv);
            int64_t i = (int64_t)std::floor(s.u * n_leaf) - box.i0, j = (int64_t)std::floor(s.v * n_leaf) - box.j0;
            ASSERT(i >= 0 && i < box.ni && j >= 0 && j < box.nj, "sample outside the domain's texel box");
            ++counts[(size_t)j * box.ni + i];
        }
        double tv = 0.0;
        for (int b = 0; b < box.nj; ++b)
            for (int a = 0; a < box.ni; ++a)
                tv += std::abs((double)counts[(size_t)b * box.ni + a] / ns -
                               sampler->leaf_prob(box.i0 + a, box.j0 + b, recv));
        tv *= 0.5;
        bool ok = tv <= tv_threshold;
        pass &= ok;
        get_default_logger().info("(2) {} histogram: TV distance {:.4f} over {} leaves (threshold {:.2f}) {}", name, tv,
                                  (size_t)box.ni * box.nj, tv_threshold, ok ? "ok" : "FAIL");
    }

    // (3) Flat patch: product descent (beta = 0) collapses to the
    // normalized emission masses — the S5 image-table distribution — and
    // matches the product table's pdf on interior texels.
    {
        dmap::BaseTriangle flat(vec3d(0, 0, 0), vec3d(1, 0, 0), vec3d(0, 1, 0), vec3d(0, 0, 1), vec3d(0, 0, 1),
                                vec3d(0, 0, 1), tri.t0, tri.t1, tri.t2);
        std::vector<double> flat_values((size_t)tex_nodes * tex_nodes, 0.4);
        dmap::HeightGrid flat_field{tex_nodes, tex_nodes, 0.1, flat_values.data()};
        flat_field.repeat = field.repeat;
        dmap::TaylorPyramid flat_pyr(flat_field, build);
        dmap::DescentSampler flat_desc(flat, flat_field, flat_pyr, em_spot, dmap::DescentWeight::Product, 0.0);
        dmap::TexelTableSampler flat_table(flat, flat_field, em_spot, /*with_metric*/ true);

        double total = flat_desc.footprint.total_mass;
        double wl = 1.0 / n_leaf;
        double worst_mass = 0.0, worst_pdf = 0.0;
        for (int b = 0; b < box.nj; ++b)
            for (int a = 0; a < box.ni; ++a) {
                int64_t i = box.i0 + a, j = box.j0 + b;
                dmap::FootprintChild leaf = flat_desc.leaf_info(i, j);
                if (leaf.overlap == dmap::Overlap::Outside || leaf.mass == 0.0)
                    continue;
                double expected = leaf.mass / total;
                double got = flat_desc.leaf_prob(i, j, nullptr);
                worst_mass = std::max(worst_mass, std::abs(got - expected) / expected);
                if (leaf.overlap == dmap::Overlap::Inside) { // interior texel: uniform table conditional
                    double uc = (i + 0.5) * wl, vc = (j + 0.5) * wl;
                    double pd = flat_desc.pdf_area(uc, vc, nullptr);
                    double pt = flat_table.pdf_area(uc, vc);
                    worst_pdf = std::max(worst_pdf, std::abs(pd - pt) / pd);
                }
            }
        // The table accumulates float CDFs over thousands of texels (row
        // sums plus the marginal), so its pdf carries a few 1e-4 of
        // rounding; the descent side is double throughout.
        bool ok = worst_mass <= 1e-12 && worst_pdf <= 1e-3;
        pass &= ok;
        get_default_logger().info("(3) flat patch: leaf mass vs normalized emission, worst rel {:.3e}; "
                                  "pdf vs product table (interior texels), worst rel {:.3e} {}",
                                  worst_mass, worst_pdf, ok ? "ok" : "FAIL");
    }

    // (4) MIS contract: sample-side pdf == query-side pdf, bit-exactly.
    for (auto [sampler, recv, name] : {std::tuple{&samp_area, (const dmap::Receiver *)nullptr, "area"},
                                       std::tuple{&samp_prod, (const dmap::Receiver *)nullptr, "product"},
                                       std::tuple{&samp_geom, (const dmap::Receiver *)&recv_near, "receiver-aware"}}) {
        int64_t mismatch = 0;
        for (int64_t k = 0; k < n_samples_pdf; ++k) {
            dmap::DescentSample s = sampler->sample(rng, recv);
            if (s.pdf_uv != sampler->pdf_uv(s.u, s.v, recv))
                ++mismatch;
        }
        bool ok = mismatch == 0;
        pass &= ok;
        get_default_logger().info("(4) {}: {} pdf re-walk mismatches in {} samples {}", name, mismatch, n_samples_pdf,
                                  ok ? "ok" : "FAIL");
    }

    get_default_logger().info("VERDICT: {} — descent samplers ({} chart): unbiased through the exact area pdf for "
                              "all weight variants, histogram matches the path probabilities, flat-patch collapse "
                              "to the image table, and the pdf re-walk is exact",
                              pass ? "PASS" : "FAIL", chart);
    if (!pass)
        std::exit(1);
}
