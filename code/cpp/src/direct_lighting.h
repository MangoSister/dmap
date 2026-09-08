#pragma once
#include "image_compare.h"
#include "ks/sobol.h"
#include "line_sampling.h"
#include "path_trace.h"
#include <string>
#include <vector>

// Direct lighting at the first hit of a camera ray, by one sampling
// strategy, through the AreaLight interface the tracer uses (sample, eval,
// pdf_hit), with the light selection probability on both sides of the MIS
// weight. Delta lights (the directional sun) are sampled under every
// strategy, since no BSDF sample can hit them. Shared by T8's MIS check
// (validate_path_tracer.cpp) and T9's ladder (test_emitter_ladder.cpp).
//
// The uniform strategy is master plan §5A baseline 4, Ling et al. 2025's
// line casting, adapted to the traverser: one line per sample through the
// world box of every displaced emitter, every crossing found by the
// scene's own intersection with the near distance advanced past each hit
// (so the walk, not a mesh, reports the smooth surface point and normal),
// and the integrand summed over the hits with the 2 x offset-area constant
// of line_sampling.h. Next-event only: a line yields a hit set, not a point
// with a density, so it has no pdf for MIS.

namespace dmap
{

enum class Strategy
{
    NEE,
    BSDF,
    MIS,
    Uniform
};

const char *strategy_name(Strategy s);
Strategy strategy_from_string(const std::string &name);

struct LineEstimator
{
    LineSampler lines;
    int max_hits = 64; // crossings followed per line, emitter or not

    explicit LineEstimator(const ks::AABB3 &box) : lines(box) {}
};

// The world box of each displaced emitter instance of the scene, and
// their union.
std::vector<ks::AABB3> displaced_emitter_bounds(const PathTraceSetup &setup);
ks::AABB3 displaced_emitter_bound(const PathTraceSetup &setup);

// One line through the box: the emitter hits along it, in order. Returns
// false when the line misses the box (it still counts as a line).
bool line_emitter_hits(const PathTraceSetup &setup, const LineEstimator &estimator, ks::RNG &rng,
                       std::vector<ks::SceneHit> &hits);

ks::color3 direct_lighting(const PathTraceSetup &setup, const ks::Ray &camera_ray, Strategy strategy,
                           ks::PTRenderSampler &sampler, const LineEstimator *lines = nullptr);

// One direct-lighting render with SmallPT's sampling setup, saved as
// <prefix>.exr and .png. Returns the image; seconds receives the render
// time when given.
RgbImage render_direct(const PathTraceSetup &setup, Strategy strategy, int spp, int seed, const fs::path &task_dir,
                       const std::string &prefix, const LineEstimator *lines = nullptr, double *seconds = nullptr);

} // namespace dmap
