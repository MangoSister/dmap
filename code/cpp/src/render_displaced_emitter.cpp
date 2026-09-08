// Phase S8 ("Plan — A-MVP sampling implementation"): rendered images of the
// displaced emitter. Scene: one displaced emissive triangle (the S7 asset)
// over a diffuse floor; the S2 mesh is the visibility geometry, the light
// is the tessellation-free DisplacedEmitterLight. Direct lighting only,
// small_pt's structure (parallel tiles, RenderTarget2, camera spawn_ray).
//
// Two experiments, per the plan:
// (1) MIS correctness check: with the product-descent light, NEE-only,
//     BSDF-sampling-only, and MIS renders must converge to the same image —
//     mean luminance within tolerance of a high-spp reference. This is the
//     standard check that the solid-angle pdf conversion and the
//     pdf-of-direction lookup are consistent.
// (2) Equal-sample ladder: NEE-only renders for the six light samplers
//     (the uniform line-casting baseline plus the five point samplers) at
//     a low spp, relative MSE against the reference (FLIP is computed by
//     code/python/poc/experiments/flip_s8.py from the saved EXRs).
//
// The BSDF path finds the emitter through the S2 mesh and contributes only
// its hit (u, v); Le and the MIS pdf are re-derived from the smooth surface
// there, so both sides of the MIS weight use identical arithmetic and the
// mesh enters only through visibility (n_tess is chosen finer than a texel
// to keep that approximation below the check tolerance).

#include "base_mesh.h"
#include "displaced_emitter_light.h"
#include "displaced_surface.h"
#include "displaced_tessellation.h"
#include "ks/assertion.h"
#include "ks/camera.h"
#include "ks/config.h"
#include "ks/embree_util.h"
#include "ks/log_util.h"
#include "ks/parallel.h"
#include "ks/render_target.h"
#include "ks/rng.h"
#include "line_sampling.h"
#include "texture_grid.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace ks;

namespace
{

struct RenderScene
{
    RTCScene scene = nullptr;
    RTCGeometry g_emitter = nullptr, g_floor = nullptr;
    uint32_t emitter_id = 0, floor_id = 0;
    const MeshData *emitter_md = nullptr;

    RenderScene(const EmbreeDevice &device, const MeshData &emitter, const MeshData &floor) : emitter_md(&emitter)
    {
        auto make_geom = [&](const MeshData &md) {
            RTCGeometry g = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
            rtcSetSharedGeometryBuffer(g, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, md.vertices.data(), 0,
                                       sizeof(float[3]), (md.vertices.size() - 1) / 3);
            rtcSetSharedGeometryBuffer(g, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, md.indices.data(), 0,
                                       sizeof(uint32_t[3]), md.indices.size() / 3);
            rtcCommitGeometry(g);
            return g;
        };
        scene = rtcNewScene(device);
        rtcSetSceneBuildQuality(scene, RTC_BUILD_QUALITY_HIGH);
        g_emitter = make_geom(emitter);
        g_floor = make_geom(floor);
        emitter_id = rtcAttachGeometry(scene, g_emitter);
        floor_id = rtcAttachGeometry(scene, g_floor);
        rtcCommitScene(scene);
    }
    ~RenderScene()
    {
        rtcReleaseScene(scene);
        rtcReleaseGeometry(g_emitter);
        rtcReleaseGeometry(g_floor);
    }

    // (u, v) of an emitter hit from the mesh texcoords.
    vec2d emitter_uv(uint32_t prim_id, float b1, float b2) const
    {
        const MeshData &md = *emitter_md;
        uint32_t i0 = md.indices[3 * prim_id], i1 = md.indices[3 * prim_id + 1], i2 = md.indices[3 * prim_id + 2];
        return (1.0 - b1 - b2) * md.get_texcoord(i0).cast<double>() + (double)b1 * md.get_texcoord(i1).cast<double>() +
               (double)b2 * md.get_texcoord(i2).cast<double>();
    }
};

enum class Strategy
{
    NEE,
    BSDF,
    MIS
};

struct RenderSetup
{
    const RenderScene *scene;
    const dmap::DisplacedEmitterLight *light;
    vec3d floor_n, floor_t1, floor_t2; // floor frame (n = unit normal)
    double albedo;
    vec3d tint; // emitter radiance color, scalar Le times this

