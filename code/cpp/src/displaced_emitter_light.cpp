#include "displaced_emitter_light.h"
#include "ks/assertion.h"
#include <cmath>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

namespace
{

// Below this |n_y . wi| the solid-angle pdf blows up and the contribution
// vanishes; treat the draw as zero-contribution instead of dividing.
constexpr double min_cos_y = 1e-7;

} // namespace

DisplacedEmitterLight::DisplacedEmitterLight(const BaseTriangle &tri, const HeightGrid &field,
                                             const TaylorPyramid &pyramid, const EmissionTile &emission,
                                             EmitterSamplerKind kind, double beta)
    : tri(&tri), field(field), emission(&emission), kind(kind)
{
    switch (kind) {
    case EmitterSamplerKind::EmissionTable:
        table = std::make_unique<TexelTableSampler>(tri, field, emission, /*with_metric*/ false);
        break;
    case EmitterSamplerKind::ProductTable:
        table = std::make_unique<TexelTableSampler>(tri, field, emission, /*with_metric*/ true);
        break;
    case EmitterSamplerKind::AreaDescent:
        descent = std::make_unique<DescentSampler>(tri, field, pyramid, emission, DescentWeight::AreaOnly, beta);
        break;
    case EmitterSamplerKind::ProductDescent:
        descent = std::make_unique<DescentSampler>(tri, field, pyramid, emission, DescentWeight::Product, beta);
        break;
    case EmitterSamplerKind::ReceiverDescent:
        descent = std::make_unique<DescentSampler>(tri, field, pyramid, emission, DescentWeight::ProductGeometry, beta);
        descent->emitter_cosine = false; // S7: the midpoint cosine estimate is harmful
        break;
    }
}

double DisplacedEmitterLight::Le_at(double u, double v) const
{
    int n = emission->n_leaf;
    return emission->texel((int64_t)std::floor(u * n), (int64_t)std::floor(v * n));
}

double DisplacedEmitterLight::mass() const { return descent ? descent->footprint.total_mass : table->total_mass; }

size_t DisplacedEmitterLight::memory_bytes() const { return descent ? descent->memory_bytes() : table->memory_bytes(); }

EmitterLightSample DisplacedEmitterLight::sample(const vec3d &p_shade, const vec3d &n_shade, ks::RNG &rng,
                                                 const vec2d *u_leaf) const
{
    EmitterLightSample out;
    double pdf_area_s;
    if (descent) {
        Receiver recv{p_shade, n_shade};
        DescentSample s = descent->sample(rng, kind == EmitterSamplerKind::ReceiverDescent ? &recv : nullptr, u_leaf);
        out.u = s.u;
        out.v = s.v;
        out.y = s.p;
        out.n_y = s.n;
        pdf_area_s = s.pdf_area;
    } else {
        TableSample s = table->sample(rng, u_leaf);
        if (!s.in_domain)
            return out; // zero-contribution draw; still counts as a sample
        out.u = s.u;
        out.v = s.v;
        out.y = s.p;
        out.n_y = s.n;
        pdf_area_s = s.pdf_area;
    }

    vec3d d = out.y - p_shade;
    out.dist = d.norm();
    out.wi = d / out.dist;
    double cos_y = std::abs(out.n_y.dot(out.wi));
    if (cos_y < min_cos_y)
        return out;
    out.pdf_area = pdf_area_s;
    out.pdf_omega = pdf_area_s * out.dist * out.dist / cos_y;
    out.Le = Le_at(out.u, out.v);
    out.ok = true;
    return out;
}

double DisplacedEmitterLight::pdf_area(const vec3d &p_shade, const vec3d &n_shade, double u, double v) const
{
    if (descent) {
        Receiver recv{p_shade, n_shade};
        return descent->pdf_area(u, v, kind == EmitterSamplerKind::ReceiverDescent ? &recv : nullptr);
    }
    return table->pdf_area(u, v);
}

double DisplacedEmitterLight::pdf_omega_from_uv(const vec3d &p_shade, const vec3d &n_shade, double u, double v) const
{
    double pa = pdf_area(p_shade, n_shade, u, v);
    if (pa <= 0.0)
        return 0.0;
    PointwiseFields f = pointwise_fields(*tri, field, u, v);
    vec3d y = tri->P(u, v) + f.h * normal_frame_at(*tri, u, v).N;
    vec3d d = y - p_shade;
    double dist2 = d.squaredNorm();
    double cos_y = std::abs(f.n.normalized().dot(d / std::sqrt(dist2)));
    if (cos_y < min_cos_y)
        return 0.0;
    return pa * dist2 / cos_y;
}

} // namespace dmap
