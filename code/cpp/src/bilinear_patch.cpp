#include "bilinear_patch.h"
#include "ks/assertion.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

double sample_linear_1d(double u, double a, double b)
{
    // Inverse CDF of the density proportional to lerp(x, a, b) on [0, 1].
    if (u == 0.0 && a == 0.0)
        return 0.0;
    double x = u * (a + b) / (a + std::sqrt(u * (b * b - a * a) + a * a));
    return std::min(x, 1.0 - 1e-12);
}

vec2d sample_bilinear(const vec2d &u, const double w[4])
{
    vec2d p;
    // v from the marginal, then u from the conditional at v.
    p[1] = sample_linear_1d(u[1], w[0] + w[1], w[2] + w[3]);
    p[0] = sample_linear_1d(u[0], (1.0 - p[1]) * w[0] + p[1] * w[2], (1.0 - p[1]) * w[1] + p[1] * w[3]);
    return p;
}

double bilinear_pdf(const vec2d &p, const double w[4])
{
    double sum = w[0] + w[1] + w[2] + w[3];
    if (sum == 0.0)
        return 1.0;
    double bilerp = (1.0 - p[0]) * (1.0 - p[1]) * w[0] + p[0] * (1.0 - p[1]) * w[1] + (1.0 - p[0]) * p[1] * w[2] +
                    p[0] * p[1] * w[3];
    return 4.0 * bilerp / sum;
}

void BilinearPatchMesh::set_emission(TextureGrid em)
{
    emission = std::move(em);
    std::vector<float> f(emission.values.begin(), emission.values.end());
    image_distrib = std::make_unique<ks::DistribTable2D>(f.data(), emission.W, emission.H);
}

double BilinearPatchMesh::emission_at(double u, double v) const
{
    ASSERT(!emission.values.empty());
    int i = std::min((int)(u * emission.W), emission.W - 1);
    int j = std::min((int)(v * emission.H), emission.H - 1);
    return emission.values[(size_t)j * emission.W + i];
}

void BilinearPatch::corners(vec3d &p00, vec3d &p10, vec3d &p01, vec3d &p11) const
{
    const uint32_t *v = &mesh->indices[4 * patch_index];
    p00 = mesh->p[v[0]];
    p10 = mesh->p[v[1]];
    p01 = mesh->p[v[2]];
    p11 = mesh->p[v[3]];
}

vec3d BilinearPatch::position(double u, double v) const
{
    vec3d p00, p10, p01, p11;
    corners(p00, p10, p01, p11);
    vec3d pu0 = (1.0 - v) * p00 + v * p01;
    vec3d pu1 = (1.0 - v) * p10 + v * p11;
    return (1.0 - u) * pu0 + u * pu1;
}

vec3d BilinearPatch::dpdu(double u, double v) const
{
    vec3d p00, p10, p01, p11;
    corners(p00, p10, p01, p11);
    return ((1.0 - v) * p10 + v * p11) - ((1.0 - v) * p00 + v * p01);
}

vec3d BilinearPatch::dpdv(double u, double v) const
{
    vec3d p00, p10, p01, p11;
    corners(p00, p10, p01, p11);
    return ((1.0 - u) * p01 + u * p11) - ((1.0 - u) * p00 + u * p10);
}

PatchSample BilinearPatch::sample(const vec2d &u2) const
{
    vec2d uv;
    double pdf_uv = 1.0;
    if (mesh->image_distrib) {
        float pdf_f;
        ks::vec2 s = mesh->image_distrib->sample_linear(ks::vec2((float)u2[0], (float)u2[1]), pdf_f);
        uv = vec2d(s[0], s[1]);
        pdf_uv = pdf_f;
    } else {
        // pbrt's approximate uniform area sampling: corner differential
        // areas as bilinear weights.
        vec3d p00, p10, p01, p11;
        corners(p00, p10, p01, p11);
        double w[4] = {(p10 - p00).cross(p01 - p00).norm(), (p10 - p00).cross(p11 - p10).norm(),
                       (p01 - p00).cross(p11 - p01).norm(), (p11 - p10).cross(p11 - p01).norm()};
        uv = sample_bilinear(u2, w);
        pdf_uv = bilinear_pdf(uv, w);
    }

    PatchSample s;
    s.u = uv[0];
    s.v = uv[1];
    s.p = position(uv[0], uv[1]);
    vec3d cross = dpdu(uv[0], uv[1]).cross(dpdv(uv[0], uv[1]));
    double J = cross.norm();
    s.n = cross / J;
    s.pdf_area = pdf_uv / J;
    return s;
}

double BilinearPatch::pdf_area(double u, double v) const
{
    double pdf_uv = 1.0;
    if (mesh->image_distrib) {
        pdf_uv = mesh->image_distrib->pdf(ks::vec2((float)u, (float)v));
    } else {
        vec3d p00, p10, p01, p11;
        corners(p00, p10, p01, p11);
        double w[4] = {(p10 - p00).cross(p01 - p00).norm(), (p10 - p00).cross(p11 - p10).norm(),
                       (p01 - p00).cross(p11 - p01).norm(), (p11 - p10).cross(p11 - p01).norm()};
        pdf_uv = bilinear_pdf(vec2d(u, v), w);
    }
    return pdf_uv / dpdu(u, v).cross(dpdv(u, v)).norm();
}

} // namespace dmap