    // Uniform baseline (Ling et al., §5A baseline 4): when set, NEE draws
    // one line per sample and sums the integrand over its emitter hits,
    // each reweighted from mesh area to smooth area (the S7 estimator).
    // NEE only — a line yields a correlated hit set, not a point with a
    // density, so there is no per-point pdf for MIS.
    const dmap::AllHitsMesh *ling_mesh = nullptr;
    const dmap::LineSampler *ling_lines = nullptr;
    const std::vector<double> *ling_uv_per_area = nullptr;
};

double power_heur(double p, double q) { return p * p / (p * p + q * q); }

vec3d shade(const RenderSetup &s, const Ray &camera_ray, Strategy strategy, RNG &rng)
{
    RTCRayHit rayhit = to_rtcrayhit(camera_ray);
    if (!intersect1(s.scene->scene, rayhit))
        return vec3d::Zero();

    if (rayhit.hit.geomID == s.scene->emitter_id) {
        vec2d uv = s.scene->emitter_uv(rayhit.hit.primID, rayhit.hit.u, rayhit.hit.v);
        return s.light->Le_at(uv[0], uv[1]) * s.tint; // directly visible emission
    }

    // Floor: Lambertian direct lighting from the emitter.
    vec3d x = camera_ray.origin.cast<double>() + (double)rayhit.ray.tfar * camera_ray.dir.cast<double>();
    const vec3d &n = s.floor_n;
    if (camera_ray.dir.cast<double>().dot(n) > 0.0)
        return vec3d::Zero(); // floor seen from below
    double brdf = s.albedo / pi;

    vec3d L = vec3d::Zero();

    if (strategy != Strategy::BSDF && s.ling_mesh) {
        static thread_local std::vector<dmap::LineHit> hits;
        vec3 o, d;
        float t_far;
        double x_sum = 0.0;
        if (s.ling_lines->sample(rng, o, d, t_far)) {
            s.ling_mesh->all_hits(o, d, 0.0f, t_far, hits);
            for (const dmap::LineHit &h : hits) {
                vec2d uv = s.scene->emitter_uv(h.prim_id, h.b1, h.b2);
                double Le = s.light->Le_at(uv[0], uv[1]);
                if (Le == 0.0)
                    continue;
                dmap::PointwiseFields f = dmap::pointwise_fields(*s.light->tri, s.light->field, uv[0], uv[1]);
                vec3d y = s.light->tri->P(uv[0], uv[1]) + f.h * dmap::normal_frame_at(*s.light->tri, uv[0], uv[1]).N;
                vec3d dvec = y - x;
                double dist = dvec.norm();
                vec3d wi = dvec / dist;
                double cos_x = n.dot(wi);
                if (cos_x <= 0.0)
                    continue;
                RTCRay shadow = spawn_rtcray<OffsetType::Shadow>(x.cast<float>(), wi.cast<float>(), n.cast<float>(),
                                                                 0.0f, (float)(0.99 * dist));
                if (occlude1(s.scene->scene, shadow))
                    continue;
                double g = cos_x * std::abs(f.n.normalized().dot(wi)) / (dist * dist);
                x_sum += Le * brdf * g * f.sqrt_det * (*s.ling_uv_per_area)[h.prim_id];
            }
        }
        L += 2.0 * s.ling_lines->offset_area() * x_sum * s.tint;
        return L; // uniform baseline is NEE-only
    }

    if (strategy != Strategy::BSDF) {
        dmap::EmitterLightSample ls = s.light->sample(x, n, rng);
        if (ls.ok && ls.Le > 0.0) {
            double cos_x = n.dot(ls.wi);
            if (cos_x > 0.0) {
                RTCRay shadow = spawn_rtcray<OffsetType::Shadow>(x.cast<float>(), ls.wi.cast<float>(), n.cast<float>(),
                                                                 0.0f, (float)(0.99 * ls.dist));
                if (!occlude1(s.scene->scene, shadow)) {
                    double w = 1.0;
                    if (strategy == Strategy::MIS)
                        w = power_heur(ls.pdf_omega, cos_x / pi);
                    L += ls.Le * s.tint * (brdf * cos_x * w / ls.pdf_omega);
                }
            }
        }
    }

    if (strategy != Strategy::NEE) {
        vec3 wi_local = sample_cosine_hemisphere(rng.next2d());
        double pdf_b = wi_local.z() * inv_pi;
        if (pdf_b > 0.0) {
            vec3d wi = (double)wi_local.x() * s.floor_t1 + (double)wi_local.y() * s.floor_t2 + (double)wi_local.z() * n;
            RTCRayHit brh;
            brh.ray =
                spawn_rtcray<OffsetType::NextBounce>(x.cast<float>(), wi.cast<float>(), n.cast<float>(), 0.0f, inf);
            brh.hit.geomID = RTC_INVALID_GEOMETRY_ID;
            brh.hit.primID = RTC_INVALID_GEOMETRY_ID;
            if (intersect1(s.scene->scene, brh) && brh.hit.geomID == s.scene->emitter_id) {
                vec2d uv = s.scene->emitter_uv(brh.hit.primID, brh.hit.u, brh.hit.v);
                double Le = s.light->Le_at(uv[0], uv[1]);
                if (Le > 0.0) {
                    double w = 1.0;
                    if (strategy == Strategy::MIS)
                        w = power_heur(pdf_b, s.light->pdf_omega_from_uv(x, n, uv[0], uv[1]));
                    // (brdf * cos_x) / pdf_b = albedo for cosine sampling.
                    L += Le * s.tint * (s.albedo * w);
                }
            }
        }
    }
    return L;
}

// One full render; returns the mean image (linear RGB, row-major).
std::vector<vec3d> render(const RenderSetup &s, const Camera &camera, Strategy strategy, int width, int height, int spp,
                          uint64_t seed_salt)
{
    std::vector<vec3d> img((size_t)width * height, vec3d::Zero());
    parallel_tile_2d(width, height, [&](int x, int y) {
        size_t pix = (size_t)y * width + x;
        vec3d acc = vec3d::Zero();
        for (int k = 0; k < spp; ++k) {
            RNG rng(pix + seed_salt * img.size(), (uint64_t)k);
            vec2 fp((x + rng.next()) / width, (y + rng.next()) / height);
            Ray ray = camera.spawn_ray(fp, vec2i(width, height), 1);
            acc += shade(s, ray, strategy, rng);
        }
        img[pix] = acc / spp;
    });
    return img;
}

double luminance(const vec3d &c) { return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; }

double mean_luminance(const std::vector<vec3d> &img)
{
    double sum = 0.0;
    for (const vec3d &c : img)
        sum += luminance(c);
    return sum / img.size();
}

// Relative MSE on luminance, with an epsilon tied to the reference's mean
// so dark pixels do not dominate.
double rel_mse(const std::vector<vec3d> &img, const std::vector<vec3d> &ref)
{
    double eps = 1e-2 * mean_luminance(ref);
    eps *= eps;
    double sum = 0.0;
    for (size_t k = 0; k < img.size(); ++k) {
        double d = luminance(img[k]) - luminance(ref[k]);
        sum += d * d / (luminance(ref[k]) * luminance(ref[k]) + eps);
    }
    return sum / img.size();
}

// Linear EXR (the metric input) plus an exposed sRGB PNG for eyes.
void save_images(const std::vector<vec3d> &img, int width, int height, const fs::path &path_prefix, double png_exposure)
{
    RenderTargetArgs args;
    args.width = width;
    args.height = height;
    args.backdrop = color3::Zero();
    RenderTarget2 rt(args);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            RenderTargetPixel p;
            p.main = img[(size_t)y * width + x].cast<float>().array();
            rt.add(x, y, 1.0f, p);
        }
    rt.composite_and_save_to_exr(path_prefix);
    RenderTarget2 rt_png(args);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            RenderTargetPixel p;
            p.main = (png_exposure * img[(size_t)y * width + x]).cast<float>().array();
            rt_png.add(x, y, 1.0f, p);
        }
    rt_png.composite_and_save_to_png(path_prefix, {}, nullptr);
}

} // namespace

