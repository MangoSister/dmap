#include "displaced_intersector.h"
#include "descent_sampler.h"
#include "ks/assertion.h"
#include "ks/embree_util.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

namespace
{

int64_t floor_div2(int64_t x) { return x >= 0 ? x / 2 : -((-x + 1) / 2); }
int floor_mod2(int64_t x) { return (int)(x - 2 * floor_div2(x)); }

// An orthonormal basis (d1, d2) of the plane normal to the unit vector n.
void plane_basis(const vec3d &n, vec3d &d1, vec3d &d2)
{
    vec3d a = std::abs(n[0]) < 0.9 ? vec3d(1.0, 0.0, 0.0) : vec3d(0.0, 1.0, 0.0);
    d1 = a.cross(n).normalized();
    d2 = n.cross(d1);
}

// Möller–Trumbore in double; b1 and b2 weight B and C.
bool ray_triangle(const vec3d &o, const vec3d &d, double t_min, double t_max, const vec3d &A, const vec3d &B,
                  const vec3d &C, double &t, double &b1, double &b2)
{
    vec3d e1 = B - A, e2 = C - A;
    vec3d p = d.cross(e2);
    double det = e1.dot(p);
    if (det == 0.0)
        return false;
    double inv = 1.0 / det;
    vec3d s = o - A;
    b1 = s.dot(p) * inv;
    if (b1 < 0.0 || b1 > 1.0)
        return false;
    vec3d q = s.cross(e1);
    b2 = d.dot(q) * inv;
    if (b2 < 0.0 || b1 + b2 > 1.0)
        return false;
    t = e2.dot(q) * inv;
    return t > t_min && t < t_max;
}

} // namespace

// What the certified leaf reads per texel: the corner heights, the
// normalized normal as an affine vector over the whole texel, and the ray
// with its normal-plane basis.
struct DisplacedIntersector::LeafBound
{
    vec2d lo;
    double w = 0.0;
    double h00 = 0.0, h10 = 0.0, h01 = 0.0, h11 = 0.0;
    AffineVec3 N_hat;
    vec3d o, d, dn, d1, d2;
    double d_len = 0.0;
};

namespace
{

// The form over the sub-square of the texel with centre (a, b) and
// half-width rho, both in texel units, from the form over the texel: the
// symbols shift and scale, the remainder stays.
AffineForm restricted(const AffineForm &f, double a, double b, double rho)
{
    double alpha = 2.0 * a - 1.0, beta = 2.0 * b - 1.0, r = 2.0 * rho;
    return AffineForm(f.c + f.a * alpha + f.b * beta, f.a * r, f.b * r, f.k);
}

AffineVec3 restricted(const AffineVec3 &v, double a, double b, double rho)
{
    return {restricted(v.x, a, b, rho), restricted(v.y, a, b, rho), restricted(v.z, a, b, rho)};
}

} // namespace

void cell_up(Cell &c)
{
    ++c.level;
    c.i = floor_div2(c.i);
    c.j = floor_div2(c.j);
}

void cell_down(Cell &c, bool sign_i, bool sign_j)
{
    --c.level;
    c.i = 2 * c.i + (sign_i ? 1 : 0);
    c.j = 2 * c.j + (sign_j ? 1 : 0);
}

void cell_next(Cell &c, bool sign_i, bool sign_j, int root_level)
{
    // Within a block the order is (first i, first j), (first i, second j),
    // (second i, first j), (second i, second j), "first" being the side
    // the ray comes from; after the last child, climb.
    while (true) {
        int code = 2 * floor_mod2(c.i + (sign_i ? 1 : 0)) + floor_mod2(c.j + (sign_j ? 1 : 0));
        switch (code) {
        case 1:
            c.j += sign_j ? 1 : -1;
            c.i += sign_i ? -1 : 1;
            return;
        case 3:
            cell_up(c);
            if (c.level > root_level)
                return;
            break;
        default:
            c.j += sign_j ? -1 : 1;
            return;
        }
    }
}

void TraversalStats::add(const TraversalStats &other)
{
    triangle_tests += other.triangle_tests;
    node_tests += other.node_tests;
    leaf_tests += other.leaf_tests;
    sub_squares += other.sub_squares;
    for (int l = 0; l < max_levels; ++l)
        node_tests_per_level[l] += other.node_tests_per_level[l];
    node_tests_after_hit += other.node_tests_after_hit;
    leaf_tests_after_hit += other.leaf_tests_after_hit;
}

