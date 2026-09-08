#pragma once
#include "displaced_geometry.h"
#include "ks/scene.h"
#include "scene_spec.h"
#include <map>
#include <memory>
#include <vector>

// The ks scene of a scene specification ("Plan — Path tracer with
// displaced surfaces", T6): built as ks builds it, one subscene per OBJ
// object and per glTF prototype with one instance per placement, except
// that a displaced shape gets a DisplacedGeometry in place of its
// MeshGeometry and its own material; a glTF node with a displaced mesh
// gets a subscene of its own, since the displacement belongs to the node,
// not to the prototype. Every geometry keeps its material, and ks's area
// lights are built as usual (displaced lights come with T8).
//
// With substitute_subdivision m > 0 the displaced shapes are replaced by
// MeshGeometry over their texel-aligned pre-tessellated substitutes (D4,
// m sub-cells per texel), the reference scene of the validation tasks.

namespace dmap
{

struct ShapePlacement
{
    int subscene = -1, geometry = -1;
    std::vector<uint32_t> instances;
};

struct DisplacedScene
{
    ks::Scene scene;
    std::vector<ShapePlacement> shapes;                     // per SceneShape of the specification
    std::vector<DisplacedGeometry *> displaced;             // per DisplacedObject; null when substituted
    std::vector<std::unique_ptr<ks::MeshData>> substitutes; // per DisplacedObject when substituted
    std::vector<std::vector<TexelFace>> substitute_faces;
    int substitute_subdivision = 0;

    // The displaced object behind a (subscene, geometry), or -1.
    int displaced_of(uint32_t subscene, uint32_t geometry) const;
    // The shape behind an (instance, geometry), or -1.
    int shape_of(uint32_t instance, uint32_t geometry) const;

    std::map<std::pair<uint32_t, uint32_t>, int> displaced_index, shape_index;
};

std::unique_ptr<DisplacedScene> create_displaced_scene(const SceneSpec &spec, const ks::EmbreeDevice &device,
                                                       int substitute_subdivision = 0);

} // namespace dmap
