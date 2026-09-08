#pragma once
#include "displaced_surface.h"
#include "ks/config.h"
#include "taylor_pyramid.h"
#include "texture_grid.h"
#include <filesystem>
#include <memory>
#include <string>
namespace fs = std::filesystem;

// The [displacement.<name>] asset ("Plan — Path tracer with displaced
// surfaces", D6 and D8): one displacement map with its height convention,
//     h = strength * (t - midlevel),   t the texture value in [0, 1],
// and the tile data built from it once and shared by every triangle of
// every object that names the asset: the closed node grid and the Taylor
// pyramid. Emission statistics are not here; they belong to the object,
// because emission is a material property in the object's own texture
// coordinates (scene_spec.h).
//
// Tile orientation follows ks's texture convention: texel row 0 is the top
// of the image, so tile coordinate v = 0 lies at the image's top edge.
// Objects whose texture coordinates run upward (OBJ) flip v, as ks's
// TextureField does (scene_spec.h, flip_v).

namespace dmap
{

struct DisplacementAsset : ks::Configurable
{
    std::string name; // the asset key, or the map file for glTF-synthesized assets
    fs::path map;
    double strength = 1.0;
    double midlevel = 0.5;
    bool repeat = true; // wrap = "repeat" (periodic tile) or "clamp"
    int resolution = 0; // leaf texels per side, 2^L; 0 = the map's own size
    PyramidBuild build = PyramidBuild::Fold;

    TextureGrid texels; // resolution x resolution, image row order
    TextureGrid nodes;  // the closed (resolution + 1)^2 node grid (D6)
    HeightGrid field;   // view over nodes with the height convention
    std::unique_ptr<TaylorPyramid> pyramid;

    DisplacementAsset(std::string name, fs::path map, double strength, double midlevel, bool repeat, int resolution,
                      PyramidBuild build);
    DisplacementAsset(const DisplacementAsset &) = delete;
    DisplacementAsset &operator=(const DisplacementAsset &) = delete;

    std::string describe() const;
};

// Keys: map (path), strength (1), midlevel (0.5), wrap ("repeat" or
// "clamp"), resolution (0 = the map's size; must be a power of two),
// pyramid_build ("fold" or "direct").
std::unique_ptr<DisplacementAsset> create_displacement_asset(const ks::ConfigArgs &args);

} // namespace dmap