DisplacedIntersector::DisplacedIntersector(const BaseTriangle &tri, const TaylorPyramid &pyramid,
                                           const HeightGrid &field)
    : tri(&tri), pyramid(&pyramid), field(field), frame(tangent_frame(tri)), domain(tri.t0, tri.t1, tri.t2)
{
    // TFDM's findRoots: the cells of the root block that meet the domain's
    // bounding box, j outer and i inner.
    int64_t i0, j0;
    footprint_roots(domain, pyramid.n_leaf, root_level, i0, j0);
    double w = pyramid.cell_width(root_level);
    int64_t i_lo = (int64_t)std::floor(domain.lo[0] / w), i_hi = (int64_t)std::ceil(domain.hi[0] / w) - 1;
    int64_t j_lo = (int64_t)std::floor(domain.lo[1] / w), j_hi = (int64_t)std::ceil(domain.hi[1] / w) - 1;
    ASSERT(i_lo == i0 && j_lo == j0 && i_hi <= i0 + 1 && j_hi <= j0 + 1, "root block does not match the footprint");
    for (int64_t j = j_lo; j <= j_hi; ++j)
        for (int64_t i = i_lo; i <= i_hi; ++i)
            roots[n_roots++] = Cell{root_level, i, j};
    residual_tolerance = 1e-10 * mean_edge(tri);
}

namespace
{

// The record of one node test (T5): the decisions of the box and the slab
// with the walk's current maximum distance, and their interval lengths
// without it.
void record_node_test(const NodeBound &nb, TangentRay ray, std::vector<NodeTestRecord> &records)
{
    NodeTestRecord rec;
    rec.level = nb.level;
    double t0, t1;
    rec.box_pass = box_interval(nb, ray, t0, t1);
    rec.slab_pass = slab_interval(nb, ray, t0, t1);
    ray.t_max = INFINITY;
    rec.box_length = box_interval(nb, ray, t0, t1) ? t1 - t0 : -1.0;
    rec.slab_length = slab_interval(nb, ray, t0, t1) ? t1 - t0 : -1.0;
    rec.both_length = both_interval(nb, ray, t0, t1) ? t1 - t0 : -1.0;
    records.push_back(rec);
}

} // namespace

bool DisplacedIntersector::intersect(const vec3d &o, const vec3d &d, double t_min, double t_max,
                                     const TraversalOptions &options, DisplacedHit &hit, TraversalStats *stats) const
{
    TangentRay ray{frame.point(o), frame.direction(d), t_min, t_max};
    bool sign_i = ray.d[0] < 0.0, sign_j = ray.d[1] < 0.0;
    bool found = false;
    int64_t node_tests_after_hit = 0, leaf_tests_after_hit = 0; // since the latest hit
    for (int r = 0; r < n_roots; ++r) {
        Cell cur = roots[r], end = roots[r];
        cell_next(end, sign_i, sign_j, root_level);
        while (cur != end) {
            double w = pyramid->cell_width(cur.level);
            vec2d c((cur.i + 0.5) * w, (cur.j + 0.5) * w);
            if (classify_square(domain, c, 0.5 * w) == Overlap::Outside) {
                cell_next(cur, sign_i, sign_j, root_level);
                continue;
            }
            if (stats) {
                ++stats->node_tests;
                ++stats->node_tests_per_level[std::min(cur.level, TraversalStats::max_levels - 1)];
                ++node_tests_after_hit;
            }
            NodeBound nb = node_bound(*tri, frame, *pyramid, cur.level, cur.i, cur.j);
            ray.t_max = t_max;
            if (stats && stats->records)
                record_node_test(nb, ray, *stats->records);
            double t0, t1;
            if (!node_interval(nb, ray, options, t0, t1)) {
                cell_next(cur, sign_i, sign_j, root_level);
                continue;
            }
            if (cur.level > 0) {
                cell_down(cur, sign_i, sign_j);
                continue;
            }
            if (stats) {
                ++stats->leaf_tests;
                ++leaf_tests_after_hit;
            }
            DisplacedHit leaf_hit;
            bool leaf;
            if (options.leaf == LeafMode::Newton) {
                leaf =
                    intersect_newton(o, d, t_min, t_max, cur.i, cur.j, leaf_hit, stats ? &stats->sub_squares : nullptr);
            } else {
                leaf = intersect_two_triangle(o, d, t_min, t_max, cur.i, cur.j, leaf_hit);
            }
            if (leaf && leaf_hit.t < t_max) {
                hit = leaf_hit;
                t_max = hit.t;
                found = true;
                node_tests_after_hit = 0;
                leaf_tests_after_hit = 0;
            }
            cell_next(cur, sign_i, sign_j, root_level);
        }
    }
    if (stats && found) {
        stats->node_tests_after_hit += node_tests_after_hit;
        stats->leaf_tests_after_hit += leaf_tests_after_hit;
    }
    return found;
}

