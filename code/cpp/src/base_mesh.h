#pragma once
#include "displaced_surface.h"
#include <filesystem>
namespace fs = std::filesystem;

// Base mesh with unit vertex normals and optional texture coordinates,
// mirroring the numpy reference (code/python/poc/dmapref/mesh.py): OBJ
// loading with fan triangulation, and area-weighted vertex normals when the
// file has none.

namespace dmap
{

struct BaseMesh
{
    std::vector<ks::vec3d> positions;
    std::vector<ks::vec3d> normals; // unit
    std::vector<ks::vec2d> texcoords;
    std::vector<Eigen::Vector3i> faces;
    std::vector<Eigen::Vector3i> face_normal_indices;
    std::vector<Eigen::Vector3i> face_texcoord_indices; // -1 where the file has none

    int n_triangles() const { return (int)faces.size(); }
    bool has_texcoords() const { return !texcoords.empty(); }
    // Identity chart (the experiments' convention: the whole tile per triangle).
    BaseTriangle triangle(int t) const;
    // The chart from the file's texture coordinates.
    BaseTriangle triangle_chart(int t) const;
};

BaseMesh load_base_obj(const fs::path &path);

} // namespace dmap
