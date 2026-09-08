// Task render_aovs ("Plan — Path tracer with displaced surfaces", T6):
// primary-ray AOVs of a scene through ks's Scene with the displaced
// surfaces as user geometry, against the same scene with the displaced
// shapes replaced by their texel-aligned pre-tessellated substitutes (D4,
// substitute_subdivision sub-cells per texel). Per pixel: depth, the
// geometric normal, the texture coordinates, and the shape id. Images of
// both and of the differences go to the task directory.
// The comparison, over pixels where either scene hits a displaced shape:
// the fraction of pixels whose shape ids differ (the silhouette band), and
// over pixels with the same displaced shape, the fraction whose depth
// differs by more than the substitute's chord error (twice the largest
// deviation over the object's faces, over the incidence cosine, plus
// rounding), whose texture coordinates differ by more than uv_tolerance
// texels, and whose normals differ by more than normal_tolerance degrees,
// with the median angle. Verdict: PASS when every fraction is below its
// bound (max_id_fraction 0.02, max_depth_fraction 0.01, max_uv_fraction
// 0.01, max_normal_fraction 0.2, max_normal_median 10 degrees).
//
// Config: scene, width (320), height (240), substitute_subdivision (1),
// uv_tolerance (1.0), normal_tolerance (15.0), the five bounds.

#include "displaced_scene.h"
#include "ks/assertion.h"
#include "ks/config.h"
#include "ks/file_util.h"
#include "ks/log_util.h"
#include "ks/parallel.h"
#include "ks/render_target.h"
#include "scene_spec.h"
#include "traversal_test_util.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace ks;

namespace
{

struct Pixel
{
    bool hit = false;
    int shape = -1;
    int displaced = -1;
    float t = 0.0f;
    vec3 n = vec3::Zero();
    vec2 uv = vec2::Zero();
    float cos = 1.0f;
};

std::vector<Pixel> render(const dmap::DisplacedScene &built, const Camera &camera, int width, int height)
{
    std::vector<Pixel> image((size_t)width * height);
    parallel_for(width * height, [&](int index) {
        int x = index % width, y = index / width;
        Ray ray = camera.spawn_ray(vec2((x + 0.5f) / width, (y + 0.5f) / height), vec2i(width, height), 1);
        SceneHit hit;
        Pixel &p = image[index];
        if (!built.scene.intersect1(ray, hit))
            return;
        p.hit = true;
        p.shape = built.shape_of(hit.inst_id, hit.geom_id);
        p.displaced = built.displaced_of(hit.subscene_id, hit.geom_id);
        p.t = hit.it.thit;
        p.n = hit.it.frame.n;
        p.uv = hit.it.uv;
        p.cos = std::abs(ray.dir.normalized().dot(p.n));
    });
    return image;
}

// Linear EXR plus an sRGB PNG.
void save_rgb(const std::vector<vec3> &img, int width, int height, const fs::path &path_prefix)
{
    RenderTargetArgs args;
    args.width = width;
    args.height = height;
    args.backdrop = color3::Zero();
    RenderTarget2 rt(args);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            RenderTargetPixel p;
            p.main = img[(size_t)y * width + x].array();
            rt.add(x, y, 1.0f, p);
        }
    rt.composite_and_save_to_exr(path_prefix);
    rt.composite_and_save_to_png(path_prefix, {}, nullptr);
}

vec3 id_color(int id)
{
    if (id < 0)
        return vec3::Zero();
    uint32_t h = (uint32_t)id * 2654435761u;
    return vec3(((h >> 0) & 255) / 255.0f, ((h >> 8) & 255) / 255.0f, ((h >> 16) & 255) / 255.0f) * 0.7f +
           vec3::Constant(0.3f);
}

void save_aovs(const std::vector<Pixel> &image, int width, int height, float depth_scale, const fs::path &dir,
               const std::string &tag)
{
    size_t n = image.size();
    std::vector<vec3> depth(n), normal(n), uv(n), id(n);
    for (size_t i = 0; i < n; ++i) {
        const Pixel &p = image[i];
        depth[i] = p.hit ? vec3::Constant(p.t / depth_scale) : vec3::Zero();
        normal[i] = p.hit ? (0.5f * p.n + vec3::Constant(0.5f)).eval() : vec3::Zero();
        uv[i] = p.hit ? vec3(p.uv[0] - std::floor(p.uv[0]), p.uv[1] - std::floor(p.uv[1]), 0.0f) : vec3::Zero();
        id[i] = id_color(p.hit ? p.shape : -1);
    }
    save_rgb(depth, width, height, dir / (tag + "_depth"));
    save_rgb(normal, width, height, dir / (tag + "_normal"));
    save_rgb(uv, width, height, dir / (tag + "_uv"));
    save_rgb(id, width, height, dir / (tag + "_id"));
}

double median(std::vector<double> v)
{
    if (v.empty())
        return 0.0;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

} // namespace