bool DisplacedIntersector::intersect_newton(const vec3d &o, const vec3d &d, double t_min, double t_max, int64_t i,
                                            int64_t j, DisplacedHit &hit, int64_t *sub_squares) const
{
    LeafBound leaf;
    leaf.w = pyramid->cell_width(0);
    leaf.lo = vec2d(i * leaf.w, j * leaf.w);
    leaf.h00 = field.h(leaf.lo[0], leaf.lo[1]);
    leaf.h10 = field.h(leaf.lo[0] + leaf.w, leaf.lo[1]);
    leaf.h01 = field.h(leaf.lo[0], leaf.lo[1] + leaf.w);
    leaf.h11 = field.h(leaf.lo[0] + leaf.w, leaf.lo[1] + leaf.w);
    vec2d c = leaf.lo + vec2d(0.5 * leaf.w, 0.5 * leaf.w);
    leaf.N_hat = AffineVec3(tri->m0 + c[0] * tri->Mu + c[1] * tri->Mv, 0.5 * leaf.w * tri->Mu, 0.5 * leaf.w * tri->Mv);
    leaf.N_hat.normalize();
    leaf.o = o;
    leaf.d = d;
    leaf.d_len = d.norm();
    leaf.dn = d / leaf.d_len;
    plane_basis(leaf.dn, leaf.d1, leaf.d2);

    bool found = false;
    int64_t count = 0;
    search_leaf(leaf, i, j, 0, 0.5, 0.5, 0.5, t_min, t_max, hit, found, count);
    if (sub_squares)
        *sub_squares += count;
    return found;
}

bool DisplacedIntersector::sub_square_bound(const LeafBound &leaf, double a, double b, double rho, double t_min,
                                            double t_max, double &t_lo) const
{
    vec2d c = leaf.lo + leaf.w * vec2d(a, b);
    if (classify_square(domain, c, leaf.w * rho) == Overlap::Outside)
        return false;
    // h = h00 + (h10 - h00) a + (h01 - h00) b + T a b over the texel: affine
    // over the sub-square up to the twist term T (rho eu)(rho ev).
    double T = leaf.h11 - leaf.h10 - leaf.h01 + leaf.h00;
    double ha = leaf.h10 - leaf.h00, hb = leaf.h01 - leaf.h00;
    AffineForm h(leaf.h00 + ha * a + hb * b + T * a * b, (ha + T * b) * rho, (hb + T * a) * rho,
                 std::abs(T) * rho * rho);
    AffineVec3 P(tri->P(c[0], c[1]), leaf.w * rho * tri->e1, leaf.w * rho * tri->e2);
    AffineVec3 S = P + h * restricted(leaf.N_hat, a, b, rho);
    AffineForm F1 = dot(leaf.d1, S), F2 = dot(leaf.d2, S);
    F1.c -= leaf.d1.dot(leaf.o);
    F2.c -= leaf.d2.dot(leaf.o);
    if (!F1.interval().contains(0.0) || !F2.interval().contains(0.0))
        return false;
    AffineForm along = dot(leaf.dn, S);
    along.c -= leaf.dn.dot(leaf.o);
    Interval range = along.interval();
    t_lo = range.lo / leaf.d_len;
    return t_lo < t_max && range.hi / leaf.d_len > t_min;
}

