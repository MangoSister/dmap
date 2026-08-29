// Phase S4 validation ("Plan — A-MVP sampling implementation"), in order:
// (1) the estimator constant on a tilted rectangle of known area — this
//     catches the offset-domain measure and line double-counting factors;
// (2) displaced-surface area against the exact triangle-area sum of the
//     pre-tessellated mesh (the mesh IS the sampled surface);
// (3) a weighted integral: per-vertex weights interpolated over each
//     micro-triangle are linear, so the reference (area x vertex mean per
//     triangle) is exact;
// (4) uniformity: total-variation distance between per-triangle hit counts
//     and per-triangle areas, the paper's own metric.
// Estimates must sit within 4 standard errors of their references.

#include "base_mesh.h"
#include "displaced_surface.h"
#include "displaced_tessellation.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "line_sampling.h"
#include "texture_grid.h"
#include <cstdlib>
#include <vector>

using namespace ks;

namespace
{

ks::MeshData make_rectangle(const vec3 &c0, const vec3 &a, const vec3 &b)
{
    ks::MeshData md;
    const vec3 verts[4] = {c0, c0 + a, c0 + a + b, c0 + b};
    for (const vec3 &v : verts)
        for (int c = 0; c < 3; ++c)
            md.vertices.push_back(v[c]);
    md.indices = {0, 1, 2, 0, 2, 3};
    dmap::finalize_mesh_data(md);
    return md;
}

struct EstimateCheck
{
    bool pass;
    double est, se;
};

// Two conditions. |est - ref| <= 4 se is the unbiasedness test: deviation
// over SE is a z-score, noise lands within 2 SE 95% of the time, while a
// wrong estimator constant shows up as thousands of SE. se <= 2% of ref
// makes the test able to resolve the reference at all: with a huge SE even
// a broken estimator would sit "within 4 se".
EstimateCheck check_estimate(double est, double se, double ref)
{
    return {std::abs(est - ref) <= 4.0 * se && se <= 0.02 * std::abs(ref), est, se};
}

} // namespace

