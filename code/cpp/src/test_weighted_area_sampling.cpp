// Phase S7 ("Plan — A-MVP sampling implementation"): the receiver
// irradiance study. One emissive displaced triangle; receivers on a seeded
// sphere of directions at several distances; unoccluded irradiance
//     I(x) = integral_S E(y) cos_r+ |cos_e| / r^2 dA(y)
// estimated by the six-sampler ladder of master plan §5A: uniform (Ling
// line casting), emission-only table, product table, area-only descent,
// product descent, receiver-aware descent. All estimators are unbiased
// (validated in S4-S6), so relative MSE at N samples is relvar / N; the
// measurement per (sampler, receiver) is the per-sample relative variance
// plus the measured draw cost, giving both the equal-sample and the
// equal-time comparison. One occluded pass (self-shadow rays against the S2
// mesh) checks that the ordering survives visibility. The correctness gate:
// every estimate must sit within confidence bounds of its dense-quadrature
// reference — a disagreement is a bug, not a finding.
//
// The Ling estimator targets the tessellated mesh, so each hit is
// reweighted by dA_smooth / dA_mesh = sqrt(det G(u, v)) * |duv| / A_mesh
// (per micro-triangle), which makes its target the same smooth-surface
// integral as the area samplers'.
//
// Weights are recomputed per sample in every descent variant (no caching);
// the equal-time numbers carry that caveat, recorded in the plan.

#include "base_mesh.h"
#include "descent_sampler.h"
#include "displaced_surface.h"
#include "displaced_tessellation.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "ks/rng.h"
#include "line_sampling.h"
#include "texture_grid.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

using namespace ks;

namespace
{

struct StudyReceiver
{
    dmap::Receiver recv;
    double distance; // in mean-edge units, for reporting
    int dir_index;
    bool occluded_study;
};

// cos_r+ * |cos_e| / r^2 for a surface point y with unit normal n_y.
double geometry_term(const dmap::Receiver &recv, const vec3d &y, const vec3d &n_y)
{
    vec3d d = y - recv.x;
    double r2 = std::max(d.squaredNorm(), 1e-12);
    vec3d omega = d / std::sqrt(r2);
    return std::max(recv.n.dot(omega), 0.0) * std::abs(n_y.dot(omega)) / r2;
}

// Segment shadow query against the tessellated mesh. The endpoint margin
// covers the gap between the smooth sample point and the mesh surface; it
// blurs self-shadow boundaries by the same margin (stated with the refs).
bool occluded(const dmap::AllHitsMesh &mesh, const vec3d &x, const vec3d &y, std::vector<dmap::LineHit> &scratch)
{
    vec3d d = y - x;
    double dist = d.norm();
    vec3 dir = (d / dist).cast<float>();
    mesh.all_hits(x.cast<float>(), dir, (float)(1e-4 * dist), (float)(0.98 * dist), scratch);
    return !scratch.empty();
}

// One quadrature sweep evaluating the reference integral for every
// receiver: per-texel midpoint rule with half weight on the hypotenuse
// (the diagonal cuts straddling texels exactly in half; see S6).
std::vector<double> quad_refs(const dmap::BaseTriangle &tri, const dmap::HeightGrid &field,
                              const dmap::TextureGrid &emission, int n, int m_interior, int m_edge,
                              const std::vector<StudyReceiver> &receivers, const dmap::AllHitsMesh *shadow_mesh)
{
    std::vector<double> refs(receivers.size(), 0.0);
    std::vector<dmap::LineHit> scratch;
    double wl = 1.0 / n;
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            if (i + j >= n)
                continue;
            int m = (i + j + 2 <= n) ? m_interior : m_edge;
            double E = emission.values[(size_t)j * n + i];
            double cell_w = (wl * wl) / ((double)m * m);
            for (int b = 0; b < m; ++b)
                for (int a = 0; a < m; ++a) {
                    double u = (i + (a + 0.5) / m) * wl;
                    double v = (j + (b + 0.5) / m) * wl;
                    double s = u + v;
                    double wgt = s < 1.0 ? 1.0 : (s == 1.0 ? 0.5 : 0.0);
                    if (wgt == 0.0)
                        continue;
                    dmap::PointwiseFields f = dmap::pointwise_fields(tri, field, u, v);
                    vec3d y = tri.P(u, v) + f.h * dmap::normal_frame_at(tri, u, v).N;
                    vec3d n_y = f.n.normalized();
                    double base = wgt * E * f.sqrt_det * cell_w;
                    for (size_t r = 0; r < receivers.size(); ++r) {
                        double g = geometry_term(receivers[r].recv, y, n_y);
                        if (g == 0.0)
                            continue;
                        if (shadow_mesh && occluded(*shadow_mesh, receivers[r].recv.x, y, scratch))
                            continue;
                        refs[r] += base * g;
                    }
                }
        }
    return refs;
}

