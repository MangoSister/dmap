#pragma once
#include "ks/material.h"
#include "ks/shader_field.h"
#include <memory>

// ks materials from toml with an emission field. ks's own parser reads
// bsdf, subsurface and normal_map; emission reaches a Material only through
// the glTF loader. Registered in main.cpp under ks's prefix, [material.x],
// with one more key, an inline shader_field_3 table as ks's BSDF parsers
// take their fields:
//     emission = { type = "constant", value = [8, 8, 8] }
// Scene::build_area_lights makes every mesh with such a material a light,
// in small_pt as in dmap's tasks.

namespace dmap
{

struct EmissiveMaterial : ks::Material
{
    ~EmissiveMaterial();
    std::unique_ptr<ks::ShaderField3> owned_emission; // the inline form
};

std::unique_ptr<ks::Material> create_material(const ks::ConfigArgs &args);

} // namespace dmap
