#include "path_trace.h"
#include "displaced_area_light.h"
#include "ks/assertion.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include <chrono>

using namespace ks;

namespace dmap
{

std::unique_ptr<PathTraceSetup> path_trace_setup(const ConfigArgs &args, const EmbreeDevice &device,
                                                 int substitute_subdivision)
{
    auto setup = std::make_unique<PathTraceSetup>();
    if (args.contains("scene")) {
        setup->spec = args.asset_table().get<SceneSpec>(args.load_string("scene"));
        ASSERT(setup->spec, "no scene named [%s]", args.load_string("scene").c_str());
    } else {
        setup->owned_spec = load_scene_spec(args);
        setup->spec = setup->owned_spec.get();
    }
    const SceneSpec &spec = *setup->spec;
    spec.require_valid();
    setup->built = create_displaced_scene(spec, device, substitute_subdivision);
    Scene &scene = setup->built->scene;

    // An inline scene already parsed the task's camera, sky and lights;
    // for a scene asset, the task table's replace the scene's.
    bool inline_scene = setup->owned_spec != nullptr;
    LightPointers light_ptrs;
    if (!inline_scene && args.contains("sky")) {
        setup->sky = create_sky_light(args["sky"]);
        light_ptrs.lights.push_back(setup->sky.get());
    } else if (spec.sky) {
        light_ptrs.lights.push_back(spec.sky.get());
    }
    if (!inline_scene && args.contains("light")) {
        ConfigArgs list = args["light"];
        for (int i = 0; i < (int)list.array_size(); ++i) {
            setup->lights.push_back(create_light(list[i]));
            light_ptrs.lights.push_back(setup->lights.back().get());
        }
    } else {
        for (const auto &light : spec.lights)
            light_ptrs.lights.push_back(light.get());
    }
    for (const auto &al : scene.area_lights)
        light_ptrs.area_lights.push_back(al.get());
    setup->light_sampler = std::make_unique<PowerLightSampler>(scene.bound());
    setup->light_sampler->build(light_ptrs);

    if (!inline_scene && args.contains("camera"))
        setup->camera = create_camera(args["camera"]);
    else if (spec.camera)
        setup->camera = std::make_unique<Camera>(*spec.camera);
    ASSERT(setup->camera, "path_trace: no camera in the task table or the scene");

    if (args.contains("pixel_filter"))
        setup->pixel_filter = create_pixel_filter(args["pixel_filter"]);
    else
        setup->pixel_filter = std::make_unique<BoxPixelFilter>();
    if (args.contains("tone_mapper"))
        setup->tone_mapper = create_tone_mapper(args["tone_mapper"]);

    SmallPTInput &input = setup->input;
    input.scene = &scene;
    input.light_sampler = setup->light_sampler.get();
    input.camera = setup->camera.get();
    input.pixel_filter = setup->pixel_filter.get();
    if (args.contains("backdrop")) {
        input.backdrop = args.load_vec3("backdrop").array();
        input.include_background = false;
    } else {
        input.include_background = true;
    }
    input.bounces = args.load_integer("bounces");
    input.clamp_direct = args.load_float("clamp_direct", 0.0f);
    input.clamp_indirect = args.load_float("clamp_indirect", 10.0f);
    input.render_width = args.load_integer("render_width");
    input.render_height = args.load_integer("render_height");
    input.crop_start_x = args.load_integer("crop_start_x", 0);
    input.crop_start_y = args.load_integer("crop_start_y", 0);
    input.crop_width = args.load_integer("crop_width", 0);
    input.crop_height = args.load_integer("crop_height", 0);
    input.spp = args.load_integer("spp");
    input.scale_ray_diff = args.load_bool("scale_ray_diff", true);
    input.rng_seed = args.load_integer("rng_seed", 0);
    input.spp_prog_interval = args.load_integer("spp_prog_interval", 32);

    auto &log = get_default_logger();
    for (const AreaLightShared *al : light_ptrs.area_lights) {
        if (const auto *dl = dynamic_cast<const DisplacedAreaLightShared *>(al)) {
            const SceneShape &shape = spec.shapes[dl->object->shape];
            log.info("path_trace: displaced emitter [{}] instance {}: {} of {} base triangles emit, sampler {}",
                     shape.name, dl->inst_id, dl->light_count(), dl->object->triangles.size(),
                     emitter_sampler_name(dl->geometry->emitter_sampler));
        }
    }
    log.info("path_trace: {} lights, {} area light groups{}", setup->light_sampler->light_count(),
             light_ptrs.area_lights.size(),
             substitute_subdivision > 0 ? " (displaced shapes as pre-tessellated substitutes)" : "");
    return setup;
}

double path_trace_render(const PathTraceSetup &setup, const fs::path &task_dir, const std::string &prefix)
{
    auto &log = get_default_logger();
    SmallPTInput input = setup.input;
    auto render_start = std::chrono::steady_clock::now();
    auto interval_start = render_start;
    input.prog_interval_callback = [&](const RenderTarget2 &rt, int spp_finished) {
        auto now = std::chrono::steady_clock::now();
        log.info("{}: [{}/{}] spp interval took {:.2f} sec", prefix, spp_finished, input.spp,
                 std::chrono::duration<float>(now - interval_start).count());
        log.flush();
        fs::path save_prefix = task_dir / string_format("%s_spp%d", prefix.c_str(), spp_finished);
        rt.composite_and_save_to_exr(save_prefix);
        if (setup.tone_mapper)
            rt.composite_and_save_to_png(save_prefix, {}, setup.tone_mapper.get());
        interval_start = now;
    };
    SmallPT tracer;
    tracer.run(input);
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - render_start).count();
    log.info("{}: rendering time {:.2f} sec", prefix, seconds);
    return seconds;
}

} // namespace dmap

void path_trace(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    EmbreeDevice device;
    int substitute_subdivision = args.load_integer("substitute_subdivision", 0);
    std::unique_ptr<dmap::PathTraceSetup> setup = dmap::path_trace_setup(args, device, substitute_subdivision);
    dmap::path_trace_render(*setup, task_dir, "path_trace");
}