struct Accum
{
    double sum = 0.0, sum_sq = 0.0;
    int64_t n = 0;
    void add(double x)
    {
        sum += x;
        sum_sq += x * x;
        ++n;
    }
    double mean() const { return sum / n; }
    double var() const { return std::max(0.0, sum_sq / n - mean() * mean()) * n / std::max<int64_t>(1, n - 1); }
    double se() const { return std::sqrt(var() / n); }
};

struct SamplerResult
{
    std::string name;
    std::vector<Accum> per_receiver;
    double ns_per_sample = 0.0;
};

double now_ms()
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// One drawn surface point in the common interface of the area samplers.
struct Draw
{
    bool ok; // false: sample outside the domain, contributes zero
    double u, v;
    vec3d y, n_y;
    double inv_pdf_area;
};

// Shared-stream estimation: one sample stream, every receiver's integrand
// evaluated on each sample.
template <typename DrawFn>
SamplerResult run_area_sampler(const std::string &name, DrawFn &&draw, int64_t n_samples,
                               const std::vector<StudyReceiver> &receivers,
                               const std::function<double(double, double)> &E, const dmap::AllHitsMesh *shadow_mesh)
{
    SamplerResult out{name, std::vector<Accum>(receivers.size()), 0.0};
    std::vector<dmap::LineHit> scratch;
    double t0 = now_ms();
    for (int64_t k = 0; k < n_samples; ++k) {
        Draw d = draw();
        if (!d.ok) {
            for (Accum &a : out.per_receiver)
                a.add(0.0);
            continue;
        }
        double base = E(d.u, d.v) * d.inv_pdf_area;
        for (size_t r = 0; r < receivers.size(); ++r) {
            double x = base * geometry_term(receivers[r].recv, d.y, d.n_y);
            if (x != 0.0 && shadow_mesh && occluded(*shadow_mesh, receivers[r].recv.x, d.y, scratch))
                x = 0.0;
            out.per_receiver[r].add(x);
        }
    }
    out.ns_per_sample = (now_ms() - t0) * 1e6 / n_samples;
    return out;
}

} // namespace