void DisplacedIntersector::search_leaf(const LeafBound &leaf, int64_t i, int64_t j, int depth, double a, double b,
                                       double rho, double t_min, double &t_max, DisplacedHit &hit, bool &found,
                                       int64_t &sub_squares) const
{
    if (depth == subdivision_depth) {
        DisplacedHit h;
        if (newton_from(leaf.o, leaf.d, t_min, t_max, i, j, leaf.lo + leaf.w * vec2d(a, b), h)) {
            hit = h;
            t_max = h.t;
            found = true;
        }
        return;
    }
    struct Child
    {
        double a, b, t_lo;
    } children[4];
    int n = 0;
    double r = 0.5 * rho;
    for (int q = 0; q < 4; ++q) {
        double ca = a + ((q & 1) ? r : -r), cb = b + ((q & 2) ? r : -r), t_lo;
        ++sub_squares;
        if (sub_square_bound(leaf, ca, cb, r, t_min, t_max, t_lo))
            children[n++] = {ca, cb, t_lo};
    }
    std::sort(children, children + n, [](const Child &x, const Child &y) { return x.t_lo < y.t_lo; });
    for (int q = 0; q < n; ++q) {
        if (children[q].t_lo >= t_max)
            break;
        search_leaf(leaf, i, j, depth + 1, children[q].a, children[q].b, r, t_min, t_max, hit, found, sub_squares);
    }
}

bool DisplacedIntersector::newton_from(const vec3d &o, const vec3d &d, double t_min, double t_max, int64_t i, int64_t j,
                                       vec2d guess, DisplacedHit &hit) const
{
    double w = pyramid->cell_width(0);
    vec2d lo(i * w, j * w), hi((i + 1) * w, (j + 1) * w);
    double d_len = d.norm();
    vec3d dn = d / d_len, d1, d2;
    plane_basis(dn, d1, d2);

    double prev_err2 = INFINITY;
    int err_streak = 0, behind_streak = 0, invalid_streak = 0;
    for (int iter = 0; iter < max_iterations; ++iter) {
        double s = guess[0], t = guess[1];
        NormalFrame f = normal_frame_at(*tri, s, t);
        double h = field.h(s, t);
        vec2d gh = field.grad(s, t);
        vec3d S = tri->P(s, t) + h * f.N;
        vec3d delta = S - o;
        vec2d F(delta.dot(d1), delta.dot(d2));
        double err2 = F.squaredNorm();
        double along = delta.dot(dn);
        err_streak = err2 > prev_err2 ? err_streak + 1 : 0;
        behind_streak = along < 0.0 ? behind_streak + 1 : 0;
        if (err_streak >= 2 || behind_streak >= 2)
            return false;
        prev_err2 = err2;

        vec3d Su = tri->e1 + gh[0] * f.N + h * f.Nu;
        vec3d Sv = tri->e2 + gh[1] * f.N + h * f.Nv;
        if (err2 < residual_tolerance * residual_tolerance) {
            if (along < 0.0 || !domain.inside(guess))
                return false;
            double t_hit = along / d_len;
            if (t_hit <= t_min || t_hit >= t_max)
                return false;
            hit.t = t_hit;
            hit.st = guess;
            hit.bary = tri->bary(s, t);
            hit.ng = Su.cross(Sv).normalized();
            hit.i = i;
            hit.j = j;
            return true;
        }

        // One Newton step on F(s, t); the 2 x 2 Jacobian is the projection of
        // (Su, Sv) onto the plane normal to the ray.
        double J00 = d1.dot(Su), J01 = d1.dot(Sv), J10 = d2.dot(Su), J11 = d2.dot(Sv);
        double det = J00 * J11 - J01 * J10;
        if (det == 0.0)
            return false;
        vec2d step((J11 * F[0] - J01 * F[1]) / det, (-J10 * F[0] + J00 * F[1]) / det);
        guess -= step;
        bool outside =
            guess[0] < lo[0] || guess[0] > hi[0] || guess[1] < lo[1] || guess[1] > hi[1] || !domain.inside(guess);
        if (outside) {
            if (++invalid_streak >= 3)
                return false;
            guess = guess.cwiseMax(lo).cwiseMin(hi);
        } else {
            invalid_streak = 0;
        }
    }
    return false;
}

