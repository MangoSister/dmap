#include "traversal_test_util.h"
#include "ks/assertion.h"
#include "ks/log_util.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace ks;

namespace dmap
{

TraceResult trace(const ObjectIntersector &obj, const RayQuery &r, const TraversalOptions &options,
                  TraversalStats *stats)
{
    TraceResult out;
    out.hit = obj.intersect(r.o, r.d, 0.0, INFINITY, options, out.h, out.triangle, stats);
    return out;
}

vec3d uniform_direction(RNG &rng)
{
    double z = 1.0 - 2.0 * rng.next();
    constexpr double pi = 3.14159265358979323846;
    double phi = 2.0 * pi * rng.next();
    double s = std::sqrt(std::max(0.0, 1.0 - z * z));
    return vec3d(s * std::cos(phi), s * std::sin(phi), z);
}

vec2d uniform_bary(RNG &rng)
{
    double r = std::sqrt((double)rng.next());
    return vec2d(1.0 - r, r * rng.next());
}

bool identity_chart(const BaseTriangle &tri)
{
    return tri.t0 == vec2d(0.0, 0.0) && tri.t1 == vec2d(1.0, 0.0) && tri.t2 == vec2d(0.0, 1.0);
}

namespace
{

double median(std::vector<double> v)
{
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

} // namespace

ObjectSummary summarize_object(const DisplacedObject &d, const ObjectIntersector &obj)
{
    ObjectSummary s;
    s.n_triangles = (int)d.triangles.size();
    double w_leaf = d.asset->pyramid->cell_width(0);
    std::vector<double> spans(s.n_triangles);
    s.edges.resize(s.n_triangles);
    for (int k = 0; k < s.n_triangles; ++k) {
        const UvTriangle &dom = obj.triangles[k].domain;
        spans[k] = (dom.hi - dom.lo).maxCoeff() / w_leaf;
        s.edges[k] = mean_edge(d.triangles[k]);
        s.max_root_level = std::max(s.max_root_level, obj.triangles[k].root_level);
    }
    s.median_edge = median(s.edges);
    s.median_span = median(spans);
    s.identity = std::all_of(d.triangles.begin(), d.triangles.end(), identity_chart);
    return s;
}

void log_object(const char *task, const std::string &scene_name, const SceneSpec &spec, const DisplacedObject &d,
                const ObjectSummary &s)
{
    const SceneShape &shape = spec.shapes[d.shape];
    get_default_logger().info("{} on {} object [{}]: {} triangles, {} texels per tile, median span {:.1f} texels, "
                              "root levels up to {}, chart {}, displacement range {:.3g} over median edge {:.3g} = "
                              "{:.2f}, options {}",
                              task, scene_name, shape.name, s.n_triangles, d.asset->pyramid->n_leaf, s.median_span,
                              s.max_root_level, s.identity ? "identity" : "general", std::abs(d.asset->strength),
                              s.median_edge, std::abs(d.asset->strength) / s.median_edge, describe(d.traversal));
}

ObjectRays object_rays(const SceneSpec &spec, const DisplacedObject &d, const ObjectIntersector &obj,
                       const ObjectSummary &summary, int n, RNG &rng)
{
    ObjectRays out;
    if (spec.camera) {
        const SceneShape &shape = spec.shapes[d.shape];
        Transform to_object = shape.instances[0].inverse();
        for (int64_t tries = 0; tries < 200 * (int64_t)n && out.n_camera < n; ++tries) {
            Ray world = spec.camera->spawn_ray(vec2(rng.next(), rng.next()), vec2i(640, 480), 1);
            Ray local = transform_ray(to_object, world);
            RayQuery r{local.origin.cast<double>(), local.dir.cast<double>()};
            double t0 = 0.0, t1 = INFINITY;
            if (!obj.bound.clip(r.o, r.d, t0, t1))
                continue;
            out.rays.push_back(r);
            ++out.n_camera;
        }
    }
    int n_tri = (int)d.triangles.size();
    for (int k = 0; k < n; ++k) {
        int t = (int)(rng.next() * n_tri) % n_tri;
        vec2d b = uniform_bary(rng);
        vec2d st = d.triangles[t].param(b[0], b[1]);
        vec3d S = displaced_position(d.triangles[t], d.field, st[0], st[1]);
        double r = summary.edges[t] * std::pow(10.0, -1.0 + 1.5 * rng.next());
        out.rays.push_back(RayQuery{S + r * uniform_direction(rng), uniform_direction(rng)});
    }
    return out;
}

MeshOracle::MeshOracle(const EmbreeDevice &device, MeshData &&data, std::vector<TexelFace> &&face_record)
    : mesh(std::move(data)), faces(std::move(face_record))
{
    ASSERT((int)faces.size() == mesh.tri_count(), "face record does not match the mesh");
    geom = MeshGeometry(mesh);
    geom.create_rtc_geom(device);
    scene = rtcNewScene(device);
    rtcSetSceneFlags(scene, RTC_SCENE_FLAG_ROBUST);
    rtcAttachGeometry(scene, geom.rtcgeom);
    rtcCommitScene(scene);
}

MeshOracle::~MeshOracle() { rtcReleaseScene(scene); }

std::unique_ptr<MeshOracle> MeshOracle::barycentric(const EmbreeDevice &device, const SceneSpec &spec,
                                                    const DisplacedObject &d, int n)
{
    TessellationGrid grid = tessellation_grid(n);
    std::vector<TexelFace> record;
    for (size_t k = 0; k < d.triangles.size(); ++k) {
        for (const Eigen::Vector3i &f : grid.faces) {
            TexelFace face;
            face.triangle = (int)k;
            for (int v = 0; v < 3; ++v)
                face.st[v] = d.triangles[k].param(grid.params[f[v]][0], grid.params[f[v]][1]);
            record.push_back(face);
        }
    }
    return std::make_unique<MeshOracle>(device, spec.tessellated(d, n), std::move(record));
}

std::unique_ptr<MeshOracle> MeshOracle::texel_aligned(const EmbreeDevice &device, const SceneSpec &spec,
                                                      const DisplacedObject &d, int m)
{
    std::vector<TexelFace> record;
    MeshData data = spec.texel_tessellated(d, m, record);
    return std::make_unique<MeshOracle>(device, std::move(data), std::move(record));
}

MeshOracle::Hit MeshOracle::intersect(const RayQuery &r) const
{
    Hit out;
    RTCRayHit rh = spawn_rtcrayhit(r.o.cast<float>(), r.d.cast<float>(), 0.0f, std::numeric_limits<float>::infinity());
    if (!intersect1(scene, rh))
        return out;
    out.hit = true;
    out.t = rh.ray.tfar;
    out.prim = (int)rh.hit.primID;
    out.u = rh.hit.u;
    out.v = rh.hit.v;
    out.ng = vec3d(rh.hit.Ng_x, rh.hit.Ng_y, rh.hit.Ng_z).normalized();
    const TexelFace &face = faces[out.prim];
    out.triangle = face.triangle;
    out.st = (1.0 - out.u - out.v) * face.st[0] + out.u * face.st[1] + out.v * face.st[2];
    return out;
}

std::vector<double> chord_errors(const DisplacedObject &d, const MeshData &mesh, const std::vector<TexelFace> &faces)
{
    std::vector<double> worst(d.triangles.size(), 0.0);
    for (size_t f = 0; f < faces.size(); ++f) {
        const TexelFace &face = faces[f];
        if (!face.interior)
            continue;
        const BaseTriangle &tri = d.triangles[face.triangle];
        vec3d S[3];
        for (int v = 0; v < 3; ++v)
            S[v] = mesh.get_pos(mesh.indices[3 * f + v]).cast<double>();
        auto at = [&](const vec2d &st) { return displaced_position(tri, d.field, st[0], st[1]); };
        double &w = worst[face.triangle];
        for (int e = 0; e < 3; ++e) {
            int a = e, b = (e + 1) % 3;
            w = std::max(w, (at(0.5 * (face.st[a] + face.st[b])) - 0.5 * (S[a] + S[b])).norm());
        }
        w = std::max(w, (at((face.st[0] + face.st[1] + face.st[2]) / 3.0) - (S[0] + S[1] + S[2]) / 3.0).norm());
    }
    for (double &w : worst)
        w *= 2.0;
    return worst;
}

std::vector<double> MeshOracle::chord_errors(const DisplacedObject &d) const
{
    return dmap::chord_errors(d, mesh, faces);
}

double MeshOracle::nearest_on_triangle(const RayQuery &r, int k) const
{
    double best = INFINITY;
    for (size_t f = 0; f < faces.size(); ++f) {
        if (faces[f].triangle != k)
            continue;
        vec3d A = mesh.get_pos(mesh.indices[3 * f]).cast<double>();
        vec3d B = mesh.get_pos(mesh.indices[3 * f + 1]).cast<double>();
        vec3d C = mesh.get_pos(mesh.indices[3 * f + 2]).cast<double>();
        vec3d e1 = B - A, e2 = C - A, p = r.d.cross(e2);
        double det = e1.dot(p);
        if (det == 0.0)
            continue;
        vec3d s = r.o - A, q = s.cross(e1);
        double b1 = s.dot(p) / det, b2 = r.d.dot(q) / det, tt = e2.dot(q) / det;
        if (b1 >= 0.0 && b2 >= 0.0 && b1 + b2 <= 1.0 && tt > 0.0)
            best = std::min(best, tt);
    }
    return best;
}

size_t MeshOracle::array_bytes() const
{
    return sizeof(float) * (mesh.vertices.size() + mesh.vertex_normals.size() + mesh.texcoords.size()) +
           sizeof(uint32_t) * mesh.indices.size();
}

} // namespace dmap