void test_weighted_area_sampling(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path mesh_path = args.load_path("mesh");
    fs::path tex_path = args.load_path("texture");
    int tex_nodes = args.load_integer("tex_nodes", 65);
    int n_tess = args.load_integer("n_tess", 64);
    int triangle_index = args.load_integer("triangle_index", -1);
    int n_dirs = args.load_integer("n_dirs", 4);
    int64_t n_samples = args.load_integer("n_samples", 262144);
    int64_t n_samples_recv = args.load_integer("n_samples_recv", 65536);
    int64_t n_lines = args.load_integer("n_lines", 1048576);
    bool with_occlusion = args.load_bool("occlusion", false);
    int64_t n_samples_occ = args.load_integer("n_samples_occ", 131072);
    double beta = (double)args.load_float("beta", 0.05f);
    dmap::PyramidBuild build = dmap::pyramid_build_from_string(args.load_string("pyramid_build", "fold"));
    uint64_t seed = args.load_integer("seed", 2027);
    int m_interior = 8, m_edge = 64; // quadrature points per texel side

    std::vector<double> amplitudes;
    for (int k = 0; k < (int)args["amplitudes"].array_size(); ++k)
        amplitudes.push_back((double)args["amplitudes"].load_float(k));
    std::vector<double> distances; // in mean-edge units
    for (int k = 0; k < (int)args["distances"].array_size(); ++k)
        distances.push_back((double)args["distances"].load_float(k));

    dmap::BaseMesh mesh = dmap::load_base_obj(mesh_path);
    dmap::TextureGrid tex = dmap::downsample_box(dmap::load_height_texture(tex_path), tex_nodes);
    int n_leaf = tex_nodes - 1;
    if (triangle_index < 0)
        triangle_index = mesh.n_triangles() / 3;
    dmap::TextureGrid em = dmap::gaussian_spot(n_leaf, 0.4, 0.3, 0.12, 4.0, 0.1);
    auto E = [&](double u, double v) {
        int i = std::min((int)(u * n_leaf), n_leaf - 1), j = std::min((int)(v * n_leaf), n_leaf - 1);
        return em.values[(size_t)j * n_leaf + i];
    };

    EmbreeDevice device;
    std::ofstream csv(task_dir / "irradiance.csv");
    csv << "texture,amplitude,sampler,receiver,dir,dist_me,occluded,ref,est,se,relvar,ns_per_sample\n";
    bool pass = true;
    double worst_z = 0.0;

    for (double amplitude : amplitudes) {
        dmap::BaseTriangle tri = mesh.triangle(triangle_index);
        double me = dmap::mean_edge(tri);
        dmap::HeightGrid field{tex.W, tex.H, amplitude * me, tex.values.data()};
        get_default_logger().info("=== {} triangle {} x {} amplitude {:.2f} ===", tex_path.filename().string(),
                                  triangle_index, mesh_path.filename().string(), amplitude);

        // Samplers.
        dmap::TaylorPyramid pyramid(field, build);
        dmap::EmissionTile em_tile(em);
        dmap::DescentSampler samp_area(tri, field, pyramid, em_tile, dmap::DescentWeight::AreaOnly, beta);
        dmap::DescentSampler samp_prod(tri, field, pyramid, em_tile, dmap::DescentWeight::Product, beta);
        dmap::DescentSampler samp_geom(tri, field, pyramid, em_tile, dmap::DescentWeight::ProductGeometry, beta);
        dmap::DescentSampler samp_geom_nc(tri, field, pyramid, em_tile, dmap::DescentWeight::ProductGeometry, beta);
        samp_geom_nc.emitter_cosine = false;
        dmap::TexelTableSampler table_em(tri, field, em_tile, /*with_metric*/ false);
        dmap::TexelTableSampler table_prod(tri, field, em_tile, /*with_metric*/ true);

        // S2 mesh: the Ling target and the shadow substrate.
        ks::MeshData md;
        dmap::append_displaced_triangle(md, tri, field, n_tess);
        dmap::finalize_mesh_data(md);
        dmap::AllHitsMesh scene(device, md);
        dmap::LineSampler lines(scene.bound());
        // Per micro-triangle: |duv| / A_mesh, the mesh-to-parameter density.
        std::vector<double> uv_per_area(md.tri_count());
        for (int t = 0; t < md.tri_count(); ++t) {
            vec3d p0 = md.get_pos(md.indices[3 * t]).cast<double>(),
                  p1 = md.get_pos(md.indices[3 * t + 1]).cast<double>(),
                  p2 = md.get_pos(md.indices[3 * t + 2]).cast<double>();
            double area = 0.5 * (p1 - p0).cross(p2 - p0).norm();
            uv_per_area[t] = (0.5 / ((double)n_tess * n_tess)) / area;
        }

        // Receivers: seeded sphere directions at each distance from the
        // displaced centre point; normals face the centre. The occluded
        // study adds grazing receivers (low elevation over the base plane),
        // where the displaced bumps self-shadow.
        dmap::PointwiseFields fc = dmap::pointwise_fields(tri, field, 1.0 / 3.0, 1.0 / 3.0);
        vec3d nc = fc.n.normalized();
        vec3d yc = tri.P(1.0 / 3.0, 1.0 / 3.0) + fc.h * dmap::normal_frame_at(tri, 1.0 / 3.0, 1.0 / 3.0).N;
        std::vector<StudyReceiver> receivers;
        RNG dir_rng(seed + 1);
        for (int k = 0; k < n_dirs; ++k) {
            vec3d omega = sample_uniform_sphere(dir_rng.next2d()).cast<double>();
            for (double dist : distances)
                receivers.push_back({{yc + dist * me * omega, -omega}, dist, k, false});
        }
        std::vector<StudyReceiver> occ_receivers;
        if (with_occlusion) {
            vec3d tangent = (std::abs(nc[0]) < 0.9 ? vec3d(1, 0, 0) : vec3d(0, 1, 0)).cross(nc).normalized();
            vec3d bitangent = nc.cross(tangent);
            const double pi = 3.14159265358979323846;
            double elev = std::sin(15.0 * pi / 180.0), cose = std::cos(15.0 * pi / 180.0);
            for (int k = 0; k < 4; ++k) {
                double phi = 2.0 * pi * (k + 0.5) / 4.0;
                vec3d omega = (cose * (std::cos(phi) * tangent + std::sin(phi) * bitangent) + elev * nc).normalized();
                occ_receivers.push_back({{yc + 1.5 * me * omega, -omega}, 1.5, k, true});
            }
        }

        // References.
        std::vector<double> refs = quad_refs(tri, field, em, n_leaf, m_interior, m_edge, receivers, nullptr);
        std::vector<double> occ_refs;
        if (with_occlusion)
            occ_refs = quad_refs(tri, field, em, n_leaf, m_interior, m_edge, occ_receivers, &scene);

        // One estimation round over a receiver set (unoccluded or occluded).
        auto run_round = [&](const std::vector<StudyReceiver> &recvs, const std::vector<double> &round_refs,
                             const dmap::AllHitsMesh *shadow, int64_t ns, int64_t ns_recv, int64_t nl) {
            RNG rng(seed);
            std::vector<SamplerResult> results;

            // Uniform (Ling): per line, sum over hits of the reweighted
            // integrand; one line is one sample.
            {
                SamplerResult res{"uniform-ling", std::vector<Accum>(recvs.size()), 0.0};
                std::vector<dmap::LineHit> hits, scratch;
                std::vector<double> x_r(recvs.size());
                const double c = 2.0 * lines.offset_area();
                double t0 = now_ms();
                for (int64_t l = 0; l < nl; ++l) {
                    std::fill(x_r.begin(), x_r.end(), 0.0);
                    vec3 o, d;
                    float t_far;
                    if (lines.sample(rng, o, d, t_far)) {
                        scene.all_hits(o, d, 0.0f, t_far, hits);
                        for (const dmap::LineHit &h : hits) {
                            uint32_t i0 = md.indices[3 * h.prim_id], i1 = md.indices[3 * h.prim_id + 1],
                                     i2 = md.indices[3 * h.prim_id + 2];
                            double b0 = 1.0 - h.b1 - h.b2;
                            vec2d uv = b0 * md.get_texcoord(i0).cast<double>() +
                                       (double)h.b1 * md.get_texcoord(i1).cast<double>() +
                                       (double)h.b2 * md.get_texcoord(i2).cast<double>();
                            dmap::PointwiseFields f = dmap::pointwise_fields(tri, field, uv[0], uv[1]);
                            vec3d y = tri.P(uv[0], uv[1]) + f.h * dmap::normal_frame_at(tri, uv[0], uv[1]).N;
                            vec3d n_y = f.n.normalized();
                            double base = E(uv[0], uv[1]) * f.sqrt_det * uv_per_area[h.prim_id];
                            for (size_t r = 0; r < recvs.size(); ++r) {
                                double g = geometry_term(recvs[r].recv, y, n_y);
                                if (g != 0.0 && shadow && occluded(*shadow, recvs[r].recv.x, y, scratch))
                                    g = 0.0;
                                x_r[r] += base * g;
                            }
                        }
                    }
                    for (size_t r = 0; r < recvs.size(); ++r)
                        res.per_receiver[r].add(c * x_r[r]);
                }
                res.ns_per_sample = (now_ms() - t0) * 1e6 / nl;
                results.push_back(std::move(res));
            }

            results.push_back(run_area_sampler(
                "emission-table",
                [&]() {
                    dmap::TableSample s = table_em.sample(rng);
                    return Draw{s.in_domain, s.u, s.v, s.p, s.n, s.in_domain ? 1.0 / s.pdf_area : 0.0};
                },
                ns, recvs, E, shadow));
            results.push_back(run_area_sampler(
                "product-table",
                [&]() {
                    dmap::TableSample s = table_prod.sample(rng);
                    return Draw{s.in_domain, s.u, s.v, s.p, s.n, s.in_domain ? 1.0 / s.pdf_area : 0.0};
                },
                ns, recvs, E, shadow));
            results.push_back(run_area_sampler(
                "area-descent",
                [&]() {
                    dmap::DescentSample s = samp_area.sample(rng);
                    return Draw{true, s.u, s.v, s.p, s.n, 1.0 / s.pdf_area};
                },
                ns, recvs, E, shadow));
            results.push_back(run_area_sampler(
                "product-descent",
                [&]() {
                    dmap::DescentSample s = samp_prod.sample(rng);
                    return Draw{true, s.u, s.v, s.p, s.n, 1.0 / s.pdf_area};
                },
                ns, recvs, E, shadow));

            // Receiver-aware descent: the sample stream depends on the
            // receiver, so it runs per receiver. Two settings: with the
            // midpoint emitter cosine in the weights, and without.
            for (auto [sampler, name] :
                 {std::pair{&samp_geom, "receiver-descent"}, std::pair{&samp_geom_nc, "receiver-descent-nocos"}}) {
                SamplerResult res{name, std::vector<Accum>(recvs.size()), 0.0};
                std::vector<dmap::LineHit> scratch;
                double t_total = 0.0;
                for (size_t r = 0; r < recvs.size(); ++r) {
                    double t0 = now_ms();
                    for (int64_t k = 0; k < ns_recv; ++k) {
                        dmap::DescentSample s = sampler->sample(rng, &recvs[r].recv);
                        double x = E(s.u, s.v) * geometry_term(recvs[r].recv, s.p, s.n) / s.pdf_area;
                        if (x != 0.0 && shadow && occluded(*shadow, recvs[r].recv.x, s.p, scratch))
                            x = 0.0;
                        res.per_receiver[r].add(x);
                    }
                    t_total += now_ms() - t0;
                }
                res.ns_per_sample = t_total * 1e6 / (ns_recv * (int64_t)recvs.size());
                results.push_back(std::move(res));
            }

            // Report: per distance group, geometric-mean relative variance
            // over directions; the correctness gate per (sampler, receiver).
            for (const SamplerResult &res : results) {
                for (size_t r = 0; r < recvs.size(); ++r) {
                    const Accum &a = res.per_receiver[r];
                    double ref = round_refs[r];
                    double relvar = a.var() / (ref * ref);
                    csv << tex_path.stem().string() << "," << amplitude << "," << res.name << "," << r << ","
                        << recvs[r].dir_index << "," << recvs[r].distance << "," << (shadow ? 1 : 0) << "," << ref
                        << "," << a.mean() << "," << a.se() << "," << relvar << "," << res.ns_per_sample << "\n";
                    double z = std::abs(a.mean() - ref) / std::max(a.se(), 1e-300);
                    worst_z = std::max(worst_z, std::abs(a.mean() - ref) / std::max(a.se() + 0.0025 * ref, 1e-300));
                    bool ok = std::abs(a.mean() - ref) <= 4.0 * a.se() + 0.01 * ref && a.se() <= 0.1 * ref;
                    if (!ok)
                        get_default_logger().info("  GATE FAIL {} receiver {} (dist {:.1f}): est {:.6g} +- {:.6g}, "
                                                  "ref {:.6g} ({:.1f} se)",
                                                  res.name, r, recvs[r].distance, a.mean(), a.se(), ref, z);
                    pass &= ok;
                }
            }
            for (double dist : (shadow ? std::vector<double>{1.5} : distances)) {
                std::string row;
                for (const SamplerResult &res : results) {
                    double log_sum = 0.0;
                    int cnt = 0;
                    for (size_t r = 0; r < recvs.size(); ++r)
                        if (recvs[r].distance == dist) {
                            double ref = round_refs[r];
                            log_sum += std::log(std::max(res.per_receiver[r].var() / (ref * ref), 1e-300));
                            ++cnt;
                        }
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%s %.3g  ", res.name.c_str(), std::exp(log_sum / cnt));
                    row += buf;
                }
                get_default_logger().info("  [{}dist {:.1f} me] relvar/sample: {}", shadow ? "occluded, " : "", dist,
                                          row);
            }
            std::string trow;
            for (const SamplerResult &res : results) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%s %.0f  ", res.name.c_str(), res.ns_per_sample);
                trow += buf;
            }
            get_default_logger().info("  draw+eval ns/sample: {}", trow);
        };

        run_round(receivers, refs, nullptr, n_samples, n_samples_recv, n_lines);
        if (with_occlusion) {
            get_default_logger().info("  --- occluded pass (self-shadowing, 4 grazing receivers at 1.5 me) ---");
            run_round(occ_receivers, occ_refs, &scene, n_samples_occ, n_samples_occ / 2, n_lines / 2);
        }

        // Cost table (per unique (face, tile) pair vs per tile; §5A / plan
        // S7). The pyramid's node channels are linear in the amplitude
        // scale, so one unscaled per-tile build serves every face and every
        // amplitude edit; the current code bakes the scale per face for
        // convenience, which does not change the measured build cost.
        {
            auto time_min = [&](const std::function<void()> &fn) {
                double best = 1e300;
                for (int rep = 0; rep < 10; ++rep) {
                    double t0 = now_ms();
                    fn();
                    best = std::min(best, now_ms() - t0);
                }
                return best;
            };
            // Hierarchy build = the shared tile data (pyramid, emission
            // mean and max) plus the triangle's footprint.
            double t_pyr = time_min([&]() { dmap::TaylorPyramid p(field, build); });
            double t_em = time_min([&]() { dmap::EmissionTile e(em); });
            double t_fp = time_min(
                [&]() { dmap::DescentSampler s(tri, field, pyramid, em_tile, dmap::DescentWeight::Product, beta); });
            double t_hier = t_pyr + t_em + t_fp;
            double t_table = time_min([&]() { dmap::TexelTableSampler t(tri, field, em_tile, true); });
            size_t mem_hier = 0;
            for (const dmap::TaylorLevel &lvl : pyramid.levels)
                mem_hier += 8 * lvl.h0.size() * sizeof(double);
            for (const dmap::EmissionTile::Level &lvl : em_tile.levels)
                mem_hier += (lvl.mean.size() + lvl.max.size()) * sizeof(double);
            mem_hier += samp_prod.footprint.nodes.size() * sizeof(dmap::FootprintNode);
            size_t mem_table = table_prod.table.margin.cdf.size() * sizeof(float);
            for (const ks::DistribTable &row : table_prod.table.cond)
                mem_table += row.cdf.size() * sizeof(float);
            get_default_logger().info("  cost per footprint ({}x{} texels): hierarchy build {:.2f} ms ({:.2f} ms "
                                      "pyramid + {:.2f} ms emission sum), {} KB; product table build {:.2f} ms, "
                                      "{} KB",
                                      n_leaf, n_leaf, t_hier, t_pyr, t_hier - t_pyr, mem_hier / 1024, t_table,
                                      mem_table / 1024);
            for (int F : {1, 10, 100})
                get_default_logger().info(
                    "  reuse {:>3}x: table {:.1f} ms / {} KB (amplitude or geometry edit re-bakes all); "
                    "hierarchy {:.2f} ms / {} KB (amplitude edit free, emission edit refolds {:.2f} ms)",
                    F, F * t_table, F * mem_table / 1024, t_hier, mem_hier / 1024, t_hier - t_pyr);
        }
    }

    csv.close();
    get_default_logger().info("wrote {}", (task_dir / "irradiance.csv").string());
    get_default_logger().info("VERDICT: {} — every estimator agrees with its dense-quadrature reference within the "
                              "gate (worst normalized deviation {:.2f}); the variance and cost tables above are the "
                              "S7 result",
                              pass ? "PASS" : "FAIL", worst_z);
    if (!pass)
        std::exit(1);
}
