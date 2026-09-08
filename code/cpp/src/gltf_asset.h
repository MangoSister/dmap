#pragma once
#include "ks/mesh_asset.h"
#include <optional>
#include <string>
#include <vector>

// A glTF asset with what ks's loader drops and the scene specification
// needs ("Plan — Path tracer with displaced surfaces", D8): the file path,
// the node names, and the node extras that attach displacement. Registered
// under ks's own prefix, [compound_mesh_asset.<name>], with the same keys.
//
// Extras written by the Blender workflow (T10) as custom properties on the
// object:
//     dmap_displacement  string  the displacement asset, or a map path
//                                relative to the glTF file
//     dmap_strength      number
//     dmap_midlevel      number
//     dmap_uv_scale      number
//     dmap_uv_scale_v    number  (the v axis alone; defaults to dmap_uv_scale)
//     dmap_uv_offset     [u, v]
// scene_spec.cpp turns them into the same record a toml `displace` entry
// gives.

namespace dmap
{

struct GltfDisplaceExtras
{
    std::optional<std::string> displacement;
    std::optional<double> strength, midlevel, uv_scale, uv_scale_v;
    std::optional<ks::vec2d> uv_offset;
    bool empty() const { return !displacement && !strength && !midlevel && !uv_scale && !uv_scale_v && !uv_offset; }
};

struct GltfAsset : ks::CompoundMeshAsset
{
    fs::path path;
    // Every node that carries a mesh, in the loader's instance order, so
    // nodes[k] is instances[k] of the base class.
    struct Node
    {
        std::string name;
        int mesh = -1; // the prototype index
        GltfDisplaceExtras extras;
    };
    std::vector<Node> nodes;
};

std::unique_ptr<GltfAsset> create_gltf_asset(const ks::ConfigArgs &args);

} // namespace dmap
