// Task describe_scene ("Plan — Path tracer with displaced surfaces", T2):
// loads a scene specification, prints what was attached, and passes when
// (1) the specification has no errors and every `displace` entry resolved
//     to exactly one shape;
// (2) objects that name the same displacement asset share one tile
//     structure (pointer equality of the asset and its pyramid);
// (3) with check_extras, every shape that has both a glTF extras record
//     and a toml overlay entry got the same record from each; and
// (4) with reference_n_tess > 0, the pre-tessellated substitutes build and
//     their area approaches the metric area of the smooth surface.
//
// Config: scene = "scene.<name>" (or the scene keys inline in the task),
// check_extras (false), reference_n_tess (0), area_tolerance (0.02).

#include "displaced_tessellation.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include "ks/material.h"
#include "scene_spec.h"
#include <cmath>
#include <fstream>
#include <map>
#include <string>

using namespace ks;

namespace
{

std::string describe_entry(const dmap::DisplaceEntry &e)
{
    return string_format(
        "displacement=%s uv_scale=(%g, %g) uv_offset=(%g, %g) flip_v=%s material=%s emitter_sampler=%s%s %s",
        e.displacement.c_str(), e.uv_scale[0], e.uv_scale[1], e.uv_offset[0], e.uv_offset[1],
        e.flip_v ? (*e.flip_v ? "true" : "false") : "default", e.material.empty() ? "(object's)" : e.material.c_str(),
        e.emitter_sampler.empty() ? "(default)" : e.emitter_sampler.c_str(), e.emission_tiled ? " emission_tiled" : "",
        dmap::describe(e.traversal).c_str());
}

// The fields the two sources can both set. The glTF side has no flip_v,
// material, sampler or traversal keys; those compare as defaults.
bool same_record(const dmap::DisplaceEntry &a, const dmap::DisplaceEntry &b, std::string &why)
{
    auto strip = [](const std::string &s) {
        return s.rfind("displacement.", 0) == 0 ? s.substr(std::string("displacement.").size()) : s;
    };
    if (strip(a.displacement) != strip(b.displacement))
        why = "displacement " + a.displacement + " vs " + b.displacement;
    else if ((a.uv_scale - b.uv_scale).norm() > 1e-6)
        why =
            string_format("uv_scale (%g, %g) vs (%g, %g)", a.uv_scale[0], a.uv_scale[1], b.uv_scale[0], b.uv_scale[1]);
    else if ((a.uv_offset - b.uv_offset).norm() > 1e-6)
        why = string_format("uv_offset (%g, %g) vs (%g, %g)", a.uv_offset[0], a.uv_offset[1], b.uv_offset[0],
                            b.uv_offset[1]);
    else
        return true;
    return false;
}

} // namespace

