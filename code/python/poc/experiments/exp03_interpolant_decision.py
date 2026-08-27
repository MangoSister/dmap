"""Experiment 3 — Bilinear or B-spline? (Master plan §4, interpolant row.)

The operator applications prefer C1; the question is whether bilinear's
gradient discontinuities at texel edges measurably hurt a toy solve.

Setup: one real base triangle, one real displacement texture, both
interpolants. For each lattice resolution n, solve geodesic distance with
the mini heat method on the texel-lattice cotan Laplacian (Path B: per-face
metric from the formula at centroids). Ground truth per interpolant: the
same heat method on the 4x-finer piecewise-linear tessellation of that
interpolant's own surface (Path A: micro-face Gram metrics), restricted to
the coarse nodes. Also count the two Path-B hazards (triangle-inequality
failures, negative cotan weights) per interpolant.

Decision criterion (from the plan): pick B-spline if bilinear shows
measurably worse error convergence or materially more invalid cells; else
bilinear (matches TFDM).
"""

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from _common import DATA_DIR, OUT_DIR, verdict
from dmapref.displacement import downsample_box, load_texture
from dmapref.heat_method import solve_heat_method
from dmapref.interpolant import BilinearInterpolant, BSplineInterpolant
from dmapref.mesh import load_obj
from dmapref.metric import metric_at
from dmapref.reference_surface import (grid_index, tessellation_grid,
                                       tessellation_metrics)

TEX_NODES = 33
LATTICES = [8, 16, 32]
REFINE = 4                          # ground truth at 4x resolution
TRIANGLE_INDICES = [137, 2200, 11083]  # fixed, arbitrary cc_torus triangles


def formula_metrics_per_face(tri, field, n):
    """Path B: per-face metric from the master formula at face centroids."""
    params, faces = tessellation_grid(n)
    G_faces = np.empty((len(faces), 2, 2))
    for t, (A, B, C) in enumerate(faces):
        u, v = (params[A] + params[B] + params[C]) / 3.0
        G_faces[t] = metric_at(tri, field, u, v)
    return params, faces, G_faces


def ground_truth_distance(tri, field, n_coarse):
    """Path A heat method on the 4x tessellated surface, restricted to the
    coarse lattice nodes."""
    n_fine = REFINE * n_coarse
    params, faces = tessellation_grid(n_fine)
    G_pl, _ = tessellation_metrics(tri, field, n_fine)
    source = grid_index(n_fine, 0, 0)
    phi_fine, _ = solve_heat_method(params, faces, G_pl, source)

    coarse_nodes = [grid_index(n_fine, REFINE * i, REFINE * j)
                    for j in range(n_coarse + 1) for i in range(n_coarse + 1 - j)]
    return phi_fine[coarse_nodes]


def run_triangle(tri, tex):
    edge = (np.linalg.norm(tri.e1) + np.linalg.norm(tri.e2)
            + np.linalg.norm(tri.e2 - tri.e1)) / 3.0
    interpolants = {
        "bilinear": BilinearInterpolant(tex, scale=0.2 * edge),
        "B-spline": BSplineInterpolant(tex, scale=0.2 * edge),
    }

    errors = {name: [] for name in interpolants}
    hazards = {name: [] for name in interpolants}
    for name, field in interpolants.items():
        print(f"  {name}:")
        for n in LATTICES:
            params, faces, G_faces = formula_metrics_per_face(tri, field, n)
            source = grid_index(n, 0, 0)
            phi, stats = solve_heat_method(params, faces, G_faces, source)
            phi_ref = ground_truth_distance(tri, field, n)

            mask = np.arange(len(phi)) != source
            rel = (np.linalg.norm(phi[mask] - phi_ref[mask])
                   / np.linalg.norm(phi_ref[mask]))
            errors[name].append(rel)
            hazards[name].append(stats)
            print(f"    n = {n:3d}   distance error (rel L2) = {rel:.3e}   "
                  f"invalid = {stats['invalid_triangles']}   "
                  f"negative weights = {stats['negative_weights']}")
    return errors, hazards


def main():
    mesh = load_obj(DATA_DIR / "cc_torus.obj")
    tex = downsample_box(load_texture(DATA_DIR / "disp_rock.png"), TEX_NODES)

    all_errors = {"bilinear": [], "B-spline": []}
    total_invalid = {"bilinear": 0, "B-spline": 0}
    for t in TRIANGLE_INDICES:
        print(f"\ntriangle {t}:")
        errors, hazards = run_triangle(mesh.triangle(t), tex)
        for name in all_errors:
            all_errors[name].append(errors[name])
            total_invalid[name] += sum(s["invalid_triangles"] for s in hazards[name])

    e_bl = np.array(all_errors["bilinear"])   # (triangles, lattices)
    e_bs = np.array(all_errors["B-spline"])

    fig, ax = plt.subplots(figsize=(5, 4))
    for name, e in (("bilinear", e_bl), ("B-spline", e_bs)):
        ax.loglog(LATTICES, e.mean(axis=0), "o-", label=f"{name} (mean of "
                  f"{len(TRIANGLE_INDICES)} triangles)")
    ax.set_xlabel("lattice resolution n")
    ax.set_ylabel("geodesic distance error (relative L2)")
    ax.set_title("Interpolant decision: toy heat-method solve (Path B)")
    ax.legend()
    fig.tight_layout()
    fig.savefig(OUT_DIR / "exp03_interpolant_decision.png", dpi=150)

    # Decision: B-spline only if bilinear is measurably worse.
    inv_bl, inv_bs = total_invalid["bilinear"], total_invalid["B-spline"]
    bilinear_worse = bool(np.any(e_bl > 2.0 * e_bs)) or inv_bl > 2 * inv_bs + 4
    decision = "B-spline" if bilinear_worse else "bilinear"

    print(f"\nDecision evidence: mean errors per lattice "
          f"bilinear {np.round(e_bl.mean(axis=0), 4)}, "
          f"B-spline {np.round(e_bs.mean(axis=0), 4)}; "
          f"invalid cells {inv_bl} vs {inv_bs}")
    verdict(True, f"interpolant decision: {decision} "
                  f"({'bilinear measurably worse' if bilinear_worse else 'no measurable penalty for bilinear; matches TFDM'})")


if __name__ == "__main__":
    main()
