#pragma once
#include "base_mesh.h"
#include "displaced_emitter_light.h"
#include "displaced_tessellation.h"
#include "displacement_asset.h"
#include "emission_tile.h"
#include "gltf_asset.h"
#include "ks/camera.h"
#include "ks/config.h"
#include "ks/light.h"
#include "ks/mesh_asset.h"
#include "traversal_options.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

// The scene specification ("Plan — Path tracer with displaced surfaces",
// T2 and D8): what every later task reads. Parsed from a [scene.<name>]
// asset, or from a task table that carries the same keys.
//
// Objects. ks's form, one asset per scene, is read as small_pt reads it:
//     object = "mesh_asset.x"     or   compound_object = "compound_mesh_asset.x"
//     material = [["material.a", ...]]        (per object, per shape; optional)
// Any number of objects, each an asset placed once:
//     [[scene.<name>.objects]]
//     asset = "mesh_asset.x"       # or a compound_mesh_asset (glTF)
//     name = "x"                   # optional, defaults to the asset key
//     to_world = { ... }           # optional, ks's transform table
//     material = ["material.a"]    # optional, per shape; for a glTF object one per node with a mesh, in node
//                                  # order, "" keeping the file's material
// A shape is one mesh of an asset: an OBJ shape (named by its `o` or `g`
// line, the object alone when the file has one unnamed shape) or a glTF
// node with a mesh (named by the node). Its full name is "object/shape".
//
// Displacement. A `displace` list attaches a displacement asset to a shape:
//     [[scene.<name>.displace]]
//     object = "torus"             # full name, shape name, or object name
//     displacement = "rock"        # the [displacement.rock] asset
//     uv_scale = 4.0               # tile coordinates = (u, v') * uv_scale + uv_offset
//     uv_scale_v = 1.0             # the v axis alone, for layouts whose axes differ in length; defaults to uv_scale
//     uv_offset = [0.0, 0.0]
//     flip_v = true                # v' = 1 - v; default true for OBJ, false for glTF, ks's convention
//     material = "material.m"      # optional override
//     emitter_sampler = "product_descent"   # when the material emits
//     emission_tiled = true        # the emission repeats with the displacement tile (the material's textures use
//                                  # the same uv_scale and uv_offset), so its statistics are one tile, not a box
//     emission_supersample = 4     # points per axis per leaf cell when sampling the emission field
//     traversal = { bound = "box" }         # optional override of the scene's table
// glTF nodes carry the same record in their extras (gltf_asset.h); a toml
// entry for the same shape wins field by field... no: the toml entry
// replaces the extras record whole (D8: the overlay wins on conflict).
//
// Traversal defaults live in the scene's `traversal` table. Camera (`camera`)
// and lights (`sky`, `light`) are parsed as small_pt parses them.
//
// The record of a displaced object holds the base mesh in double, one
// BaseTriangle per face in tile parameterization, a HeightGrid view of the
// asset's tile, and, when its material emits, the emission statistics over
// its texture domain at the displacement's leaf resolution (emission_tile.h).

namespace dmap
{

struct DisplaceEntry
{
    std::string object;
    std::string displacement;
    ks::vec2d uv_scale = ks::vec2d::Ones();
    ks::vec2d uv_offset = ks::vec2d::Zero();
    std::optional<bool> flip_v;
    std::string material;
    std::string emitter_sampler;
    bool emission_tiled = false;
    int emission_supersample = 4;
    TraversalOptions traversal;
    // glTF extras only: the asset's own values must agree (D9: no
    // per-object strength), or, when no toml asset of that name exists,
    // they define the asset synthesized from the map path.
    std::optional<double> strength, midlevel;
    std::string source; // "toml" or "gltf"
};

struct SceneShape
{
    std::string name;       // "object/shape"
    std::string short_name; // the shape or node name alone; may be empty
    int object = -1;
    const ks::MeshData *mesh = nullptr;
    const ks::Material *material = nullptr; // null when the scene assigns none
    std::vector<ks::Transform> instances;   // object space to world, one per placement
    int prototype = -1, mesh_index = -1;    // the ks subscene and geometry (T6)
    bool from_gltf = false;
};

struct SceneObject
{
    std::string name;
    std::string asset; // the asset table key
    const ks::MeshAsset *mesh_asset = nullptr;
    const GltfAsset *gltf = nullptr;
    ks::Transform to_world;
    std::vector<int> shapes;
};

struct DisplacedObject
{
    int shape = -1;
    const DisplacementAsset *asset = nullptr;
    std::string asset_name;
    ks::vec2d uv_scale = ks::vec2d::Ones();
    ks::vec2d uv_offset = ks::vec2d::Zero();
    bool flip_v = true;
    const ks::Material *material = nullptr;
    EmitterSamplerKind emitter_sampler = EmitterSamplerKind::ProductDescent;
    TraversalOptions traversal;
    std::string source;

    BaseMesh base;
    std::vector<BaseTriangle> triangles; // one per face, tile parameterization
    HeightGrid field;
    std::unique_ptr<EmissionTile> emission; // when the material emits: a box over the domain, or one tile when
                                            // the emission repeats with the displacement (emission_tiled)

    // Tile coordinates of the object's texture coordinates and back.
    ks::vec2d tile_coords(const ks::vec2d &uv) const;
    ks::vec2d object_uv(const ks::vec2d &t) const;
    bool emits() const { return emission != nullptr; }
};

struct SceneSpec : ks::Configurable
{
    std::vector<SceneObject> objects;
    std::vector<SceneShape> shapes;
    std::vector<DisplacedObject> displaced;
    std::vector<DisplaceEntry> entries_toml, entries_gltf;
    TraversalOptions traversal;
    std::unique_ptr<ks::Camera> camera;
    std::unique_ptr<ks::SkyLight> sky;
    std::vector<std::unique_ptr<ks::Light>> lights;
    std::vector<std::unique_ptr<DisplacementAsset>> owned_assets; // synthesized from glTF extras
    std::vector<std::string> errors;

    bool valid() const { return errors.empty(); }
    void require_valid() const;

    // The pre-tessellated substitute of a displaced object at subdivision n
    // per base triangle (plan D4 oracles): S(u, v) on the barycentric grid,
    // texture coordinates in the object's own space.
    ks::MeshData tessellated(const DisplacedObject &d, int n) const;
    // The texel-aligned substitute (displaced_tessellation.h) with m
    // sub-cells per texel, and the record of its faces.
    ks::MeshData texel_tessellated(const DisplacedObject &d, int m, std::vector<TexelFace> &faces) const;
};

std::unique_ptr<SceneSpec> load_scene_spec(const ks::ConfigArgs &args);
// The [scene.<name>] parser: load_scene_spec with the errors printed.
std::unique_ptr<SceneSpec> create_scene_spec(const ks::ConfigArgs &args);

EmitterSamplerKind emitter_sampler_from_string(const std::string &name);
const char *emitter_sampler_name(EmitterSamplerKind kind);

// A base mesh from a ks MeshData: positions, vertex normals (area-weighted
// when the data has none), texture coordinates, triangles.
BaseMesh base_mesh_from_mesh_data(const ks::MeshData &data);

} // namespace dmap
