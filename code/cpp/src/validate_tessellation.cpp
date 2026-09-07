// Phase S2 validation ("Plan — A-MVP sampling implementation"): does the
// summed micro-triangle area of the pre-tessellated displaced mesh converge
// to the metric area integral of the smooth surface as the tessellation
// refines? Mirrors the Phase 0 experiment 2 check at the integral level.
// One question, one verdict.

#include "base_mesh.h"
#include "displaced_surface.h"
#include "displaced_tessellation.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/log_util.h"
#include "texture_grid.h"
#include <cstdlib>
#include <fstream>
#include <vector>

using namespace ks;

namespace
{

void write_obj(const fs::path &path, const ks::MeshData &m)
{
    std::ofstream out(path);
    for (int k = 0; k < m.vertex_count(); ++k) {
        vec3 p = m.get_pos(k);
        out << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
    }
    for (int k = 0; k < m.vertex_count(); ++k) {
        vec3 n = m.get_vertex_normal(k);
        out << "vn " << n[0] << " " << n[1] << " " << n[2] << "\n";
    }
    for (int t = 0; t < m.tri_count(); ++t) {
        uint32_t a = m.indices[3 * t] + 1, b = m.indices[3 * t + 1] + 1, c = m.indices[3 * t + 2] + 1;
        out << "f " << a << "//" << a << " " << b << "//" << b << " " << c << "//" << c << "\n";
    }
}

} // namespace

void validate_tessellation(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path mesh_path = args.load_path("mesh");
    fs::path tex_path = args.load_path("texture");
    int tex_nodes = args.load_integer("tex_nodes", 33);
    int n_triangles = args.load_integer("n_triangles", 8);
    int n_quad = args.load_integer("n_quad", 512);
    double max_final_error = (double)args.load_float("max_final_error", 0.05f);
    bool save_obj = args.load_bool("save_obj", true);

    std::vector<int> levels;
    for (int k = 0; k < (int)args["levels"].array_size(); ++k)
        levels.push_back(args["levels"].load_integer(k));
    std::vector<double> amplitudes;
    for (int k = 0; k < (int)args["amplitudes"].array_size(); ++k)
        amplitudes.push_back((double)args["amplitudes"].load_float(k));

    dmap::BaseMesh mesh = dmap::load_base_obj(mesh_path);
    dmap::TextureGrid tex = dmap::downsample_box(dmap::load_height_texture(tex_path), tex_nodes);
    get_default_logger().info("{} ({} triangles) + {} ({}x{} nodes)", mesh_path.filename().string(), mesh.n_triangles(),
                              tex_path.filename().string(), tex_nodes, tex_nodes);

    // Deterministic triangle picks, evenly spaced over the mesh.
    std::vector<int> picks(n_triangles);
    for (int k = 0; k < n_triangles; ++k)
        picks[k] = (int)((int64_t)k * mesh.n_triangles() / n_triangles);

    bool pass = true;
    for (double amp : amplitudes) {
        double A_metric = 0.0;
        for (int t : picks) {
            dmap::BaseTriangle tri = mesh.triangle(t);
            dmap::HeightGrid field{tex.W, tex.H, amp * dmap::mean_edge(tri), tex.values.data()};
            A_metric += dmap::metric_area(tri, field, n_quad);
        }

        double prev_diff = 0.0, final_diff = 0.0;
        bool monotone = true;
        for (size_t li = 0; li < levels.size(); ++li) {
            int n = levels[li];
            double A_tess = 0.0;
            for (int t : picks) {
                dmap::BaseTriangle tri = mesh.triangle(t);
                dmap::HeightGrid field{tex.W, tex.H, amp * dmap::mean_edge(tri), tex.values.data()};
                ks::MeshData md;
                dmap::append_displaced_triangle(md, tri, field, n);
                dmap::finalize_mesh_data(md);
                A_tess += dmap::mesh_data_area(md);
            }
            double diff = std::abs(A_tess - A_metric) / A_metric;
            get_default_logger().info("  amplitude {:.2f}: n = {:3d}   tessellated {:.6f}   metric {:.6f}   "
                                      "relative diff {:.3e}",
                                      amp, n, A_tess, A_metric, diff);
            if (li > 0 && diff > prev_diff)
                monotone = false;
            prev_diff = diff;
            final_diff = diff;
        }
        if (!monotone || final_diff > max_final_error)
            pass = false;
        get_default_logger().info("  amplitude {:.2f}: monotone {}, final relative diff {:.3e}", amp, monotone,
                                  final_diff);
    }

    if (save_obj) {
        // Submesh of the sampled triangles at the finest level, for eyes.
        double amp = amplitudes.front();
        int n = levels.back();
        ks::MeshData md;
        for (int t : picks) {
            dmap::BaseTriangle tri = mesh.triangle(t);
            dmap::HeightGrid field{tex.W, tex.H, amp * dmap::mean_edge(tri), tex.values.data()};
            dmap::append_displaced_triangle(md, tri, field, n);
        }
        dmap::finalize_mesh_data(md);
        fs::path obj_path = task_dir / (mesh_path.stem().string() + "_displaced.obj");
        write_obj(obj_path, md);
        get_default_logger().info("wrote {}", obj_path.string());
    }

    get_default_logger().info("VERDICT: {} — tessellated area converges to the metric area integral "
                              "(monotone, final diff <= {:.0e}) on {}",
                              pass ? "PASS" : "FAIL", max_final_error, mesh_path.filename().string());
    if (!pass)
        std::exit(1);
}
