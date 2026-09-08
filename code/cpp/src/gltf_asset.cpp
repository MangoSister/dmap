#include "gltf_asset.h"
#include "ks/assertion.h"
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tiny_gltf.h>

namespace dmap
{

namespace
{

GltfDisplaceExtras read_extras(const tinygltf::Value &extras)
{
    GltfDisplaceExtras out;
    if (!extras.IsObject())
        return out;
    auto number = [&](const char *key) -> std::optional<double> {
        if (extras.Has(key) && extras.Get(key).IsNumber())
            return extras.Get(key).GetNumberAsDouble();
        return std::nullopt;
    };
    if (extras.Has("dmap_displacement") && extras.Get("dmap_displacement").IsString())
        out.displacement = extras.Get("dmap_displacement").Get<std::string>();
    out.strength = number("dmap_strength");
    out.midlevel = number("dmap_midlevel");
    out.uv_scale = number("dmap_uv_scale");
    out.uv_scale_v = number("dmap_uv_scale_v");
    if (extras.Has("dmap_uv_offset")) {
        const tinygltf::Value &v = extras.Get("dmap_uv_offset");
        if (v.IsArray() && v.ArrayLen() == 2 && v.Get(0).IsNumber() && v.Get(1).IsNumber())
            out.uv_offset = ks::vec2d(v.Get(0).GetNumberAsDouble(), v.Get(1).GetNumberAsDouble());
    }
    return out;
}

} // namespace

std::unique_ptr<GltfAsset> create_gltf_asset(const ks::ConfigArgs &args)
{
    // The keys of ks::create_compound_mesh_asset.
    std::unique_ptr<GltfAsset> asset = std::make_unique<GltfAsset>();
    asset->path = args.load_path("path");
    std::string fmt = args.load_string("format", "glb");
    ASSERT(fmt == "glb" || fmt == "gltf", "Unsupported compound mesh asset format [%s].", fmt.c_str());
    ks::CompoundMeshAsset::LoadMaterialOptions options;
    options.enable = args.load_bool("load_materials");
    options.bsdf_type = args.load_string("bsdf_type", "principled_bsdf") == "principled_bsdf"
                            ? ks::CompoundMeshAsset::LoadMaterialOptions::BSDFType::PrincipledBSDF
                            : ks::CompoundMeshAsset::LoadMaterialOptions::BSDFType::PrincipledBRDF;
    asset->load_from_gltf(asset->path, options);

    // The same traversal the loader used to build `instances`, so the k-th
    // node with a mesh is instance k.
    ks::traverse_gltf_scene_graph(asset->path, [&](const tinygltf::Model &, const tinygltf::Node &node,
                                                   const ks::Transform &, const ks::Transform &) {
        if (node.mesh >= 0)
            asset->nodes.push_back({node.name, node.mesh, read_extras(node.extras)});
        return true;
    });
    ASSERT(asset->nodes.size() == asset->instances.size(), "[%s]: %zu mesh nodes but %zu instances",
           asset->path.string().c_str(), asset->nodes.size(), asset->instances.size());
    for (size_t k = 0; k < asset->nodes.size(); ++k) {
        ASSERT(asset->nodes[k].mesh == (int)asset->instances[k].first, "[%s]: node %zu instance order mismatch",
               asset->path.string().c_str(), k);
    }
    return asset;
}

} // namespace dmap
