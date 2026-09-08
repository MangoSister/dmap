#include "displacement_asset.h"
#include "ks/assertion.h"
#include "ks/file_util.h"
#include "ks/log_util.h"

namespace dmap
{

namespace
{

bool is_power_of_two(int n) { return n >= 1 && (n & (n - 1)) == 0; }

} // namespace

DisplacementAsset::DisplacementAsset(std::string name, fs::path map, double strength, double midlevel, bool repeat,
                                     int resolution, PyramidBuild build)
    : name(std::move(name)), map(std::move(map)), strength(strength), midlevel(midlevel), repeat(repeat),
      resolution(resolution), build(build)
{
    TextureGrid image = load_height_texture(this->map);
    ASSERT(image.W == image.H, "[%s]: a displacement map must be square, got %d x %d", this->map.string().c_str(),
           image.W, image.H);
    if (this->resolution <= 0)
        this->resolution = image.W;
    ASSERT(is_power_of_two(this->resolution),
           "[%s]: the tile needs 2^L texels per side, got %d (set `resolution` to a power of two)",
           this->map.string().c_str(), this->resolution);
    ASSERT(this->resolution <= image.W, "[%s]: resolution %d exceeds the map's %d texels", this->map.string().c_str(),
           this->resolution, image.W);
    texels = this->resolution == image.W ? std::move(image) : downsample_box(image, this->resolution);
    nodes = close_tile(texels, repeat);
    field = HeightGrid{nodes.W, nodes.H, strength, nodes.values.data(), -strength * midlevel, repeat};
    pyramid = std::make_unique<TaylorPyramid>(field, build);
}

std::string DisplacementAsset::describe() const
{
    return ks::string_format("map=%s resolution=%d strength=%g midlevel=%g wrap=%s build=%s",
                             map.filename().string().c_str(), resolution, strength, midlevel,
                             repeat ? "repeat" : "clamp", pyramid_build_name(build));
}

std::unique_ptr<DisplacementAsset> create_displacement_asset(const ks::ConfigArgs &args)
{
    fs::path map = args.load_path("map");
    double strength = args.load_float("strength", 1.0f);
    double midlevel = args.load_float("midlevel", 0.5f);
    std::string wrap = args.load_string("wrap", "repeat");
    ASSERT(wrap == "repeat" || wrap == "clamp", "[%s]: wrap must be repeat or clamp, got [%s]", map.string().c_str(),
           wrap.c_str());
    int resolution = args.load_integer("resolution", 0);
    PyramidBuild build = pyramid_build_from_string(args.load_string("pyramid_build", "fold"));
    return std::make_unique<DisplacementAsset>(map.stem().string(), map, strength, midlevel, wrap == "repeat",
                                               resolution, build);
}

} // namespace dmap
