"""Experiment 2 — Does the formula describe the rendered surface, and how
far apart are the two at leaf scale?

The formula gives the metric of the smooth interpolated surface. A renderer
draws piecewise-linear micro-triangles. This experiment measures the gap:
per-micro-face Gram matrices (Path A) against the formula at face centroids,
across tessellation levels, on real base meshes with real displacement
textures.

The number at the level where micro-faces match texel size IS the master
plan's limitation 2 ("the metric is that of the smooth interpolated
surface") — report it explicitly.
"""

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from _common import DATA_DIR, OUT_DIR, verdict
from dmapref.displacement import downsample_box, load_texture
from dmapref.interpolant import BilinearInterpolant
from dmapref.mesh import load_obj
from dmapref.metric import det2, metric_at
from dmapref.reference_surface import tessellation_metrics

TEX_NODES = 33          # 33x33 nodes -> 32x32 bilinear cells
LEVELS = [4, 8, 16, 32, 64]
LEAF_LEVEL = 32         # micro-face size == texel size
N_TRIANGLES = 8
AMPLITUDES = [0.05, 0.2]  # displacement scale as a fraction of mean edge length


def mean_edge_length(tri):
    return (np.linalg.norm(tri.e1) + np.linalg.norm(tri.e2)
            + np.linalg.norm(tri.e2 - tri.e1)) / 3.0


def triangle_errors(tri, field, n):
    """Mean/max relative Frobenius metric error and area-element error of
    the piecewise-linear surface at tessellation level n."""
    G_pl, centroids = tessellation_metrics(tri, field, n)
    frob, area = [], []
    for G_lin, (u, v) in zip(G_pl, centroids):
        G = metric_at(tri, field, u, v)
        frob.append(np.linalg.norm(G_lin - G) / np.linalg.norm(G))
        area.append(abs(np.sqrt(det2(G_lin)) - np.sqrt(det2(G)))
                    / np.sqrt(det2(G)))
    return np.array(frob), np.array(area)


def run_asset(name, mesh_path, tex_path, amplitude, rng):
    mesh = load_obj(mesh_path)
    tex = downsample_box(load_texture(tex_path), TEX_NODES)
    picks = rng.choice(mesh.n_triangles, size=N_TRIANGLES, replace=False)
    print(f"\n{name}, amplitude {amplitude} x edge "
          f"({mesh.n_triangles} triangles, sampling {[int(t) for t in picks]}):")

    mean_area_err = {n: [] for n in LEVELS}
    max_area_err = {n: [] for n in LEVELS}
    for t in picks:
        tri = mesh.triangle(t)
        field = BilinearInterpolant(tex, scale=amplitude * mean_edge_length(tri))
        for n in LEVELS:
            _, area = triangle_errors(tri, field, n)
            mean_area_err[n].append(area.mean())
            max_area_err[n].append(area.max())

    means = np.array([np.mean(mean_area_err[n]) for n in LEVELS])
    maxes = np.array([np.max(max_area_err[n]) for n in LEVELS])
    for n, m, x in zip(LEVELS, means, maxes):
        marker = "   <- leaf scale (micro-face == texel)" if n == LEAF_LEVEL else ""
        print(f"  n = {n:3d}   area error mean = {m:.2e}   max = {x:.2e}{marker}")
    return means, maxes


def main():
    assets = [
        ("cc_torus + disp_rock", DATA_DIR / "cc_torus.obj", DATA_DIR / "disp_rock.png"),
        ("spot + disp_cobble", DATA_DIR / "spot.obj", DATA_DIR / "disp_cobble.png"),
    ]

    fig, ax = plt.subplots(figsize=(6, 4.5))
    all_monotone, leaf_numbers = True, {}
    for name, mesh_path, tex_path in assets:
        for amplitude in AMPLITUDES:
            # Same seed per run: identical triangle picks across amplitudes.
            rng = np.random.default_rng(42)
            means, maxes = run_asset(name, mesh_path, tex_path, amplitude, rng)
            all_monotone &= bool(np.all(np.diff(means) < 0))
            leaf_numbers[f"{name} @ {amplitude}"] = (
                means[LEVELS.index(LEAF_LEVEL)], maxes[LEVELS.index(LEAF_LEVEL)])
            ax.loglog(LEVELS, means, "o-", label=f"{name} @ {amplitude} (mean)")

    ax.set_xlabel("tessellation level n (micro-edges per base edge)")
    ax.set_ylabel("relative area-element error |dA_pl - dA| / dA")
    ax.set_title("Smooth-formula metric vs tessellated (rendered) surface")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT_DIR / "exp02_metric_vs_tessellation.png", dpi=150)

    print("\nLimitation-2 numbers (leaf scale, micro-face == texel):")
    for key, (m, x) in leaf_numbers.items():
        print(f"  {key}: mean {m:.2e}, max {x:.2e}")

    # The purpose is to PRICE the discrepancy (master plan, limitation 2),
    # not to pass a threshold: the check is that the tessellated surface
    # converges to the formula, which is what makes the price meaningful.
    verdict(all_monotone,
            "tessellation error decreases monotonically toward the formula; "
            "leaf-scale numbers above are the limitation-2 price")


if __name__ == "__main__":
    main()
