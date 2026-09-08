#include "scene_spec.h"
#include "displaced_tessellation.h"
#include "ks/assertion.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include "ks/material.h"
#include "ks/shader_field.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

namespace
{

// "mesh_asset.torus" -> "torus"; a bare key is returned as is.
std::string strip_prefix(const std::string &name, const std::string &prefix)
{
    std::string p = prefix + ".";
    return name.rfind(p, 0) == 0 ? name.substr(p.size()) : name;
}

std::string with_prefix(const std::string &name, const std::string &prefix)
{
    return prefix + "." + strip_prefix(name, prefix);
}

int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

// Area-weighted vertex normals: the summed cross products (twice the face
// areas), normalized at the end.
std::vector<vec3d> area_weighted_vertex_normals(const std::vector<vec3d> &positions,
                                                const std::vector<Eigen::Vector3i> &faces)
{
    std::vector<vec3d> normals(positions.size(), vec3d::Zero());
    for (const auto &f : faces) {
        vec3d n = (positions[f[1]] - positions[f[0]]).cross(positions[f[2]] - positions[f[0]]);
        normals[f[0]] += n;
        normals[f[1]] += n;
        normals[f[2]] += n;
    }
    for (auto &n : normals)
        n.normalize();
    return normals;
}

std::vector<const ks::Material *> material_list(const ks::ConfigArgs &list, const ks::ConfigurableTable &table,
                                                std::vector<std::string> &errors)
{
    std::vector<const ks::Material *> materials((size_t)list.array_size(), nullptr);
    for (size_t k = 0; k < materials.size(); ++k) {
        std::string raw = list.load_string((int)k);
        if (raw.empty())
            continue; // keeps the asset's own material
        std::string name = with_prefix(raw, "material");
        materials[k] = table.get<ks::Material>(name);
        if (!materials[k])
            errors.push_back("no material named [" + name + "]");
    }
    return materials;
}

void add_mesh_asset_object(SceneSpec &spec, const std::string &name, const std::string &asset_key,
                           const ks::MeshAsset &asset, const ks::Transform &to_world,
                           const std::vector<const ks::Material *> &materials)
{
    if (!materials.empty() && materials.size() != asset.meshes.size())
        spec.errors.push_back(ks::string_format("object [%s]: %zu materials for %zu shapes", name.c_str(),
                                                materials.size(), asset.meshes.size()));
    SceneObject object;
    object.name = name;
    object.asset = asset_key;
    object.mesh_asset = &asset;
    object.to_world = to_world;
    for (size_t k = 0; k < asset.meshes.size(); ++k) {
        SceneShape shape;
        shape.short_name = k < asset.mesh_names.size() ? asset.mesh_names[k] : "";
        shape.name = shape.short_name.empty() && asset.meshes.size() == 1 ? name : name + "/" + shape.short_name;
        shape.object = (int)spec.objects.size();
        shape.mesh = asset.meshes[k].get();
        if (k < materials.size())
            shape.material = materials[k];
        else if (k < asset.materials.size())
            shape.material = asset.materials[k].get();
        shape.instances = {to_world};
        shape.prototype = 0;
        shape.mesh_index = (int)k;
        shape.from_gltf = false;
        object.shapes.push_back((int)spec.shapes.size());
        spec.shapes.push_back(std::move(shape));
    }
    spec.objects.push_back(std::move(object));
}

// One shape per (node with a mesh, primitive). Two nodes instancing the
// same glTF mesh are two shapes that share their MeshData; a later phase
// that needs them to differ duplicates the prototype (T6).
void add_gltf_object(SceneSpec &spec, const std::string &name, const std::string &asset_key, const GltfAsset &asset,
                     const ks::Transform &to_world, const std::vector<const ks::Material *> &materials)
{
    if (!materials.empty() && materials.size() != asset.instances.size())
        spec.errors.push_back(ks::string_format("object [%s]: %zu materials for %zu nodes with a mesh", name.c_str(),
                                                materials.size(), asset.instances.size()));
    SceneObject object;
    object.name = name;
    object.asset = asset_key;
    object.gltf = &asset;
    object.to_world = to_world;
    for (size_t k = 0; k < asset.nodes.size(); ++k) {
        const GltfAsset::Node &node = asset.nodes[k];
        const ks::MeshAsset &prototype = asset.prototypes[node.mesh];
        const ks::Transform &local = asset.instances[k].second;
        for (size_t m = 0; m < prototype.meshes.size(); ++m) {
            SceneShape shape;
            shape.short_name = node.name;
            shape.name = name + "/" + node.name;
            if (prototype.meshes.size() > 1)
                shape.name += "/" + std::to_string(m);
            shape.object = (int)spec.objects.size();
            shape.mesh = prototype.meshes[m].get();
            shape.material = m < prototype.materials.size() ? prototype.materials[m].get() : nullptr;
            if (k < materials.size() && materials[k])
                shape.material = materials[k]; // the scene's override, per node
            shape.instances = {to_world * local};
            shape.prototype = node.mesh;
            shape.mesh_index = (int)m;
            shape.from_gltf = true;
            object.shapes.push_back((int)spec.shapes.size());
            spec.shapes.push_back(std::move(shape));
        }
    }
    spec.objects.push_back(std::move(object));
}

void add_object(SceneSpec &spec, const ks::ConfigurableTable &table, const std::string &asset_name,
                const std::string &name_or_empty, const ks::Transform &to_world,
                const std::vector<const ks::Material *> &materials)
{
    std::string name = name_or_empty;
    if (const ks::MeshAsset *mesh_asset = table.get<ks::MeshAsset>(with_prefix(asset_name, "mesh_asset"))) {
        std::string key = with_prefix(asset_name, "mesh_asset");
        if (name.empty())
            name = strip_prefix(key, "mesh_asset");
        add_mesh_asset_object(spec, name, key, *mesh_asset, to_world, materials);
    } else if (const GltfAsset *gltf = table.get<GltfAsset>(with_prefix(asset_name, "compound_mesh_asset"))) {
        std::string key = with_prefix(asset_name, "compound_mesh_asset");
        if (name.empty())
            name = strip_prefix(key, "compound_mesh_asset");
        add_gltf_object(spec, name, key, *gltf, to_world, materials);
    } else {
        spec.errors.push_back("no mesh_asset or compound_mesh_asset named [" + asset_name + "]");
    }
}

DisplaceEntry parse_displace_entry(const ks::ConfigArgs &a, const TraversalOptions &base)
{
    DisplaceEntry e;
    e.object = a.load_string("object");
    e.displacement = a.load_string("displacement");
    double uv_scale = a.load_float("uv_scale", 1.0f);
    e.uv_scale = vec2d(uv_scale, a.load_float("uv_scale_v", (float)uv_scale));
    e.uv_offset = a.load_vec2("uv_offset", false, ks::vec2::Zero()).cast<double>();
    if (a.contains("flip_v"))
        e.flip_v = a.load_bool("flip_v");
    e.material = a.load_string("material", "");
    e.emitter_sampler = a.load_string("emitter_sampler", "");
    e.emission_tiled = a.load_bool("emission_tiled", false);
    e.emission_supersample = a.load_integer("emission_supersample", 4);
    e.traversal = a.contains("traversal") ? parse_traversal_options(a["traversal"], base) : base;
    e.source = "toml";
    return e;
}

DisplaceEntry entry_from_extras(const std::string &object, const GltfDisplaceExtras &x, const TraversalOptions &base)
{
    DisplaceEntry e;
    e.object = object;
    e.displacement = *x.displacement;
    double uv_scale = x.uv_scale.value_or(1.0);
    e.uv_scale = vec2d(uv_scale, x.uv_scale_v.value_or(uv_scale));
    e.uv_offset = x.uv_offset.value_or(vec2d::Zero());
    e.strength = x.strength;
    e.midlevel = x.midlevel;
    e.traversal = base;
    e.source = "gltf";
    return e;
}

// The shapes an entry's object name denotes: the full name, else the
// shape or node name, else an object with exactly one shape.
std::vector<int> match_shapes(const SceneSpec &spec, const std::string &object)
{
    std::vector<int> hits;
    for (size_t i = 0; i < spec.shapes.size(); ++i)
        if (spec.shapes[i].name == object)
            hits.push_back((int)i);
    if (!hits.empty())
        return hits;
    for (size_t i = 0; i < spec.shapes.size(); ++i)
        if (!spec.shapes[i].short_name.empty() && spec.shapes[i].short_name == object)
            hits.push_back((int)i);
    if (!hits.empty())
        return hits;
    for (const SceneObject &o : spec.objects)
        if (o.name == object && o.shapes.size() == 1)
            hits.push_back(o.shapes[0]);
    return hits;
}

const DisplacementAsset *resolve_asset(SceneSpec &spec, const ks::ConfigurableTable &table, const DisplaceEntry &e,
                                       const SceneShape &shape)
{
    std::string key = strip_prefix(e.displacement, "displacement");
    const DisplacementAsset *asset = table.get<DisplacementAsset>("displacement." + key);
    if (!asset) {
        for (const auto &owned : spec.owned_assets)
            if (owned->name == key)
                asset = owned.get();
    }
    if (asset) {
        auto agrees = [](double a, double b) { return std::abs(a - b) <= 1e-6 * std::max(1.0, std::abs(b)); };
        if (e.strength && !agrees(*e.strength, asset->strength))
            spec.errors.push_back(ks::string_format(
                "[%s]: dmap_strength %g differs from the asset's %g; per-object strength is not supported (D9)",
                shape.name.c_str(), *e.strength, asset->strength));
        if (e.midlevel && !agrees(*e.midlevel, asset->midlevel))
            spec.errors.push_back(ks::string_format("[%s]: dmap_midlevel %g differs from the asset's %g",
                                                    shape.name.c_str(), *e.midlevel, asset->midlevel));
        return asset;
    }
    if (e.source != "gltf") {
        spec.errors.push_back("[" + shape.name + "]: no [displacement." + key + "] asset");
        return nullptr;
    }
    // A glTF object naming no toml asset: the string is a map path relative
    // to the glTF file, and the extras give the height convention.
    const SceneObject &object = spec.objects[shape.object];
    fs::path map = object.gltf->path.parent_path() / e.displacement;
    if (!fs::exists(map)) {
        spec.errors.push_back("[" + shape.name + "]: dmap_displacement [" + e.displacement +
                              "] is neither a [displacement.*] asset nor a file next to the glTF");
        return nullptr;
    }
    spec.owned_assets.push_back(std::make_unique<DisplacementAsset>(
        key, map, e.strength.value_or(1.0), e.midlevel.value_or(0.5), true, 0, PyramidBuild::Fold));
    return spec.owned_assets.back().get();
}

// The aligned box of leaf cells holding the object's emission: the
// smallest power-of-two square, aligned to its size, that contains the
// domain's bounding box.
void build_emission(SceneSpec &spec, DisplacedObject &d, const SceneShape &shape, const DisplaceEntry &e)
{
    int n_leaf = d.asset->resolution;
    if (e.emission_tiled) {
        // One tile of the emission, read through the inverse chart, which
        // repeats it exactly when the material's textures tile with the
        // displacement (the repeating layout of emission_tile.h).
        TextureGrid grid = emission_from_field(
            *d.material->emission, n_leaf, 0, 0, n_leaf, [&](const vec2d &t) { return d.object_uv(t); },
            e.emission_supersample);
        d.emission = std::make_unique<EmissionTile>(grid);
        return;
    }
    double lo_u = INFINITY, lo_v = INFINITY, hi_u = -INFINITY, hi_v = -INFINITY;
    for (const BaseTriangle &tri : d.triangles) {
        for (const vec2d &t : {tri.t0, tri.t1, tri.t2}) {
            lo_u = std::min(lo_u, t[0]);
            hi_u = std::max(hi_u, t[0]);
            lo_v = std::min(lo_v, t[1]);
            hi_v = std::max(hi_v, t[1]);
        }
    }
    int64_t i_lo = (int64_t)std::floor(lo_u * n_leaf), i_hi = (int64_t)std::ceil(hi_u * n_leaf);
    int64_t j_lo = (int64_t)std::floor(lo_v * n_leaf), j_hi = (int64_t)std::ceil(hi_v * n_leaf);
    int64_t size = 1;
    while (size < std::max(i_hi - i_lo, j_hi - j_lo))
        size *= 2;
    int64_t i0, j0;
    while (true) {
        i0 = floor_div(i_lo, size) * size;
        j0 = floor_div(j_lo, size) * size;
        if (i_hi <= i0 + size && j_hi <= j0 + size)
            break;
        size *= 2;
    }
    constexpr int64_t max_size = 4096;
    if (size > max_size) {
        spec.errors.push_back(ks::string_format("[%s]: emission box of %lld cells per side exceeds %lld",
                                                shape.name.c_str(), (long long)size, (long long)max_size));
        return;
    }
    TextureGrid grid = emission_from_field(
        *d.material->emission, n_leaf, i0, j0, (int)size, [&](const vec2d &t) { return d.object_uv(t); },
        e.emission_supersample);
    d.emission = std::make_unique<EmissionTile>(grid, n_leaf, i0, j0);
}

void build_displaced(SceneSpec &spec, const ks::ConfigurableTable &table, int shape_index, const DisplaceEntry &e,
                     const std::string &source)
{
    const SceneShape &shape = spec.shapes[shape_index];
    DisplacedObject d;
    d.shape = shape_index;
    d.source = source;
    d.asset_name = strip_prefix(e.displacement, "displacement");
    d.asset = resolve_asset(spec, table, e, shape);
    if (!d.asset)
        return;
    d.uv_scale = e.uv_scale;
    if (d.uv_scale[0] == 0.0 || d.uv_scale[1] == 0.0)
        spec.errors.push_back("[" + shape.name + "]: uv_scale must be nonzero");
    d.uv_offset = e.uv_offset;
    d.flip_v = e.flip_v.value_or(!shape.from_gltf);
    d.material = shape.material;
    if (!e.material.empty()) {
        d.material = table.get<ks::Material>(with_prefix(e.material, "material"));
        if (!d.material)
            spec.errors.push_back("[" + shape.name + "]: no material named [" + e.material + "]");
    }
    if (!e.emitter_sampler.empty())
        d.emitter_sampler = emitter_sampler_from_string(e.emitter_sampler);
    d.traversal = e.traversal;

    if (!shape.mesh->has_texcoord()) {
        spec.errors.push_back("[" + shape.name + "]: no texture coordinates; displacement needs a chart");
        return;
    }
    d.base = base_mesh_from_mesh_data(*shape.mesh);
    d.field = d.asset->field;
    d.triangles.reserve(d.base.faces.size());
    int n_degenerate = 0, n_outside = 0;
    for (size_t t = 0; t < d.base.faces.size(); ++t) {
        const Eigen::Vector3i &f = d.base.faces[t];
        vec2d tc[3];
        for (int k = 0; k < 3; ++k)
            tc[k] = d.tile_coords(d.base.texcoords[f[k]]);
        Eigen::Matrix2d T;
        T.col(0) = tc[1] - tc[0];
        T.col(1) = tc[2] - tc[0];
        if (std::abs(T.determinant()) <= 1e-30) {
            ++n_degenerate;
            continue;
        }
        if (!d.asset->repeat) {
            for (int k = 0; k < 3; ++k)
                if (tc[k][0] < -1e-6 || tc[k][0] > 1.0 + 1e-6 || tc[k][1] < -1e-6 || tc[k][1] > 1.0 + 1e-6)
                    ++n_outside;
        }
        d.triangles.emplace_back(d.base.positions[f[0]], d.base.positions[f[1]], d.base.positions[f[2]],
                                 d.base.normals[f[0]], d.base.normals[f[1]], d.base.normals[f[2]], tc[0], tc[1], tc[2]);
    }
    if (n_degenerate > 0)
        spec.errors.push_back(ks::string_format("[%s]: %d of %zu triangles are degenerate in texture space",
                                                shape.name.c_str(), n_degenerate, d.base.faces.size()));
    if (n_outside > 0)
        spec.errors.push_back(ks::string_format(
            "[%s]: %d triangle corners leave the tile under wrap = clamp; use repeat or fit the layout to [0, 1]^2",
            shape.name.c_str(), n_outside));
    if (d.material && d.material->emission && spec.errors.empty())
        build_emission(spec, d, shape, e);
    spec.displaced.push_back(std::move(d));
}

} // namespace