bool DisplacedIntersector::intersect_two_triangle(const vec3d &o, const vec3d &d, double t_min, double t_max, int64_t i,
                                                  int64_t j, DisplacedHit &hit) const
{
    double w = pyramid->cell_width(0);
    vec2d st00(i * w, j * w), st10((i + 1) * w, j * w), st01(i * w, (j + 1) * w), st11((i + 1) * w, (j + 1) * w);
    vec3d S00 = displaced_position(*tri, field, st00[0], st00[1]);
    vec3d S10 = displaced_position(*tri, field, st10[0], st10[1]);
    vec3d S01 = displaced_position(*tri, field, st01[0], st01[1]);
    vec3d S11 = displaced_position(*tri, field, st11[0], st11[1]);

    // The faces of tessellation_grid: (i, j), (i+1, j), (i, j+1) and
    // (i+1, j), (i+1, j+1), (i, j+1); both orient like Su x Sv.
    const vec3d *A[2] = {&S00, &S10};
    const vec3d *B[2] = {&S10, &S11};
    const vec3d *C[2] = {&S01, &S01};
    const vec2d *a[2] = {&st00, &st10};
    const vec2d *b[2] = {&st10, &st11};
    const vec2d *c[2] = {&st01, &st01};
    bool found = false;
    for (int k = 0; k < 2; ++k) {
        double t, b1, b2;
        if (!ray_triangle(o, d, t_min, t_max, *A[k], *B[k], *C[k], t, b1, b2))
            continue;
        vec2d st = (1.0 - b1 - b2) * *a[k] + b1 * *b[k] + b2 * *c[k];
        if (!domain.inside(st))
            continue;
        hit.t = t;
        hit.st = st;
        hit.bary = tri->bary(st[0], st[1]);
        hit.ng = (*B[k] - *A[k]).cross(*C[k] - *A[k]).normalized();
        hit.i = i;
        hit.j = j;
        t_max = t;
        found = true;
    }
    return found;
}

vec3d DisplacedIntersector::two_triangle_position(double s, double t) const
{
    double w = pyramid->cell_width(0);
    int64_t i = (int64_t)std::floor(s / w), j = (int64_t)std::floor(t / w);
    double a = s / w - i, b = t / w - j;
    vec3d S00 = displaced_position(*tri, field, i * w, j * w);
    vec3d S10 = displaced_position(*tri, field, (i + 1) * w, j * w);
    vec3d S01 = displaced_position(*tri, field, i * w, (j + 1) * w);
    vec3d S11 = displaced_position(*tri, field, (i + 1) * w, (j + 1) * w);
    if (a + b <= 1.0)
        return S00 + a * (S10 - S00) + b * (S01 - S00);
    return S11 + (1.0 - a) * (S01 - S11) + (1.0 - b) * (S10 - S11);
}

float float_below(double x)
{
    float f = (float)x;
    return (double)f <= x ? f : std::nextafter(f, -INFINITY);
}

float float_above(double x)
{
    float f = (float)x;
    return (double)f >= x ? f : std::nextafter(f, INFINITY);
}

void rtc_bounds(const Aabb3 &b, RTCBounds &out)
{
    double pad = 8.0 * std::numeric_limits<float>::epsilon() *
                 (1.0 + std::max(b.lo.cwiseAbs().maxCoeff(), b.hi.cwiseAbs().maxCoeff()));
    out.lower_x = float_below(b.lo[0] - pad);
    out.lower_y = float_below(b.lo[1] - pad);
    out.lower_z = float_below(b.lo[2] - pad);
    out.upper_x = float_above(b.hi[0] + pad);
    out.upper_y = float_above(b.hi[1] + pad);
    out.upper_z = float_above(b.hi[2] + pad);
}

ObjectIntersector::ObjectIntersector(const std::vector<BaseTriangle> &tris, const TaylorPyramid &pyramid,
                                     const HeightGrid &field)
{
    triangles.reserve(tris.size());
    boxes.reserve(tris.size());
    bound = Aabb3{vec3d::Constant(INFINITY), vec3d::Constant(-INFINITY)};
    for (const BaseTriangle &tri : tris) {
        triangles.emplace_back(tri, pyramid, field);
        double h_min, h_max;
        triangle_height_range(tri, pyramid, h_min, h_max);
        boxes.push_back(triangle_aabb(tri, h_min, h_max));
        bound.expand(boxes.back());
    }
}

