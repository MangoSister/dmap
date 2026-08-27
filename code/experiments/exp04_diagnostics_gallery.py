"""Experiment 4 — First look at the pre-bake diagnostics (application D).

Obliquity sin^2(theta) and the normalized integrability defect delta over
every base triangle of two assets, at the barycenter. No baseline yet; this
is a smoke test of the machinery and a first look at real value ranges.
"""

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from _common import DATA_DIR, OUT_DIR, verdict
from dmapref.diagnostics import mesh_diagnostics
from dmapref.mesh import load_obj


def summarize(name, values):
    q = np.percentile(values, [50, 90, 99])
    print(f"  {name}: median {q[0]:.3e}, p90 {q[1]:.3e}, "
          f"p99 {q[2]:.3e}, max {values.max():.3e}")
    return q


def run_asset(name, mesh_path, axes_row):
    mesh = load_obj(mesh_path)
    print(f"\n{name}: {mesh.n_triangles} triangles")
    sin2, delta = mesh_diagnostics(mesh)
    summarize("sin^2(theta)", sin2)
    summarize("|delta| (normalized)", np.abs(delta))

    ax_hist, ax_scatter = axes_row
    ax_hist.hist(sin2, bins=60, log=True, color="tab:blue")
    ax_hist.set_xlabel("sin^2(theta)")
    ax_hist.set_title(f"{name}: obliquity")

    barycenters = np.array([mesh.triangle(t).P(1 / 3, 1 / 3)
                            for t in range(mesh.n_triangles)])
    sc = ax_scatter.scatter(barycenters[:, 0], barycenters[:, 2],
                            c=sin2, s=1.5, cmap="viridis")
    ax_scatter.set_aspect("equal")
    ax_scatter.set_title(f"{name}: sin^2(theta) map (xz view)")
    plt.colorbar(sc, ax=ax_scatter, shrink=0.8)
    return sin2, delta


def main():
    fig, axes = plt.subplots(2, 2, figsize=(11, 9))
    ok = True
    for row, (name, path) in enumerate([("cc_torus", DATA_DIR / "cc_torus.obj"),
                                        ("spot", DATA_DIR / "spot.obj")]):
        sin2, delta = run_asset(name, path, axes[row])
        ok &= bool((sin2 >= 0).all() and (sin2 <= 1).all() and np.isfinite(delta).all())

    fig.tight_layout()
    fig.savefig(OUT_DIR / "exp04_diagnostics_gallery.png", dpi=150)
    verdict(ok, "diagnostics computed over both assets; "
                "figure in out/exp04_diagnostics_gallery.png")


if __name__ == "__main__":
    main()