vec2d DisplacedObject::tile_coords(const vec2d &uv) const
{
    vec2d t(uv[0], flip_v ? 1.0 - uv[1] : uv[1]);
    return t.cwiseProduct(uv_scale) + uv_offset;
}

vec2d DisplacedObject::object_uv(const vec2d &t) const
{
    vec2d uv = (t - uv_offset).cwiseQuotient(uv_scale);
    if (flip_v)
        uv[1] = 1.0 - uv[1];
    return uv;
}

void SceneSpec::require_valid() const
{
    for (const std::string &e : errors)
        fprintf(stderr, "scene error: %s\n", e.c_str());
    ASSERT(errors.empty(), "the scene specification has %zu errors", errors.size());
}

ks::MeshData SceneSpec::tessellated(const DisplacedObject &d, int n) const
{
    ks::MeshData out;
    for (const BaseTriangle &tri : d.triangles)
        append_displaced_triangle(out, tri, d.field, n);
    for (size_t k = 0; k + 1 < out.texcoords.size(); k += 2) {
        vec2d uv = d.object_uv(vec2d(out.texcoords[k], out.texcoords[k + 1]));
        out.texcoords[k] = (float)uv[0];
        out.texcoords[k + 1] = (float)uv[1];
    }
    finalize_mesh_data(out);
    return out;
}

