#pragma once
#include "ks/distrib.h"
#include "ks/maths.h"
#include "texture_grid.h"
#include <memory>
#include <vector>

// Port of PBRT-v4's textured bilinear-patch area sampling
// (src/pbrt/shapes.cpp, BilinearPatch::Sample/PDF): draw (u, v) from a 2D
// distribution built from the emission image, map through the patch, and
// divide the uv pdf by the pointwise Jacobian |dp/du x dp/dv| for an exact
// pdf per unit surface area. The placement distribution only steers
// variance; the Jacobian division keeps the estimator unbiased.
//
// Not ported: the spherical-rectangle solid-angle path — pbrt itself
// bypasses it whenever an image distribution is present. One deliberate
// deviation: emission is defined piecewise-constant per texel (matching the
// table exactly), where pbrt builds the table from a filtered image and
// looks emission up bilinearly, accepting the placement mismatch.

namespace dmap
{

// Corner-weight helpers (pbrt's SampleLinear/SampleBilinear/BilinearPDF):
// sample (u, v) with density proportional to the bilinear interpolation of
// w = {w00, w10, w01, w11}. Used for pbrt's approximate uniform-area
// sampling of non-rectangular patches, and later for leaf-level warps.
double sample_linear_1d(double u, double a, double b);
ks::vec2d sample_bilinear(const ks::vec2d &u, const double w[4]);
double bilinear_pdf(const ks::vec2d &p, const double w[4]);

// Shared storage: vertices, 4 indices per patch (p00, p10, p01, p11), and
// one emission image with its sampling distribution (identity chart per
// patch — the same restriction pbrt has when no uv coordinates are given).
struct BilinearPatchMesh
{
    std::vector<ks::vec3d> p;
    std::vector<uint32_t> indices; // 4 per patch
    TextureGrid emission;          // optional; empty => no image distribution
    std::unique_ptr<ks::DistribTable2D> image_distrib;

    int n_patches() const { return (int)indices.size() / 4; }
    void set_emission(TextureGrid em);
    double emission_at(double u, double v) const; // piecewise constant per texel
};

struct PatchSample
{
    ks::vec3d p;
    ks::vec3d n; // unit, from dp/du x dp/dv
    double u, v;
    double pdf_area; // pdf per unit surface area
};

struct BilinearPatch
{
    const BilinearPatchMesh *mesh = nullptr;
    int patch_index = 0;

    BilinearPatch() = default;
    BilinearPatch(const BilinearPatchMesh &mesh, int patch_index) : mesh(&mesh), patch_index(patch_index) {}

    void corners(ks::vec3d &p00, ks::vec3d &p10, ks::vec3d &p01, ks::vec3d &p11) const;
    ks::vec3d position(double u, double v) const;
    ks::vec3d dpdu(double u, double v) const;
    ks::vec3d dpdv(double u, double v) const;

    // Area sampling (pbrt BilinearPatch::Sample(Point2f)): uv from the
    // image distribution when present, else from the corner-Jacobian
    // bilinear approximation (uniform for a rectangle in the limit).
    PatchSample sample(const ks::vec2d &u2) const;
    // The same pdf for a given (u, v) (pbrt BilinearPatch::PDF).
    double pdf_area(double u, double v) const;
};

} // namespace dmap
