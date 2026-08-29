// Phase S6 validation ("Plan — A-MVP sampling implementation"), four checks:
// (1) unbiasedness: Monte Carlo integrals over the displaced triangle
//     through each sampler's pdf agree with dense per-texel quadrature —
//     area-only descent, product descent, receiver-aware descent at a near
//     and a far receiver, the emission-only table, the product table, and
//     product descent on an emission map with exact-zero cells (which also
//     proves the certified-zero pruning never draws a zero-emission point);
// (2) sample histogram at leaf resolution matches the discrete path
//     probabilities (TV distance), for product and receiver-aware descent;
// (3) on a flat patch the product descent with beta = 0 collapses to the
//     normalized emission integrals (the S5 image-table distribution), and
//     its pdf matches the product table on interior texels;
// (4) the MIS contract: the sample-side pdf equals the query-side pdf
//     re-walked from (u, v), bit-exactly, for every variant.
// Estimates must sit within 4 standard errors (see line_sampling.h on SE).

#include "base_mesh.h"
#include "descent_sampler.h"
#include "displaced_surface.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "ks/rng.h"
#include "texture_grid.h"
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

// Integral over the triangle of f(u, v) sqrt(det G) du dv by a per-texel
// midpoint rule. Points exactly on the hypotenuse get half weight; the
// diagonal cuts straddling texels exactly in half, so with power-of-two m
// the clipped fraction is integrated exactly.
double quad_integral(const dmap::BaseTriangle &tri, const dmap::HeightGrid &field, int n, int m_interior, int m_edge,
                     const std::function<double(double, double)> &f)
{
    double wl = 1.0 / n;
    double sum = 0.0;
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            if (i + j >= n)
                continue; // fully outside
            int m = (i + j + 2 <= n) ? m_interior : m_edge;
            double cell = 0.0;
            for (int b = 0; b < m; ++b)
                for (int a = 0; a < m; ++a) {
                    double u = (i + (a + 0.5) / m) * wl;
                    double v = (j + (b + 0.5) / m) * wl;
                    double s = u + v;
                    double wgt = s < 1.0 ? 1.0 : (s == 1.0 ? 0.5 : 0.0);
                    if (wgt == 0.0)
                        continue;
                    cell += wgt * f(u, v) * dmap::pointwise_fields(tri, field, u, v).sqrt_det;
                }
            sum += cell * (wl * wl) / ((double)m * m);
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
    double tv_threshold = (double)args.load_float("tv_threshold", 0.05f);
    uint64_t seed = args.load_integer("seed", 2027);
    int m_interior = 8, m_edge = 128; // quadrature points per texel side

    dmap::BaseMesh mesh = dmap::load_base_obj(mesh_path);
    dmap::TextureGrid tex = dmap::downsample_box(dmap::load_height_texture(tex_path), tex_nodes);
    int n_leaf = tex_nodes - 1;
    if (triangle_index < 0)
        triangle_index = mesh.n_triangles() / 3;
    dmap::BaseTriangle tri = mesh.triangle(triangle_index);
    dmap::HeightGrid field{tex.W, tex.H, amplitude * dmap::mean_edge(tri), tex.values.data()};
    get_default_logger().info("{} triangle {} + {} ({} nodes), amplitude {:.2f}, beta {:.2f}",
                              mesh_path.filename().string(), triangle_index, tex_path.filename().string(), tex_nodes,
                              amplitude, beta);

    dmap::TextureGrid em_spot = dmap::gaussian_spot(n_leaf, 0.4, 0.3, 0.12, 4.0, 0.1);
    dmap::TextureGrid em_zero = dmap::checkerboard(n_leaf, 8, 0.0, 1.0);
    auto E_spot = [&](double u, double v) {
        int i = std::min((int)(u * n_leaf), n_leaf - 1), j = std::min((int)(v * n_leaf), n_leaf - 1);
        return em_spot.values[(size_t)j * n_leaf + i];
    };
    auto E_zero = [&](double u, double v) {
        int i = std::min((int)(u * n_leaf), n_leaf - 1), j = std::min((int)(v * n_leaf), n_leaf - 1);
        return em_zero.values[(size_t)j * n_leaf + i];
    };

    // Receivers: on the centre surface normal, near field and far field.
    dmap::PointwiseFields fc = dmap::pointwise_fields(tri, field, 1.0 / 3.0, 1.0 / 3.0);
    vec3d nc = fc.n.normalized();
    vec3d yc = tri.P(1.0 / 3.0, 1.0 / 3.0) + fc.h * dmap::normal_frame_at(tri, 1.0 / 3.0, 1.0 / 3.0).N;
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

    dmap::DescentSampler samp_area(tri, field, em_spot, dmap::DescentWeight::AreaOnly, beta);
    dmap::DescentSampler samp_prod(tri, field, em_spot, dmap::DescentWeight::Product, beta);
    dmap::DescentSampler samp_geom(tri, field, em_spot, dmap::DescentWeight::ProductGeometry, beta);
    dmap::DescentSampler samp_zero(tri, field, em_zero, dmap::DescentWeight::Product, beta);
    dmap::TexelTableSampler table_em(tri, field, em_spot, /*with_metric*/ false);
    dmap::TexelTableSampler table_prod(tri, field, em_spot, /*with_metric*/ true);

    RNG rng(seed);
    bool pass = true;
    auto one = [](double, double) { return 1.0; };

    // (1) Unbiasedness against dense quadrature.
    {
        double ref_area = quad_integral(tri, field, n_leaf, m_interior, m_edge, one);
        double ref_E = quad_integral(tri, field, n_leaf, m_interior, m_edge, E_spot);
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
            double ref = quad_integral(tri, field, n_leaf, m_interior, m_edge, f);
            std::string label = std::string("(1) receiver-aware descent (") + name + "), integral E G dA";
            check(label.c_str(), mc_descent(samp_geom, rng, n_samples, recv, f), ref, pass);
        }

        // Zero-cell emission: pruning must be exact and never draw E == 0.
        double ref_Ez = quad_integral(tri, field, n_leaf, m_interior, m_edge, E_zero);
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

    // (2) Leaf-resolution histogram against the discrete path probabilities.
    for (auto [sampler, recv, ns, name] :
         {std::tuple{&samp_prod, (const dmap::Receiver *)nullptr, n_samples_hist, "product"},
          std::tuple{&samp_geom, (const dmap::Receiver *)&recv_near, n_samples_hist / 2, "receiver-aware near"}}) {
        std::vector<int64_t> counts((size_t)n_leaf * n_leaf, 0);
        for (int64_t k = 0; k < ns; ++k) {
            dmap::DescentSample s = sampler->sample(rng, recv);
            int i = std::min((int)(s.u * n_leaf), n_leaf - 1), j = std::min((int)(s.v * n_leaf), n_leaf - 1);
            ++counts[(size_t)j * n_leaf + i];
        }
        double tv = 0.0;
        for (int j = 0; j < n_leaf; ++j)
            for (int i = 0; i < n_leaf; ++i)
                tv += std::abs((double)counts[(size_t)j * n_leaf + i] / ns - sampler->leaf_prob(i, j, recv));
        tv *= 0.5;
        bool ok = tv <= tv_threshold;
        pass &= ok;
        get_default_logger().info("(2) {} histogram: TV distance {:.4f} over {} leaves (threshold {:.2f}) {}", name, tv,
                                  (size_t)n_leaf * n_leaf, tv_threshold, ok ? "ok" : "FAIL");
    }

    // (3) Flat patch: product descent (beta = 0) collapses to the
    // normalized emission integrals — the S5 image-table distribution —
    // and matches the product table's pdf on interior texels.
    {
        dmap::BaseTriangle flat(vec3d(0, 0, 0), vec3d(1, 0, 0), vec3d(0, 1, 0), vec3d(0, 0, 1), vec3d(0, 0, 1),
                                vec3d(0, 0, 1));
        std::vector<double> flat_values((size_t)tex_nodes * tex_nodes, 0.4);
        dmap::HeightGrid flat_field{tex_nodes, tex_nodes, 0.1, flat_values.data()};
        dmap::DescentSampler flat_desc(flat, flat_field, em_spot, dmap::DescentWeight::Product, 0.0);
        dmap::TexelTableSampler flat_table(flat, flat_field, em_spot, /*with_metric*/ true);

        double e_root = flat_desc.e_sum.back()[0];
        double wl = 1.0 / n_leaf;
        double worst_mass = 0.0, worst_pdf = 0.0;
        for (int j = 0; j < n_leaf; ++j)
            for (int i = 0; i < n_leaf; ++i) {
                double expected = flat_desc.e_sum[0][(size_t)j * n_leaf + i] / e_root;
                double got = flat_desc.leaf_prob(i, j, nullptr);
                worst_mass = std::max(worst_mass, std::abs(got - expected) / std::max(expected, 1e-300));
                if (i + j + 2 <= n_leaf) { // interior texel: uniform table conditional
                    double uc = (i + 0.5) * wl, vc = (j + 0.5) * wl;
                    double pd = flat_desc.pdf_area(uc, vc, nullptr);
                    double pt = flat_table.pdf_area(uc, vc);
                    worst_pdf = std::max(worst_pdf, std::abs(pd - pt) / pd);
                }
            }
        // The table accumulates float CDFs over 4096 texels (row sums plus
        // the marginal), so its pdf carries a few 1e-4 of rounding; the
        // descent side is double throughout.
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

    get_default_logger().info("VERDICT: {} — descent samplers: unbiased through the exact area pdf for all weight "
                              "variants, histogram matches the path probabilities, flat-patch collapse to the image "
                              "table, and the pdf re-walk is exact",
                              pass ? "PASS" : "FAIL");
    if (!pass)
        std::exit(1);
}