ks::MeshData SceneSpec::texel_tessellated(const DisplacedObject &d, int m, std::vector<TexelFace> &faces) const
{
    ks::MeshData out;
    faces.clear();
    for (size_t k = 0; k < d.triangles.size(); ++k)
        append_texel_tessellation(out, d.triangles[k], d.field, d.asset->pyramid->n_leaf, m, (int)k, faces);
    for (size_t k = 0; k + 1 < out.texcoords.size(); k += 2) {
        vec2d uv = d.object_uv(vec2d(out.texcoords[k], out.texcoords[k + 1]));
        out.texcoords[k] = (float)uv[0];
        out.texcoords[k + 1] = (float)uv[1];
    }
    finalize_mesh_data(out);
    return out;
}

BaseMesh base_mesh_from_mesh_data(const ks::MeshData &data)
{
    BaseMesh mesh;
    int n_vertices = data.vertex_count();
    mesh.positions.reserve(n_vertices);
    for (int i = 0; i < n_vertices; ++i)
        mesh.positions.push_back(data.get_pos(i).cast<double>());
    if (data.has_texcoord()) {
        mesh.texcoords.reserve(n_vertices);
        for (int i = 0; i < n_vertices; ++i)
            mesh.texcoords.push_back(data.get_texcoord(i).cast<double>());
    }
    int n_faces = data.tri_count();
    mesh.faces.reserve(n_faces);
    for (int t = 0; t < n_faces; ++t)
        mesh.faces.emplace_back((int)data.indices[3 * t], (int)data.indices[3 * t + 1], (int)data.indices[3 * t + 2]);
    if (data.has_vertex_normal()) {
        mesh.normals.reserve(n_vertices);
        for (int i = 0; i < n_vertices; ++i)
            mesh.normals.push_back(data.get_vertex_normal(i).cast<double>().normalized());
    } else {
        mesh.normals = area_weighted_vertex_normals(mesh.positions, mesh.faces);
    }
    mesh.face_normal_indices = mesh.faces;
    mesh.face_texcoord_indices.assign(mesh.faces.size(), Eigen::Vector3i(-1, -1, -1));
    if (data.has_texcoord())
        mesh.face_texcoord_indices = mesh.faces;
    return mesh;
}