void describe_scene(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    bool check_extras = args.load_bool("check_extras", false);
    int reference_n_tess = args.load_integer("reference_n_tess", 0);
    double area_tolerance = args.load_float("area_tolerance", 0.02f);

    std::unique_ptr<dmap::SceneSpec> inline_spec;
    const dmap::SceneSpec *spec = nullptr;
    std::string scene_name;
    if (args.contains("scene")) {
        scene_name = args.load_string("scene");
        spec = args.asset_table().get<dmap::SceneSpec>(scene_name);
        ASSERT(spec, "no scene named [%s]", scene_name.c_str());
    } else {
        scene_name = "(inline)";
        inline_spec = dmap::load_scene_spec(args);
        spec = inline_spec.get();
    }

    std::ofstream report(task_dir / "scene.txt");
    auto out = [&](const std::string &line) {
        get_default_logger().info("{}", line);
        report << line << "\n";
    };

    out(string_format("describe_scene: %s", scene_name.c_str()));
    out(string_format("traversal defaults: %s", dmap::describe(spec->traversal).c_str()));
    for (const dmap::SceneObject &o : spec->objects) {
        out(string_format("object [%s] from %s, %zu shapes", o.name.c_str(), o.asset.c_str(), o.shapes.size()));
        for (int s : o.shapes) {
            const dmap::SceneShape &shape = spec->shapes[s];
            out(string_format("  shape [%s]: %d triangles, %zu instances, texcoords=%s, material=%s%s",
                              shape.name.c_str(), shape.mesh->tri_count(), shape.instances.size(),
                              shape.mesh->has_texcoord() ? "yes" : "no", shape.material ? "yes" : "none",
                              shape.material && shape.material->emission ? " (emits)" : ""));
        }
    }
    out(string_format("%zu toml displace entries, %zu glTF extras entries", spec->entries_toml.size(),
                      spec->entries_gltf.size()));
    for (const dmap::DisplaceEntry &e : spec->entries_toml)
        out(string_format("  toml [%s]: %s", e.object.c_str(), describe_entry(e).c_str()));
    for (const dmap::DisplaceEntry &e : spec->entries_gltf)
        out(string_format("  gltf [%s]: %s", e.object.c_str(), describe_entry(e).c_str()));

    for (const dmap::DisplacedObject &d : spec->displaced) {
        const dmap::SceneShape &shape = spec->shapes[d.shape];
        out(string_format("displaced [%s] (%s): asset [%s] %s", shape.name.c_str(), d.source.c_str(),
                          d.asset_name.c_str(), d.asset->describe().c_str()));
        double lo_u = INFINITY, lo_v = INFINITY, hi_u = -INFINITY, hi_v = -INFINITY;
        for (const dmap::BaseTriangle &tri : d.triangles)
            for (const vec2d &t : {tri.t0, tri.t1, tri.t2}) {
                lo_u = std::min(lo_u, t[0]);
                hi_u = std::max(hi_u, t[0]);
                lo_v = std::min(lo_v, t[1]);
                hi_v = std::max(hi_v, t[1]);
            }
        out(string_format("  uv_scale=(%g, %g) uv_offset=(%g, %g) flip_v=%s; %zu triangles, tile domain [%g, %g] x "
                          "[%g, %g]; material=%s; emitter_sampler=%s; %s",
                          d.uv_scale[0], d.uv_scale[1], d.uv_offset[0], d.uv_offset[1], d.flip_v ? "true" : "false",
                          d.triangles.size(), lo_u, hi_u, lo_v, hi_v, d.material ? "yes" : "none",
                          dmap::emitter_sampler_name(d.emitter_sampler), dmap::describe(d.traversal).c_str()));
        if (d.emission && d.emission->repeat)
            out(string_format("  emission tile: %d x %d cells, mass per tile %.6g", d.emission->n, d.emission->n,
                              d.emission->sum(d.emission->n_levels - 1, 0, 0)));
        else if (d.emission)
            out(string_format(
                "  emission box: %d x %d cells at (%lld, %lld), total mass %.6g", d.emission->n, d.emission->n,
                (long long)d.emission->i0, (long long)d.emission->j0,
                d.emission->sum(d.emission->n_levels, d.emission->i0 / d.emission->n, d.emission->j0 / d.emission->n)));
    }
    out(string_format("camera: %s; sky: %s; %zu lights", spec->camera ? "yes" : "none", spec->sky ? "yes" : "none",
                      spec->lights.size()));

    bool pass = true;
    // (1)
    for (const std::string &e : spec->errors)
        out("error: " + e);
    // Every entry names one shape, and a shape named by both sources is one
    // record, so the records number the entries minus the overlaps.
    size_t n_overlap = 0;
    for (const dmap::DisplacedObject &d : spec->displaced)
        n_overlap += d.source == "toml over gltf" ? 1 : 0;
    bool resolved = spec->errors.empty() &&
                    spec->displaced.size() + n_overlap == spec->entries_toml.size() + spec->entries_gltf.size();
    out(string_format("(1) specification valid and entries resolved: %s (%zu displaced objects from %zu entries, "
                      "%zu errors)",
                      resolved ? "yes" : "NO", spec->displaced.size(),
                      spec->entries_toml.size() + spec->entries_gltf.size(), spec->errors.size()));
    pass = pass && resolved;

    // (2)
    bool shared = true;
    std::map<std::string, const dmap::DisplacementAsset *> first_asset;
    for (const dmap::DisplacedObject &d : spec->displaced) {
        auto it = first_asset.find(d.asset_name);
        if (it == first_asset.end()) {
            first_asset[d.asset_name] = d.asset;
            continue;
        }
        bool same = it->second == d.asset && it->second->pyramid.get() == d.asset->pyramid.get() &&
                    it->second->field.values == d.field.values;
        if (!same)
            out(string_format("  asset [%s] is not shared by [%s]", d.asset_name.c_str(),
                              spec->shapes[d.shape].name.c_str()));
        shared = shared && same;
    }
    out(string_format("(2) objects sharing an asset share its tile data: %s (%zu assets, %zu displaced objects)",
                      shared ? "yes" : "NO", first_asset.size(), spec->displaced.size()));
    pass = pass && shared;

    // (3)
    if (check_extras) {
        int n_pairs = 0, n_same = 0;
        for (const dmap::DisplaceEntry &t : spec->entries_toml) {
            for (const dmap::DisplaceEntry &g : spec->entries_gltf) {
                // Same shape: the toml name resolves to the full glTF name or its node name.
                bool suffix = g.object.size() > t.object.size() &&
                              g.object.compare(g.object.size() - t.object.size(), t.object.size(), t.object) == 0 &&
                              g.object[g.object.size() - t.object.size() - 1] == '/';
                bool same_object = t.object == g.object || suffix;
                if (!same_object)
                    continue;
                ++n_pairs;
                std::string why;
                if (same_record(t, g, why))
                    ++n_same;
                else
                    out(string_format("  [%s]: extras and overlay differ: %s", g.object.c_str(), why.c_str()));
            }
        }
        bool ok = n_pairs > 0 && n_same == n_pairs;
        out(string_format("(3) glTF extras give the overlay's record: %s (%d of %d shapes agree)", ok ? "yes" : "NO",
                          n_same, n_pairs));
        pass = pass && ok;
    }

    // (4) The substitute's area converges to the metric area of the smooth
    // surface as the subdivision doubles (a chord approximation from below
    // on rough content); the metric area uses a 4 x finer quadrature.
    if (reference_n_tess > 0 && spec->errors.empty()) {
        bool ok = true;
        for (const dmap::DisplacedObject &d : spec->displaced) {
            double smooth_area = 0.0;
            for (const dmap::BaseTriangle &tri : d.triangles)
                smooth_area += dmap::metric_area(tri, d.field, 4 * reference_n_tess);
            double err[2];
            int n_tris[2];
            for (int k = 0; k < 2; ++k) {
                MeshData mesh = spec->tessellated(d, reference_n_tess << k);
                n_tris[k] = mesh.tri_count();
                err[k] = std::abs(dmap::mesh_data_area(mesh) - smooth_area) / smooth_area;
            }
            bool converging = err[1] < err[0] || err[1] <= area_tolerance;
            out(string_format("  substitute [%s]: %d triangles at n_tess=%d (area rel err %.2e), %d at n_tess=%d "
                              "(%.2e), metric area %.6g: %s",
                              spec->shapes[d.shape].name.c_str(), n_tris[0], reference_n_tess, err[0], n_tris[1],
                              2 * reference_n_tess, err[1], smooth_area, converging ? "converging" : "NOT converging"));
            ok = ok && converging;
        }
        out(string_format("(4) pre-tessellated substitutes converge to the metric area: %s", ok ? "yes" : "NO"));
        pass = pass && ok;
    }

    out(string_format("describe_scene %s: %s", scene_name.c_str(), pass ? "PASS" : "FAIL"));
}
