#pragma once
#include "displaced_surface.h"
#include <filesystem>
namespace fs = std::filesystem;

// Base mesh with unit vertex normals, mirroring the numpy reference
// (code/python/poc/dmapref/mesh.py): OBJ loading with fan triangulation,
// and area-weighted vertex normals when the file has none.

namespace dmap
{

struct BaseMesh
{
    std::vector<ks::vec3d> positions;
    std::vector<ks::vec3d> normals; // unit
    std::vector<Eigen::Vector3i> faces;
    std::vector<Eigen::Vector3i> face_normal_indices;

    int n_triangles() const { return (int)faces.size(); }
    BaseTriangle triangle(int t) const;
};

BaseMesh load_base_obj(const fs::path &path);

} // namespace dmap