EmitterSamplerKind emitter_sampler_from_string(const std::string &name)
{
    if (name == "emission_table")
        return EmitterSamplerKind::EmissionTable;
    if (name == "product_table")
        return EmitterSamplerKind::ProductTable;
    if (name == "area_descent")
        return EmitterSamplerKind::AreaDescent;
    if (name == "product_descent")
        return EmitterSamplerKind::ProductDescent;
    if (name == "receiver_descent")
        return EmitterSamplerKind::ReceiverDescent;
    ASSERT(false,
           "emitter_sampler must be emission_table, product_table, area_descent, product_descent or "
           "receiver_descent, got [%s]",
           name.c_str());
    return EmitterSamplerKind::ProductDescent;
}

const char *emitter_sampler_name(EmitterSamplerKind kind)
{
    switch (kind) {
    case EmitterSamplerKind::EmissionTable:
        return "emission_table";
    case EmitterSamplerKind::ProductTable:
        return "product_table";
    case EmitterSamplerKind::AreaDescent:
        return "area_descent";
    case EmitterSamplerKind::ProductDescent:
        return "product_descent";
    case EmitterSamplerKind::ReceiverDescent:
        return "receiver_descent";
    }
    return "?";
}

std::unique_ptr<SceneSpec> load_scene_spec(const ks::ConfigArgs &args)
{
    std::unique_ptr<SceneSpec> spec = std::make_unique<SceneSpec>();
    const ks::ConfigurableTable &table = args.asset_table();

    if (args.contains("traversal"))
        spec->traversal = parse_traversal_options(args["traversal"], TraversalOptions());

    // Objects: ks's single-asset form, then the list.
    std::vector<std::vector<const ks::Material *>> ks_materials;
    if (args.contains("material")) {
        ks::ConfigArgs lists = args["material"];
        for (int i = 0; i < (int)lists.array_size(); ++i)
            ks_materials.push_back(material_list(lists[i], table, spec->errors));
    }
    size_t ks_object = 0;
    auto next_ks_materials = [&]() {
        std::vector<const ks::Material *> m;
        if (ks_object < ks_materials.size())
            m = ks_materials[ks_object];
        ++ks_object;
        return m;
    };
    if (args.contains("object"))
        add_object(*spec, table, args.load_string("object"), "", ks::Transform(), next_ks_materials());
    if (args.contains("compound_object"))
        add_object(*spec, table, args.load_string("compound_object"), "", ks::Transform(), next_ks_materials());
    if (args.contains("objects")) {
        ks::ConfigArgs list = args["objects"];
        for (int i = 0; i < (int)list.array_size(); ++i) {
            ks::ConfigArgs entry = list[i];
            std::vector<const ks::Material *> materials;
            if (entry.contains("material"))
                materials = material_list(entry["material"], table, spec->errors);
            add_object(*spec, table, entry.load_string("asset"), entry.load_string("name", ""),
                       entry.load_transform("to_world", ks::Transform()), materials);
        }
    }

    // Displacement records from both sources.
    for (const SceneObject &object : spec->objects) {
        if (!object.gltf)
            continue;
        // Shapes of a glTF object are in node order, prototype.meshes.size() per node.
        size_t s = 0;
        for (const GltfAsset::Node &node : object.gltf->nodes) {
            size_t n_meshes = object.gltf->prototypes[node.mesh].meshes.size();
            if (node.extras.displacement) {
                for (size_t m = 0; m < n_meshes; ++m)
                    spec->entries_gltf.push_back(
                        entry_from_extras(spec->shapes[object.shapes[s + m]].name, node.extras, spec->traversal));
            } else if (!node.extras.empty()) {
                spec->errors.push_back("[" + object.name + "/" + node.name +
                                       "]: dmap_* extras without dmap_displacement");
            }
            s += n_meshes;
        }
    }
    if (args.contains("displace")) {
        ks::ConfigArgs list = args["displace"];
        for (int i = 0; i < (int)list.array_size(); ++i)
            spec->entries_toml.push_back(parse_displace_entry(list[i], spec->traversal));
    }

    // Resolve: extras first, the toml overlay replaces them per shape.
    std::map<int, std::pair<DisplaceEntry, std::string>> by_shape;
    for (const DisplaceEntry &e : spec->entries_gltf) {
        std::vector<int> hits = match_shapes(*spec, e.object);
        ASSERT(hits.size() == 1, "extras entry [%s] must name one shape", e.object.c_str());
        by_shape[hits[0]] = {e, "gltf"};
    }
    for (const DisplaceEntry &e : spec->entries_toml) {
        std::vector<int> hits = match_shapes(*spec, e.object);
        if (hits.size() != 1) {
            std::string names;
            for (int h : hits)
                names += " [" + spec->shapes[h].name + "]";
            spec->errors.push_back(
                ks::string_format("displace [%s]: matches %zu shapes%s", e.object.c_str(), hits.size(), names.c_str()));
            continue;
        }
        auto it = by_shape.find(hits[0]);
        if (it != by_shape.end() && it->second.second == "toml") {
            spec->errors.push_back("displace [" + e.object + "]: listed twice");
            continue;
        }
        by_shape[hits[0]] = {e, it == by_shape.end() ? "toml" : "toml over gltf"};
    }
    for (const auto &[shape_index, record] : by_shape)
        build_displaced(*spec, table, shape_index, record.first, record.second);

    if (args.contains("camera"))
        spec->camera = ks::create_camera(args["camera"]);
    if (args.contains("sky"))
        spec->sky = ks::create_sky_light(args["sky"]);
    if (args.contains("light")) {
        ks::ConfigArgs list = args["light"];
        for (int i = 0; i < (int)list.array_size(); ++i)
            spec->lights.push_back(ks::create_light(list[i]));
    }
    return spec;
}

std::unique_ptr<SceneSpec> create_scene_spec(const ks::ConfigArgs &args)
{
    std::unique_ptr<SceneSpec> spec = load_scene_spec(args);
    for (const std::string &e : spec->errors)
        fprintf(stderr, "scene error: %s\n", e.c_str());
    return spec;
}

} // namespace dmap