void render_displaced_emitter(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    fs::path mesh_path = args.load_path("mesh");
    fs::path tex_path = args.load_path("texture");
    int tex_nodes = args.load_integer("tex_nodes", 65);
    double amplitude = (double)args.load_float("amplitude", 0.2f);
    int triangle_index = args.load_integer("triangle_index", -1);
    int n_tess = args.load_integer("n_tess", 128);
    double beta = (double)args.load_float("beta", 0.05f);
    dmap::PyramidBuild build = dmap::pyramid_build_from_string(args.load_string("pyramid_build", "fold"));
    double emission_scale = (double)args.load_float("emission_scale", 8.0f);
    double albedo = (double)args.load_float("albedo", 0.7f);
    double floor_offset = (double)args.load_float("floor_offset", 1.2f);
    double floor_extent = (double)args.load_float("floor_extent", 6.0f);
    int width = args.load_integer("width", 384);
    int height = args.load_integer("height", 288);
    float vfov = args.load_float("vfov", 40.0f);
    int spp_check = args.load_integer("spp_check", 64);
    int spp_ladder = args.load_integer("spp_ladder", 16);
    int spp_ref = args.load_integer("spp_ref", 1024);
    double mis_tolerance = (double)args.load_float("mis_tolerance", 0.02f);
    double png_exposure = (double)args.load_float("png_exposure", 2.0f);

    dmap::BaseMesh mesh = dmap::load_base_obj(mesh_path);
    dmap::TextureGrid tex = dmap::downsample_box(dmap::load_height_texture(tex_path), tex_nodes);
    int n_leaf = tex_nodes - 1;
    if (triangle_index < 0)
        triangle_index = mesh.n_triangles() / 3;
    dmap::BaseTriangle tri = mesh.triangle(triangle_index);
    double me = dmap::mean_edge(tri);
    dmap::HeightGrid field{tex.W, tex.H, amplitude * me, tex.values.data()};

    // Emission: checkerboard with exact-zero cells (structured, and the
    // certified-zero pruning shows up in the picture), scaled to radiance.
    dmap::TextureGrid em = dmap::checkerboard(n_leaf, 8, 0.0, emission_scale);
    dmap::TaylorPyramid pyramid(field, build);
    dmap::EmissionTile em_tile(em);

    // Triangle frame: t1, t2 in the base plane, nc the displaced-centre
    // normal. All scene placement is in this frame, in mean-edge units.
    dmap::PointwiseFields fc = dmap::pointwise_fields(tri, field, 1.0 / 3.0, 1.0 / 3.0);
    vec3d nc = fc.n.normalized();
    vec3d yc = tri.P(1.0 / 3.0, 1.0 / 3.0) + fc.h * dmap::normal_frame_at(tri, 1.0 / 3.0, 1.0 / 3.0).N;
    vec3d t1 = (tri.e1 - tri.e1.dot(nc) * nc).normalized();
    vec3d t2 = nc.cross(t1);
    auto frame_point = [&](const ConfigArgs &a, const std::string &name) {
        return yc + me * ((double)a[name].load_float(0) * t1 + (double)a[name].load_float(1) * t2 +
                          (double)a[name].load_float(2) * nc);
    };

    // Geometry: S2 emitter mesh and the floor quad below.
    ks::MeshData emitter_md;
    dmap::append_displaced_triangle(emitter_md, tri, field, n_tess);
    dmap::finalize_mesh_data(emitter_md);
    ks::MeshData floor_md;
    {
        vec3d c = yc - floor_offset * me * nc;
        double ext = 0.5 * floor_extent * me;
        vec3d v[4] = {c - ext * t1 - ext * t2, c + ext * t1 - ext * t2, c + ext * t1 + ext * t2,
                      c - ext * t1 + ext * t2};
        for (const vec3d &p : v)
            for (int k = 0; k < 3; ++k)
                floor_md.vertices.push_back((float)p[k]);
        floor_md.indices = {0, 1, 2, 0, 2, 3}; // ccw seen from +nc
        dmap::finalize_mesh_data(floor_md);
    }
    EmbreeDevice device;
    RenderScene scene(device, emitter_md, floor_md);

    // Uniform baseline machinery: all-hits line casting over the emitter,
    // with the mesh-to-parameter density per micro-triangle (see S7).
    dmap::AllHitsMesh ling_mesh(device, emitter_md);
    dmap::LineSampler ling_lines(ling_mesh.bound());
    std::vector<double> ling_uv_per_area(emitter_md.tri_count());
    for (int t = 0; t < emitter_md.tri_count(); ++t) {
        vec3d p0 = emitter_md.get_pos(emitter_md.indices[3 * t]).cast<double>(),
              p1 = emitter_md.get_pos(emitter_md.indices[3 * t + 1]).cast<double>(),
              p2 = emitter_md.get_pos(emitter_md.indices[3 * t + 2]).cast<double>();
        double area = 0.5 * (p1 - p0).cross(p2 - p0).norm();
        ling_uv_per_area[t] = (0.5 / ((double)n_tess * n_tess)) / area;
    }

    const std::pair<dmap::EmitterSamplerKind, const char *> ladder[] = {
        {dmap::EmitterSamplerKind::EmissionTable, "emission-table"},
        {dmap::EmitterSamplerKind::ProductTable, "product-table"},
        {dmap::EmitterSamplerKind::AreaDescent, "area-descent"},
        {dmap::EmitterSamplerKind::ProductDescent, "product-descent"},
        {dmap::EmitterSamplerKind::ReceiverDescent, "receiver-descent"},
    };
    dmap::DisplacedEmitterLight light_prod(tri, field, pyramid, em_tile, dmap::EmitterSamplerKind::ProductDescent,
                                           beta);

    RenderSetup setup;
    setup.scene = &scene;
    setup.light = &light_prod;
    setup.floor_n = nc;
    setup.floor_t1 = t1;
    setup.floor_t2 = t2;
    setup.albedo = albedo;
    setup.tint = vec3d(1.0, 0.62, 0.35);

    std::ofstream csv(task_dir / "render_metrics.csv");
    csv << "viewpoint,image,spp,rel_mse,mean_lum,mean_lum_rel_diff\n";
    bool pass = true;

    const char *viewpoints[] = {"cam1", "cam2"};
    for (int vp = 0; vp < 2; ++vp) {
        std::string cp = std::string(viewpoints[vp]) + "_pos", ct = std::string(viewpoints[vp]) + "_target";
        Camera camera(frame_point(args, cp).cast<float>(), frame_point(args, ct).cast<float>(), nc.cast<float>(),
                      to_radian(vfov), (float)width / height);

        std::vector<vec3d> ref = render(setup, camera, Strategy::MIS, width, height, spp_ref, 1000 + vp);
        double ref_lum = mean_luminance(ref);
        save_images(ref, width, height, task_dir / (std::string(viewpoints[vp]) + "_reference"), png_exposure);

        // Framing sanity: how much of the image sees emitter / floor light.
        {
            size_t lit = 0;
            for (const vec3d &c : ref)
                if (luminance(c) > 1e-4 * ref_lum)
                    ++lit;
            get_default_logger().info("[{}] reference mean luminance {:.4f}, lit fraction {:.2f}", viewpoints[vp],
                                      ref_lum, (double)lit / ref.size());
        }
        csv << viewpoints[vp] << ",reference," << spp_ref << ",0,0," << ref_lum << ",0\n";

        // (1) MIS correctness check, viewpoint 1 only.
        if (vp == 0) {
            const std::pair<Strategy, const char *> strategies[] = {
                {Strategy::NEE, "nee"}, {Strategy::BSDF, "bsdf"}, {Strategy::MIS, "mis"}};
            for (auto [strat, name] : strategies) {
                std::vector<vec3d> img = render(setup, camera, strat, width, height, spp_check, 2000 + (int)strat);
                double lum = mean_luminance(img), mse = rel_mse(img, ref);
                double rel = std::abs(lum - ref_lum) / ref_lum;
                bool ok = rel <= mis_tolerance;
                pass &= ok;
                get_default_logger().info("[{}] (1) {} @{} spp: mean luminance {:.4f} (ref {:.4f}, rel diff "
                                          "{:.4f}), rel MSE {:.4f} {}",
                                          viewpoints[vp], name, spp_check, lum, ref_lum, rel, mse, ok ? "ok" : "FAIL");
                save_images(img, width, height, task_dir / (std::string(viewpoints[vp]) + "_" + name), png_exposure);
                csv << viewpoints[vp] << "," << name << "," << spp_check << "," << mse << "," << lum << "," << rel
                    << "\n";
            }
        }

        // (2) Equal-sample ladder, NEE-only. The uniform baseline first
        // (one line per sample), then the five point samplers.
        {
            RenderSetup s2 = setup;
            s2.ling_mesh = &ling_mesh;
            s2.ling_lines = &ling_lines;
            s2.ling_uv_per_area = &ling_uv_per_area;
            std::vector<vec3d> img = render(s2, camera, Strategy::NEE, width, height, spp_ladder, 3100);
            double mse = rel_mse(img, ref);
            double lum = mean_luminance(img);
            get_default_logger().info("[{}] (2) uniform-ling NEE @{} spp: rel MSE {:.4f}", viewpoints[vp], spp_ladder,
                                      mse);
            save_images(img, width, height, task_dir / (std::string(viewpoints[vp]) + "_ladder_uniform-ling"),
                        png_exposure);
            csv << viewpoints[vp] << ",ladder_uniform-ling," << spp_ladder << "," << mse << "," << lum << ","
                << std::abs(lum - ref_lum) / ref_lum << "\n";
        }
        for (auto [kind, name] : ladder) {
            dmap::DisplacedEmitterLight light(tri, field, pyramid, em_tile, kind, beta);
            RenderSetup s2 = setup;
            s2.light = &light;
            std::vector<vec3d> img = render(s2, camera, Strategy::NEE, width, height, spp_ladder, 3000 + (int)kind);
            double mse = rel_mse(img, ref);
            double lum = mean_luminance(img);
            get_default_logger().info("[{}] (2) {} NEE @{} spp: rel MSE {:.4f}", viewpoints[vp], name, spp_ladder, mse);
            save_images(img, width, height, task_dir / (std::string(viewpoints[vp]) + "_ladder_" + name), png_exposure);
            csv << viewpoints[vp] << ",ladder_" << name << "," << spp_ladder << "," << mse << "," << lum << ","
                << std::abs(lum - ref_lum) / ref_lum << "\n";
        }
    }

    csv.close();
    get_default_logger().info("wrote images and {}", (task_dir / "render_metrics.csv").string());
    get_default_logger().info("VERDICT: {} — NEE-only, BSDF-sampling-only, and MIS converge to the same image "
                              "(mean luminance within {:.0f}% of the reference); the ladder images and metrics are "
                              "the S8 deliverable (FLIP via flip_s8.py)",
                              pass ? "PASS" : "FAIL", 100.0 * mis_tolerance);
    if (!pass)
        std::exit(1);
}