void validate_line_sampling(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path mesh_path = args.load_path("mesh");
    fs::path tex_path = args.load_path("texture");
    double amplitude = (double)args.load_float("amplitude", 0.05f);
    int tex_nodes = args.load_integer("tex_nodes", 257);
    int n_tess = args.load_integer("n_tess", 32);
    int n_triangles = args.load_integer("n_triangles", 8);
    int64_t n_lines = args.load_integer("n_lines", 400000);
    double tv_threshold = (double)args.load_float("tv_threshold", 0.1f);
    uint64_t seed = args.load_integer("seed", 2027);

    EmbreeDevice device;
    RNG rng(seed);
    bool pass = true;

    // (1) Tilted rectangle of known area.
    {
        vec3 a(0.8f, 0.1f, 0.05f), b(-0.06f, 0.5f, 0.12f);
        ks::MeshData md = make_rectangle(vec3(0.1f, -0.2f, 0.3f), a, b);
        double ref = (double)a.cross(b).norm();
        dmap::AllHitsMesh scene(device, md);
        dmap::LineSampler lines(scene.bound());
        auto [est, se] =
            dmap::estimate_surface_integral(scene, lines, rng, n_lines, [](const dmap::LineHit &) { return 1.0; });
        EstimateCheck c = check_estimate(est, se, ref);
        pass &= c.pass;
        get_default_logger().info("(1) rectangle area: est {:.6f} +- {:.6f}, exact {:.6f} ({:.2f} se) {}", c.est, c.se,
                                  ref, std::abs(est - ref) / se, c.pass ? "ok" : "FAIL");
    }

    // Displaced submesh shared by checks 2-4. A contiguous triangle range:
    // a scattered pick spreads the submesh over the whole asset, which
    // makes the sampling box huge relative to the surface and starves the
    // estimator of hits.
    int t_start = args.load_integer("t_start", 0);
    dmap::BaseMesh mesh = dmap::load_base_obj(mesh_path);
    dmap::TextureGrid tex = dmap::downsample_box(dmap::load_height_texture(tex_path), tex_nodes);
    ks::MeshData md;
    for (int k = 0; k < n_triangles; ++k) {
        int t = (t_start + k) % mesh.n_triangles();
        dmap::BaseTriangle tri = mesh.triangle(t);
        dmap::HeightGrid field{tex.W, tex.H, amplitude * dmap::mean_edge(tri), tex.values.data()};
        dmap::append_displaced_triangle(md, tri, field, n_tess);
    }
    dmap::finalize_mesh_data(md);
    dmap::AllHitsMesh scene(device, md);
    dmap::LineSampler lines(scene.bound());

    // (2) Displaced surface area, with hit counts for the uniformity check.
    std::vector<int64_t> hit_counts(md.tri_count(), 0);
    {
        double ref = dmap::mesh_data_area(md);
        auto [est, se] = dmap::estimate_surface_integral(
            scene, lines, rng, n_lines, [](const dmap::LineHit &) { return 1.0; }, &hit_counts);
        EstimateCheck c = check_estimate(est, se, ref);
        pass &= c.pass;
        get_default_logger().info("(2) displaced area ({} micro-triangles): est {:.6f} +- {:.6f}, "
                                  "triangle sum {:.6f} ({:.2f} se) {}",
                                  md.tri_count(), c.est, c.se, ref, std::abs(est - ref) / se, c.pass ? "ok" : "FAIL");
    }

    // (3) Weighted integral with per-vertex weights (linear per triangle,
    // so the reference is exact).
    {
        dmap::HeightGrid weight_field{tex.W, tex.H, 1.0, tex.values.data()};
        std::vector<double> Ev(md.vertex_count());
        for (int k = 0; k < md.vertex_count(); ++k) {
            vec2 uv = md.get_texcoord(k);
            Ev[k] = weight_field.h(uv[0], uv[1]);
        }
        double ref = 0.0;
        for (int t = 0; t < md.tri_count(); ++t) {
            uint32_t i0 = md.indices[3 * t], i1 = md.indices[3 * t + 1], i2 = md.indices[3 * t + 2];
            vec3d p0 = md.get_pos(i0).cast<double>(), p1 = md.get_pos(i1).cast<double>(),
                  p2 = md.get_pos(i2).cast<double>();
            double area = 0.5 * (p1 - p0).cross(p2 - p0).norm();
            ref += area * (Ev[i0] + Ev[i1] + Ev[i2]) / 3.0;
        }
        auto [est, se] = dmap::estimate_surface_integral(scene, lines, rng, n_lines, [&](const dmap::LineHit &h) {
            uint32_t i0 = md.indices[3 * h.prim_id], i1 = md.indices[3 * h.prim_id + 1],
                     i2 = md.indices[3 * h.prim_id + 2];
            return (1.0 - h.b1 - h.b2) * Ev[i0] + h.b1 * Ev[i1] + h.b2 * Ev[i2];
        });
        EstimateCheck c = check_estimate(est, se, ref);
        pass &= c.pass;
        get_default_logger().info("(3) weighted integral: est {:.6f} +- {:.6f}, exact {:.6f} ({:.2f} se) {}", c.est,
                                  c.se, ref, std::abs(est - ref) / se, c.pass ? "ok" : "FAIL");
    }

    // (4) Uniformity: hits vs areas, total-variation distance, binned per
    // base triangle (micro-triangle bins would need far more hits than the
    // estimate checks do to resolve).
    {
        int64_t n_hits = 0;
        for (int64_t c : hit_counts)
            n_hits += c;
        int tris_per_base = md.tri_count() / n_triangles;
        std::vector<int64_t> bin_hits(n_triangles, 0);
        std::vector<double> bin_area(n_triangles, 0.0);
        double total_area = dmap::mesh_data_area(md);
        for (int t = 0; t < md.tri_count(); ++t) {
            int bin = t / tris_per_base;
            bin_hits[bin] += hit_counts[t];
            vec3d p0 = md.get_pos(md.indices[3 * t]).cast<double>(),
                  p1 = md.get_pos(md.indices[3 * t + 1]).cast<double>(),
                  p2 = md.get_pos(md.indices[3 * t + 2]).cast<double>();
            bin_area[bin] += 0.5 * (p1 - p0).cross(p2 - p0).norm();
        }
        double tv = 0.0;
        for (int b = 0; b < n_triangles; ++b)
            tv += std::abs((double)bin_hits[b] / n_hits - bin_area[b] / total_area);
        tv *= 0.5;
        bool ok = tv <= tv_threshold;
        pass &= ok;
        get_default_logger().info("(4) uniformity: {} hits binned per base triangle ({} bins), TV distance "
                                  "{:.4f} (threshold {:.2f}) {}",
                                  n_hits, n_triangles, tv, tv_threshold, ok ? "ok" : "FAIL");
    }

    get_default_logger().info("VERDICT: {} — line-sampling estimator (Ling et al. port) unbiased on known "
                              "areas and weighted integrals, hits uniform in area",
                              pass ? "PASS" : "FAIL");
    if (!pass)
        std::exit(1);
}
