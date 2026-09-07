#include "ks/config.h"
#include "ks/gpu/ksvk.h"
#include "ks/log_util.h"
#include "ks/material.h"
#include "ks/mesh_asset.h"
#include "ks/normal_map.h"
#include "ks/opacity_map.h"
#include "ks/parallel.h"
#include "ks/subsurface.h"
#include "ks/texture.h"
#include <filesystem>
namespace fs = std::filesystem;
#include <cxxopts.hpp>

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Don't reorder these headers...
// clang-format off
#include <windows.h>
#include <volk.h>
#include <vulkan/vulkan_win32.h>
// clang-format on
#elif defined(__linux__)

#endif

using namespace ks;

#define DECLARE_TASK(ENTRY_POINT) void ENTRY_POINT(const ks::ConfigArgs &args, const fs::path &task_dir, int task_id)

namespace ks
{
DECLARE_TASK(small_pt);
DECLARE_TASK(convert_hdri_to_equal_area);
DECLARE_TASK(tonemap);
} // namespace ks
DECLARE_TASK(test_weighted_area_sampling);
DECLARE_TASK(validate_pointwise);
DECLARE_TASK(validate_tessellation);
DECLARE_TASK(validate_pyramid);
DECLARE_TASK(test_pyramid_channels);
DECLARE_TASK(validate_line_sampling);
DECLARE_TASK(validate_bilinear_patch);
DECLARE_TASK(validate_descent);
DECLARE_TASK(render_displaced_emitter);

int main(int argc, char *argv[])
{
    cxxopts::Options options("dmap", "Tessellation-free Displacement Maps Fun");
    // clang-format off
    options.add_options()
        ("n,nthreads", "number of cpu threads", cxxopts::value<int>()->default_value("0"))
        ("d,device", "gpu device index", cxxopts::value<int>()->default_value("0"))
        ("v,validation", "vulkan validation", cxxopts::value<int>()->default_value("1"))
        ("w,window", "require window/swapchain", cxxopts::value<int>()->default_value("1"))
         ("c,config", "config file", cxxopts::value<std::string>()->default_value(std::string(DATA_DIR) + "configs/test_weighted_area_sampling.toml"))
        ("r,asset_root_dir", "asset root dir", cxxopts::value<std::string>()->default_value(DATA_DIR));
    // clang-format on
    auto args = options.parse(argc, argv);

    fs::path cfg_path(args["config"].as<std::string>());
    ConfigService cfg;
    std::string asset_root_dir = args["asset_root_dir"].as<std::string>();
    if (!asset_root_dir.empty()) {
        cfg.set_asset_root_dir(fs::path(asset_root_dir));
    }
    cfg.parse_file(cfg_path);

    create_default_logger(cfg.output_directory() / "log.txt");

    int nthreads = args["nthreads"].as<int>();
    // DEBUG
    // nthreads = 1;
    init_parallel(nthreads);

    //     std::array<const char *, 1> shader_search_paths = {SHADER_DIR};
    //     int vk_device = args["device"].as<int>();
    //     bool vk_validation = (args["validation"].as<int>() != 0);
    //     bool vk_swapchain = (args["window"].as<int>() != 0); // TODO

    //     vk::ContextArgs vkctx_args = get_default_context_args(vk_validation, vk_swapchain);
    // #if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    //     vkctx_args.device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    //     vkctx_args.device_extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
    // #elif defined(__linux__)
    //     vkctx_args.device_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    //     vkctx_args.device_extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    // #endif

    //     const char *slang_profile = "spirv_1_6 + spvGroupNonUniformBallot + spvGroupNonUniformArithmetic";
    //     init_gpu(shader_search_paths, slang_profile, vk_device, vkctx_args);

    // init_cuda();

    // These need to be ordered.
    // Shader fields are defined in-place most of the time...but need to retrieve the parser.
    cfg.register_asset("shader_field_1", create_shader_field_color<1>);
    cfg.register_asset("shader_field_2", create_shader_field_color<2>);
    cfg.register_asset("shader_field_3", create_shader_field_color<3>);
    cfg.register_asset("shader_field_4", create_shader_field_color<4>);
    cfg.register_asset("texture", create_texture);
    cfg.register_asset("normal_map", create_normal_map);
    cfg.register_asset("opacity_map", create_opacity_map);
    cfg.register_asset("bsdf", create_bsdf);
    cfg.register_asset("bssrdf", create_bssrdf);
    cfg.register_asset("material", create_material);
    cfg.register_asset("mesh_asset", create_mesh_asset);
    cfg.register_asset("compound_mesh_asset", create_compound_mesh_asset);
    // cfg.register_asset("camera_animation", create_camera_animation);
    // cfg.register_asset("gaussian_scene", create_gaussian_scene_asset);
    cfg.load_assets();

    // These does not need to be ordered.
    cfg.register_task("small_pt", small_pt);
    cfg.register_task("convert_hdri_to_equal_area", convert_hdri_to_equal_area);
    cfg.register_task("tonemap", tonemap);
    //
    cfg.register_task("test_weighted_area_sampling", test_weighted_area_sampling);
    cfg.register_task("validate_pointwise", validate_pointwise);
    cfg.register_task("validate_tessellation", validate_tessellation);
    cfg.register_task("validate_pyramid", validate_pyramid);
    cfg.register_task("test_pyramid_channels", test_pyramid_channels);
    cfg.register_task("validate_line_sampling", validate_line_sampling);
    cfg.register_task("validate_bilinear_patch", validate_bilinear_patch);
    cfg.register_task("validate_descent", validate_descent);
    cfg.register_task("render_displaced_emitter", render_displaced_emitter);
    cfg.run_all_tasks();

    return 0;
}