namespace
{

// The query of ObjectIntersector::intersect travels in the intersect
// context, which embree hands back to the callbacks; the context comes
// first so that the two pointers agree.
struct ObjectQuery
{
    RTCIntersectContext context;
    const ObjectIntersector *object = nullptr;
    vec3d o, d;
    double t_min = 0.0, t_max = INFINITY;
    const TraversalOptions *options = nullptr;
    DisplacedHit *hit = nullptr;
    int triangle = -1;
    TraversalStats *stats = nullptr;
    bool found = false;
};

// The box of one triangle, widened for the float ray.
void triangle_bounds(const RTCBoundsFunctionArguments *args)
{
    const ObjectIntersector *object = (const ObjectIntersector *)args->geometryUserPtr;
    rtc_bounds(object->boxes[args->primID], *args->bounds_o);
}

void triangle_intersect(const RTCIntersectFunctionNArguments *args)
{
    if (!args->valid[0])
        return;
    ObjectQuery *q = (ObjectQuery *)args->context;
    RTCRayHit *rh = (RTCRayHit *)args->rayhit;
    int k = (int)args->primID;
    if (q->stats)
        ++q->stats->triangle_tests;
    DisplacedHit h;
    if (!q->object->triangles[k].intersect(q->o, q->d, q->t_min, q->t_max, *q->options, h, q->stats))
        return;
    *q->hit = h;
    q->triangle = k;
    q->t_max = h.t;
    q->found = true;
    rh->ray.tfar = float_above(h.t);
    rh->hit.geomID = args->geomID;
    rh->hit.primID = (unsigned)k;
    rh->hit.u = rh->hit.v = 0.0f;
    rh->hit.Ng_x = (float)h.ng[0];
    rh->hit.Ng_y = (float)h.ng[1];
    rh->hit.Ng_z = (float)h.ng[2];
}

} // namespace

ObjectIntersector::~ObjectIntersector()
{
    if (scene)
        rtcReleaseScene(scene);
}

void ObjectIntersector::build_bvh(RTCDevice device)
{
    ASSERT(!scene, "the BVH is already built");
    RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_USER);
    rtcSetGeometryUserPrimitiveCount(geom, (unsigned)triangles.size());
    rtcSetGeometryUserData(geom, this);
    rtcSetGeometryBoundsFunction(geom, triangle_bounds, this);
    rtcSetGeometryIntersectFunction(geom, triangle_intersect);
    rtcCommitGeometry(geom);
    scene = rtcNewScene(device);
    rtcSetSceneFlags(scene, RTC_SCENE_FLAG_ROBUST);
    rtcAttachGeometry(scene, geom);
    rtcReleaseGeometry(geom);
    rtcCommitScene(scene);
}

bool ObjectIntersector::intersect(const vec3d &o, const vec3d &d, double t_min, double t_max,
                                  const TraversalOptions &options, DisplacedHit &hit, int &triangle,
                                  TraversalStats *stats) const
{
    if (!scene)
        return intersect_flat(o, d, t_min, t_max, options, hit, triangle, stats);
    ObjectQuery q;
    rtcInitIntersectContext(&q.context);
    q.object = this;
    q.o = o;
    q.d = d;
    q.t_min = t_min;
    q.t_max = t_max;
    q.options = &options;
    q.hit = &hit;
    q.stats = stats;
    RTCRayHit rh = ks::spawn_rtcrayhit(o.cast<float>(), d.cast<float>(), float_below(t_min), float_above(t_max));
    rtcIntersect1(scene, &q.context, &rh);
    triangle = q.triangle;
    return q.found;
}

bool ObjectIntersector::intersect_flat(const vec3d &o, const vec3d &d, double t_min, double t_max,
                                       const TraversalOptions &options, DisplacedHit &hit, int &triangle,
                                       TraversalStats *stats) const
{
    bool found = false;
    triangle = -1;
    for (size_t k = 0; k < triangles.size(); ++k) {
        double t0 = t_min, t1 = t_max;
        if (!boxes[k].clip(o, d, t0, t1))
            continue;
        if (stats)
            ++stats->triangle_tests;
        DisplacedHit h;
        if (triangles[k].intersect(o, d, t_min, t_max, options, h, stats)) {
            hit = h;
            triangle = (int)k;
            t_max = h.t;
            found = true;
        }
    }
    return found;
}

} // namespace dmap
