#include "base_mesh.h"
#include "ks/assertion.h"
#include <fstream>
#include <sstream>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

BaseTriangle BaseMesh::triangle(int t) const
{
    const Eigen::Vector3i &vi = faces[t];
    const Eigen::Vector3i &ni = face_normal_indices[t];
    return BaseTriangle(positions[vi[0]], positions[vi[1]], positions[vi[2]], normals[ni[0]], normals[ni[1]],
                        normals[ni[2]]);
}

BaseTriangle BaseMesh::triangle_chart(int t) const
{
    const Eigen::Vector3i &vi = faces[t];
    const Eigen::Vector3i &ni = face_normal_indices[t];
    const Eigen::Vector3i &ti = face_texcoord_indices[t];
    ASSERT(has_texcoords() && ti.minCoeff() >= 0, "triangle %d has no texture coordinates", t);
    return BaseTriangle(positions[vi[0]], positions[vi[1]], positions[vi[2]], normals[ni[0]], normals[ni[1]],
                        normals[ni[2]], texcoords[ti[0]], texcoords[ti[1]], texcoords[ti[2]]);
}

namespace
{

// OBJ face corner 'v', 'v/t', 'v//n', or 'v/t/n' to 0-based indices;
// missing entries are -1.
void parse_corner(const std::string &token, int &v, int &t, int &n)
{
    size_t s1 = token.find('/');
    v = std::stoi(token.substr(0, s1)) - 1;
    t = -1;
    n = -1;
    if (s1 != std::string::npos) {
        size_t s2 = token.find('/', s1 + 1);
        std::string t_str = token.substr(s1 + 1, s2 == std::string::npos ? std::string::npos : s2 - s1 - 1);
        if (!t_str.empty())
            t = std::stoi(t_str) - 1;
        if (s2 != std::string::npos && s2 + 1 < token.size())
            n = std::stoi(token.substr(s2 + 1)) - 1;
    }
}

// Cross products summed per vertex; the cross-product length is twice the
// triangle area, which provides the area weighting for free.
std::vector<vec3d> area_weighted_vertex_normals(const std::vector<vec3d> &positions,
                                                const std::vector<Eigen::Vector3i> &faces)
{
    std::vector<vec3d> normals(positions.size(), vec3d::Zero());
    for (const auto &f : faces) {
        vec3d n = (positions[f[1]] - positions[f[0]]).cross(positions[f[2]] - positions[f[0]]);
        normals[f[0]] += n;
        normals[f[1]] += n;
        normals[f[2]] += n;
    }
    return normals;
}

} // namespace

BaseMesh load_base_obj(const fs::path &path)
{
    std::ifstream file(path);
    ASSERT(file.is_open(), "Cannot open OBJ file [%s].", path.string().c_str());

    BaseMesh mesh;
    bool has_vn = false;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string head;
        if (!(ss >> head))
            continue;
        if (head == "v") {
            double x, y, z;
            ss >> x >> y >> z;
            mesh.positions.emplace_back(x, y, z);
        } else if (head == "vt") {
            double s, t;
            ss >> s >> t;
            mesh.texcoords.emplace_back(s, t);
        } else if (head == "vn") {
            double x, y, z;
            ss >> x >> y >> z;
            mesh.normals.emplace_back(x, y, z);
            has_vn = true;
        } else if (head == "f") {
            struct Corner
            {
                int v, t, n;
            };
            std::vector<Corner> corners;
            std::string token;
            while (ss >> token) {
                Corner c;
                parse_corner(token, c.v, c.t, c.n);
                corners.push_back(c);
            }
            for (size_t k = 1; k + 1 < corners.size(); ++k) { // fan triangulation
                mesh.faces.emplace_back(corners[0].v, corners[k].v, corners[k + 1].v);
                mesh.face_normal_indices.emplace_back(corners[0].n, corners[k].n, corners[k + 1].n);
                mesh.face_texcoord_indices.emplace_back(corners[0].t, corners[k].t, corners[k + 1].t);
            }
        }
    }

    if (has_vn) {
        for (const auto &ni : mesh.face_normal_indices) {
            ASSERT(ni.minCoeff() >= 0, "[%s]: has vn but faces lack normal indices.", path.string().c_str());
        }
    } else {
        mesh.normals = area_weighted_vertex_normals(mesh.positions, mesh.faces);
        mesh.face_normal_indices = mesh.faces;
    }
    for (auto &n : mesh.normals) {
        n.normalize();
    }
    return mesh;
}

} // namespace dmap
