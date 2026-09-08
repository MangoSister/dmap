#include "emissive_material.h"
#include "ks/assertion.h"
#include "ks/subsurface.h"

namespace dmap
{

EmissiveMaterial::~EmissiveMaterial() = default;

std::unique_ptr<ks::Material> create_material(const ks::ConfigArgs &args)
{
    std::unique_ptr<ks::Material> base = ks::create_material(args);
    if (!args.contains("emission"))
        return base;

    std::unique_ptr<EmissiveMaterial> material = std::make_unique<EmissiveMaterial>();
    material->bsdf = base->bsdf;
    material->subsurface = base->subsurface;
    material->normal_map = base->normal_map;
    material->opacity_map = base->opacity_map;
    material->lambert_exit = std::move(base->lambert_exit);

    material->owned_emission = args.asset_table().create_in_place<ks::ShaderField3>("shader_field_3", args["emission"]);
    ASSERT(material->owned_emission, "material: cannot build the emission field (an inline shader_field_3 table)");
    material->emission = material->owned_emission.get();
    return material;
}

} // namespace dmap
