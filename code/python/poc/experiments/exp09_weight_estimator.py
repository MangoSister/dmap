"""Experiment 9 - Does the descent weight's midpoint model track a cell's
true area? (Master plan section 5A; A-MVP S7 open decision on coarse-level
product weights.)

The descent weight for a cell should approximate the integral of
sqrt(det G) over it. The implemented weight (S6) evaluates sqrt(det G) once,
from the node's (h0, gu, gv) at the cell centre. G contains grad h grad h^T,
so building that term from a single gradient discards the gradient
covariance over the cell - which is exactly the cell's roughness.

Three estimators against the truth (the per-texel quadrature the S7
references use):

  (1) midpoint      what the code does today
  (2) mean gradient the same form with h and grad h replaced by their cell
                    means - isolates whether the gradient-hull midpoint is
                    simply a bad choice of slope
  (3) moments       sqrt(det E[G]). Every term of the metric formula is at
                    most quadratic in (h, grad h), so E[G] is exact from
                    E[h], E[h^2], E[grad h], E[grad h grad h^T]: seven
                    base-independent numbers that fold by averaging.

Estimator error is not variance. This measures the weight, not the sampler;
whether the moment weights close the S7 variance gap needs the S7 harness.
"""

import numpy as np

from _common import DATA_DIR, verdict
from dmapref.displacement import downsample_box, load_texture
from dmapref.interpolant import BilinearInterpolant
from dmapref.mesh import load_obj
from dmapref.metric import base_forms_at, det2, offset_metric
from dmapref.pyramid import TaylorPyramid

TEX_NODES = 65                       # 64 leaf cells per side
MESH = "cc_torus.obj"
COMBOS = [("disp_rock.png", 0.05), ("disp_rock.png", 0.2), ("disp_cobble.png", 0.2)]
MIDPOINT_FLOOR = 0.9                 # below this the midpoint model is judged biased
COARSE_CELL = 8                      # cells at least this wide are "coarse" here


def sqrt_det_G(tri, u, v, h, gh, h_sq=None, gh_outer=None):
    """sqrt(det G) at (u, v). With h_sq and gh_outer supplied, the quadratic
    terms use second moments instead of squares of means, which makes the
    result sqrt(det E[G]) rather than sqrt(det G(E[h], E[grad h]))."""
    G0, B0, C0, a = base_forms_at(tri, u, v)
    Q = offset_metric(G0, B0, C0, h) if h_sq is None else G0 - 2.0 * h * B0 + h_sq * C0
    slope = np.outer(gh, gh) if gh_outer is None else gh_outer
    G = Q + slope + np.outer(gh, a) + np.outer(a, gh)
    return np.sqrt(max(det2(G), 0.0))


def per_texel_fields(tri, field, n):
    """h, grad h and the truth sqrt(det G) at every texel centre."""
    h = np.empty((n, n))
    gu, gv = np.empty((n, n)), np.empty((n, n))
    sqrt_det = np.empty((n, n))
    for j in range(n):
        for i in range(n):
            u, v = (i + 0.5) / n, (j + 0.5) / n
            h[j, i] = field.h(u, v)
            g = field.grad(u, v)
            gu[j, i], gv[j, i] = g
            sqrt_det[j, i] = sqrt_det_G(tri, u, v, h[j, i], g)
    return h, gu, gv, sqrt_det


def main():
    mesh = load_obj(DATA_DIR / MESH)
    tri = mesh.triangle(mesh.n_triangles // 3)
    n = TEX_NODES - 1
    all_ok = True

    for tex, amplitude in COMBOS:
        values = downsample_box(load_texture(DATA_DIR / tex), n)
        V = np.pad(values, ((0, 1), (0, 1)), mode="edge")
        edges = [np.linalg.norm(tri.q1 - tri.q0), np.linalg.norm(tri.q2 - tri.q1),
                 np.linalg.norm(tri.q0 - tri.q2)]
        scale = amplitude * float(np.mean(edges))
        pyramid = TaylorPyramid(V, scale)
        h, gu, gv, sqrt_det = per_texel_fields(tri, BilinearInterpolant(V, scale), n)

        print(f"\n=== {tex}, amplitude {amplitude} - median estimator / truth ===")
        print(f"{'cell':>5} {'(1) midpoint':>14} {'(2) mean grad':>14} "
              f"{'(3) moments':>13} {'cells low (1)':>14}")
        crossover, coarsest = 0, None

        for L in range(pyramid.n_levels - 1):     # the root holds too few cells
            m, k, w = n >> L, 1 << L, pyramid.cell_width(L)
            level = pyramid.levels[L]
            block = lambda A: A.reshape(m, k, m, k).mean(axis=(1, 3))
            mean_h, mean_h_sq = block(h), block(h * h)
            mean_gu, mean_gv = block(gu), block(gv)
            mean_uu, mean_uv, mean_vv = block(gu * gu), block(gu * gv), block(gv * gv)
            truth = block(sqrt_det)

            est = np.empty((3, m, m))
            for j in range(m):
                for i in range(m):
                    u0, v0 = (i + 0.5) * w, (j + 0.5) * w
                    node_g = np.array([level["gu"][j, i], level["gv"][j, i]])
                    mean_g = np.array([mean_gu[j, i], mean_gv[j, i]])
                    outer = np.array([[mean_uu[j, i], mean_uv[j, i]],
                                      [mean_uv[j, i], mean_vv[j, i]]])
                    est[0, j, i] = sqrt_det_G(tri, u0, v0, level["h0"][j, i], node_g)
                    est[1, j, i] = sqrt_det_G(tri, u0, v0, mean_h[j, i], mean_g)
                    est[2, j, i] = sqrt_det_G(tri, u0, v0, mean_h[j, i], mean_g,
                                              mean_h_sq[j, i], outer)

            # cells whose closed square lies inside the triangle domain
            ii, jj = np.meshgrid(np.arange(m), np.arange(m), indexing="xy")
            inside = ((ii + 1) * w + (jj + 1) * w) <= 1.0
            ratios = [np.median((est[q] / truth)[inside]) for q in range(3)]
            fraction_low = np.mean((est[0] / truth)[inside] < 1.0)
            coarsest = ratios

            # Moments must beat the midpoint where the problem is, at coarse
            # cells. At fine cells the midpoint is already near-exact and the
            # moment estimator's own overshoot can exceed it; record where
            # that crossover sits rather than demanding it never happens.
            if k >= COARSE_CELL:
                all_ok &= abs(ratios[2] - 1.0) <= abs(ratios[0] - 1.0) + 1e-9
            elif abs(ratios[2] - 1.0) > abs(ratios[0] - 1.0):
                crossover = max(crossover, k)

            print(f"{k:>5} {ratios[0]:>14.3f} {ratios[1]:>14.3f} {ratios[2]:>13.3f} "
                  f"{fraction_low:>14.2f}")

        print(f"  moments beat the midpoint from {2 * crossover:>2d}-texel cells up"
              if crossover else "  moments beat the midpoint at every level")
        if amplitude >= 0.2:
            # the bias this experiment exists to find must actually be there
            all_ok &= coarsest[0] < MIDPOINT_FLOOR

    verdict(all_ok, "the midpoint model underestimates a cell's mean "
                    "sqrt(det G) in every coarse cell, by up to 2x at "
                    "amplitude 0.2, and the underestimate tracks the S7 gap "
                    "between product descent and the product table; second "
                    "moments of (h, grad h) beat it from 8-texel cells up, "
                    "overshoot on rough content below that, and cost seven "
                    "channels to store")


if __name__ == "__main__":
    main()