void render_aovs(const ConfigArgs &args, const fs::path &task_dir, int task_id)
{
    std::string scene_name = args.load_string("scene");
    const dmap::SceneSpec *spec = args.asset_table().get<dmap::SceneSpec>(scene_name);
    ASSERT(spec, "no scene named [%s]", scene_name.c_str());
    spec->require_valid();
    ASSERT(spec->camera, "render_aovs needs the scene's camera");
    int width = args.load_integer("width", 320), height = args.load_integer("height", 240);
    int m = args.load_integer("substitute_subdivision", 1);
    double uv_tolerance = (double)args.load_float("uv_tolerance", 1.0f);
    double normal_tolerance = (double)args.load_float("normal_tolerance", 15.0f);
    double max_id = (double)args.load_float("max_id_fraction", 0.02f);
    double max_depth = (double)args.load_float("max_depth_fraction", 0.01f);
    double max_uv = (double)args.load_float("max_uv_fraction", 0.01f);
    double max_normal = (double)args.load_float("max_normal_fraction", 0.2f);
    double max_normal_median = (double)args.load_float("max_normal_median", 10.0f);
    constexpr double float_rounding = 1e-5;

    auto &log = get_default_logger();
    EmbreeDevice device;
    std::unique_ptr<dmap::DisplacedScene> ours = dmap::create_displaced_scene(*spec, device, 0);
    std::unique_ptr<dmap::DisplacedScene> theirs = dmap::create_displaced_scene(*spec, device, m);
    AABB3 bound = ours->scene.bound();
    double scale = (bound.max - bound.min).norm();

    // The chord error and the texel size of each displaced object.
    std::vector<double> chord(spec->displaced.size(), 0.0), texel(spec->displaced.size(), 0.0);
    for (size_t di = 0; di < spec->displaced.size(); ++di) {
        const dmap::DisplacedObject &d = spec->displaced[di];
        std::vector<double> per_triangle =
            dmap::chord_errors(d, *theirs->substitutes[di], theirs->substitute_faces[di]);
        chord[di] = per_triangle.empty() ? 0.0 : *std::max_element(per_triangle.begin(), per_triangle.end());
        texel[di] = 1.0 / (d.asset->pyramid->n_leaf * d.uv_scale.cwiseAbs().maxCoeff());
        log.info("  [{}]: substitute {} faces, chord error {:.3e}, texel {:.3e} in texture coordinates",
                 spec->shapes[d.shape].name, theirs->substitutes[di]->tri_count(), chord[di], texel[di]);
    }

    std::vector<Pixel> a = render(*ours, *spec->camera, width, height);
    std::vector<Pixel> b = render(*theirs, *spec->camera, width, height);
    float depth_scale = (float)scale;
    save_aovs(a, width, height, depth_scale, task_dir, "displaced");
    save_aovs(b, width, height, depth_scale, task_dir, "substitute");

    int64_t n_either = 0, n_id = 0, n_same = 0, n_depth = 0, n_uv = 0, n_normal = 0;
    std::vector<double> angles;
    std::vector<vec3> diff(a.size(), vec3::Zero());
    for (size_t i = 0; i < a.size(); ++i) {
        const Pixel &p = a[i], &q = b[i];
        bool either = p.displaced >= 0 || q.displaced >= 0;
        if (!either)
            continue;
        ++n_either;
        if (p.shape != q.shape) {
            ++n_id;
            diff[i] = vec3(1.0f, 0.0f, 0.0f);
            continue;
        }
        ++n_same;
        int di = p.displaced;
        double cos = std::max((double)q.cos, 0.1);
        double depth_tolerance = 2.0 * chord[di] / cos + float_rounding * (q.t + scale);
        double dt = std::abs((double)p.t - q.t);
        if (dt > depth_tolerance) {
            ++n_depth;
            diff[i][1] = 1.0f;
        }
        double duv = (p.uv - q.uv).cast<double>().norm() / texel[di];
        if (duv > uv_tolerance) {
            ++n_uv;
            diff[i][2] = 1.0f;
        }
        double angle = std::acos(std::clamp((double)p.n.dot(q.n), -1.0, 1.0)) * 180.0 / 3.14159265358979323846;
        angles.push_back(angle);
        if (angle > normal_tolerance) {
            ++n_normal;
            diff[i][0] = std::max(diff[i][0], 0.5f);
        }
    }
    save_rgb(diff, width, height, task_dir / "difference");

    auto frac = [](int64_t n, int64_t of) { return of ? (double)n / (double)of : 0.0; };
    double f_id = frac(n_id, n_either), f_depth = frac(n_depth, n_same), f_uv = frac(n_uv, n_same),
           f_normal = frac(n_normal, n_same), med = median(angles);
    log.info("render_aovs on {} at {}x{}, substitutes at {} sub-cells per texel: {} pixels see a displaced shape "
             "in either scene",
             scene_name, width, height, m, n_either);
    log.info("  shape id differs on {} ({:.2f}%, bound {:.0f}%); of {} pixels with the same displaced shape: depth "
             "beyond the chord error on {} ({:.2f}%, bound {:.0f}%), uv beyond {:.1f} texels on {} ({:.2f}%, bound "
             "{:.0f}%), normal beyond {:.0f} degrees on {} ({:.2f}%, bound {:.0f}%), median angle {:.2f} degrees "
             "(bound {:.0f})",
             n_id, 100.0 * f_id, 100.0 * max_id, n_same, n_depth, 100.0 * f_depth, 100.0 * max_depth, uv_tolerance,
             n_uv, 100.0 * f_uv, 100.0 * max_uv, normal_tolerance, n_normal, 100.0 * f_normal, 100.0 * max_normal, med,
             max_normal_median);
    bool pass =
        f_id <= max_id && f_depth <= max_depth && f_uv <= max_uv && f_normal <= max_normal && med <= max_normal_median;
    log.info("VERDICT: {} — AOVs on {}", pass ? "PASS" : "FAIL", scene_name);
}
