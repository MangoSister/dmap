#pragma once
#include "displaced_scene.h"
#include "ks/camera.h"
#include "ks/config.h"
#include "ks/light.h"
#include "ks/render_target.h"
#include "ks/small_pt.h"
#include "ks/tonemap.h"
#include "scene_spec.h"
#include <memory>
#include <string>

// Task path_trace ("Plan — Path tracer with displaced surfaces", T8): ks's
// small_pt driver with the scene built through the scene specification
// (T2) and the displaced scene (T6), rendered by ks's SmallPT, which is
// shared, not copied (D7). Displaced emitters are area lights through
// DisplacedGeometry::create_area_light; every other light is ks's.
//
// Config: the small_pt keys (bounces, render_width, render_height, spp,
// rng_seed, spp_prog_interval, clamp_direct, clamp_indirect, crop_*,
// scale_ray_diff, pixel_filter, tone_mapper, backdrop), and the scene
// either as `scene = "scene.x"` or as the scene keys inline (object,
// material, objects, displace, traversal, camera, sky, light). A camera,
// sky or light list in the task table replaces the scene's.
// substitute_subdivision > 0 renders the displaced shapes as their
// texel-aligned pre-tessellated substitutes with ks's mesh lights (D4),
// the reference of the verification.

namespace dmap
{

struct PathTraceSetup
{
    std::unique_ptr<SceneSpec> owned_spec; // when the scene keys are inline
    const SceneSpec *spec = nullptr;
    std::unique_ptr<DisplacedScene> built;
    std::unique_ptr<ks::SkyLight> sky;              // the task's, when it has one
    std::vector<std::unique_ptr<ks::Light>> lights; // the task's, when it has some
    std::unique_ptr<ks::LightSampler> light_sampler;
    std::unique_ptr<ks::Camera> camera;
    std::unique_ptr<ks::PixelFilter> pixel_filter;
    std::unique_ptr<ks::ToneMapper> tone_mapper;
    ks::SmallPTInput input;
};

std::unique_ptr<PathTraceSetup> path_trace_setup(const ks::ConfigArgs &args, const ks::EmbreeDevice &device,
                                                 int substitute_subdivision);

// Renders input.spp samples per pixel, writing <prefix>_spp<k>.exr (and
// .png with a tone mapper) at every progress interval as small_pt does,
// and returns the render time in seconds.
double path_trace_render(const PathTraceSetup &setup, const fs::path &task_dir, const std::string &prefix);

} // namespace dmap
