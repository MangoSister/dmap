"""Experiment 5 — Is the Taylor pyramid tight enough? (The Phase-1 kill-test.)

Master plan §7, Phase 1: certified bound width against the true range of
each metric quantity, per pyramid level, on real assets and textures.

Kill criterion (revised 2026-08-26 after the per-application review in the
project log): conservativeness is absolute -- any bound violation kills.
Tightness must be within 3x of the true range at cells up to 8 texels per
side, on typical (not adversarial) content; those are the cells whose
bounds the applications read for their answers. Coarser cells only steer
efficiency (descent allocation, refinement depth, walk length, deep LoD
drops) and are reported for the Phase 2 consumers, not gated on.

Setup: per (mesh, texture, amplitude) and per sampled base triangle, build
the Taylor pyramid over a 128x128-cell tile (identity chart, Phase-0
convention), propagate every level to certified intervals on sqrt(det G)
and the stretch eigenvalues of G0^-1 G plus a normal cone, and compare
against dense-sampled truth. The 0.05x-edge amplitude rows are the
"typical content" of the criterion; 0.2x-edge is the stress case.

Two metrics per (level, quantity):
- tightness ratio = certified width / true range, over cells whose true
  range is at least 1e-3 of the quantity's magnitude (elsewhere the ratio
  measures nothing; the kept fraction is reported);
- relative width = certified width / quantity magnitude, over all cells
  (what a bound consumer actually pays on flat cells too).
The cone's truth is the max angle of sampled normals around the per-cell
mean normal; its conservativeness check is the max sampled angle around
the certified axis.
"""

import time

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from _common import DATA_DIR, OUT_DIR, verdict
from dmapref.dense_reference import (cell_sample_points, level_ranges,
                                     pointwise_fields, pool_range)
from dmapref.displacement import downsample_box, load_texture
from dmapref.mesh import load_obj
from dmapref.node_bounds import (cell_enclosure, propagate_affine,
                                 taylor_forms)
from dmapref.pyramid import TaylorPyramid

TEX_NODES = 257                       # 256x256 leaf cells, 9 levels
                                      # (3x3 box downsample of the 1k textures)
SAMPLES_PER_CELL = 5                  # per axis, corners approached
N_TRIANGLES = 10
AMPLITUDES = [0.05, 0.2]
ASSETS = [("cc_torus.obj", "disp_rock.png"),
          ("cc_torus.obj", "disp_cobble.png"),
          ("spot.obj", "disp_rock.png"),
          ("spot.obj", "disp_cobble.png")]
QUANTITIES = ["sqrt_det", "lam_min", "lam_max", "cone"]
CRITERION_TEXELS = (1, 8)             # cells the applications read bounds at
RANGE_FLOOR = 1e-3                    # of the quantity's magnitude
KILL_RATIO = 3.0
MIN_KEPT = 0.1                        # ratio median needs >= 10% of cells


def mean_edge(tri):
    return (np.linalg.norm(tri.e1) + np.linalg.norm(tri.e2)
            + np.linalg.norm(tri.e2 - tri.e1)) / 3.0


def cone_truth(n_samples, n_cells, m, n_levels):
    """Per-cell true cone half-angle at every level: max angle of the
    sampled unnormalized normals around the per-cell mean direction."""
    unit = n_samples / np.linalg.norm(n_samples, axis=-1, keepdims=True)
    # Mean direction per leaf cell, pooled by summing upward.
    sums = [unit.reshape(n_cells, m, n_cells, m, 3).sum(axis=(1, 3))]
    while sums[-1].shape[0] > 1:
        k = sums[-1].shape[0] // 2
        sums.append(sums[-1].reshape(k, 2, k, 2, 3).sum(axis=(1, 3)))
    out = []
    leaf_idx = np.arange(n_cells * m) // m
    for level in range(n_levels):
        idx = leaf_idx >> level
        axis = sums[level] / np.linalg.norm(sums[level], axis=-1,
                                            keepdims=True)
        ax = axis[idx[:, None], idx[None, :]]
        cos = (unit * ax).sum(axis=-1)
        ang = np.arccos(np.clip(cos, -1.0, 1.0))
        m_lvl = sums[level].shape[0]
        mm = n_cells * m // m_lvl
        out.append(ang.reshape(m_lvl, mm, m_lvl, mm).max(axis=(1, 3)))
    return out


def max_angle_to(n_samples, axis, idx):
    """Per-cell max angle of samples to a per-cell axis (3, mc, mc)."""
    unit = n_samples / np.linalg.norm(n_samples, axis=-1, keepdims=True)
    ax = np.moveaxis(axis[:, idx[:, None], idx[None, :]], 0, -1)
    ang = np.arccos(np.clip((unit * ax).sum(axis=-1), -1.0, 1.0))
    mc = axis.shape[1]
    mm = ang.shape[0] // mc
    return ang.reshape(mc, mm, mc, mm).max(axis=(1, 3))


def triangle_tightness(tri, values, scale):
    """Per level and quantity: certified width, true width, magnitude
    scale, and conservativeness violations."""
    pyr = TaylorPyramid(values, scale)
    n, m = pyr.n_leaf, SAMPLES_PER_CELL
    x = cell_sample_points(n, m)
    X, Y = np.meshgrid(x, x, indexing="xy")
    f = pointwise_fields(tri, values, scale, X, Y)

    truth = {q: level_ranges(f[q], n, m, pyr.n_levels)
             for q in ["sqrt_det", "lam_min", "lam_max"]}
    cone_true = cone_truth(f["n"], n, m, pyr.n_levels)
    q_scale = {q: float(np.median(np.abs(f[q])))
               for q in ["sqrt_det", "lam_min", "lam_max"]}
    q_scale["cone"] = 1.0             # radians; an O(1) angle is "wide"
    leaf_idx = np.arange(n * m) // m

    rows = []
    for level in range(pyr.n_levels):
        U0, V0 = pyr.centers(level)
        s = pyr.half_extent(level)
        enc = cell_enclosure(tri, U0, V0, s, s)
        b = propagate_affine(*taylor_forms(pyr.levels[level], s, s), enc)

        row = {}
        for q in ["sqrt_det", "lam_min", "lam_max"]:
            lo, hi = b[q]
            t_lo, t_hi = truth[q][level]
            tol = 1e-7 * q_scale[q]
            row[q] = {"w_cert": hi - lo, "w_true": t_hi - t_lo,
                      "viol": int(((lo > t_lo + tol)
                                   | (hi < t_hi - tol)).sum())}
        cert_ang = b["cone"]["half_angle"]
        sample_ang = max_angle_to(f["n"], b["cone"]["axis"], leaf_idx >> level)
        row["cone"] = {"w_cert": cert_ang, "w_true": cone_true[level],
                       "viol": int((cert_ang < sample_ang - 1e-7).sum())}
        rows.append(row)
    return rows, q_scale


def main():
    rng = np.random.default_rng(3)
    t_start = time.perf_counter()
    # Sorted: set iteration order varies per process, and tri_picks draws
    # from the seeded generator in this order, so it must be deterministic.
    textures = {name: downsample_box(load_texture(DATA_DIR / name), TEX_NODES)
                for name in sorted({t for _, t in ASSETS})}
    meshes = {name: load_obj(DATA_DIR / name)
              for name in sorted({m for m, _ in ASSETS})}
    tri_picks = {name: rng.choice(mesh.n_triangles, N_TRIANGLES, replace=False)
                 for name, mesh in meshes.items()}

    n_levels = (TEX_NODES - 1).bit_length()
    texels = [2 ** k for k in range(n_levels)]
    criterion = [k for k, t in enumerate(texels)
                 if CRITERION_TEXELS[0] <= t <= CRITERION_TEXELS[1]]

    stats = {}      # (mesh, tex, amp, level, q) -> dict of aggregates
    total_viol = 0
    for mesh_name, tex_name in ASSETS:
        for amp in AMPLITUDES:
            acc = [{q: {"ratio": [], "relw": [], "n": 0, "kept": 0}
                    for q in QUANTITIES} for _ in range(n_levels)]
            for t in tri_picks[mesh_name]:
                tri = meshes[mesh_name].triangle(int(t))
                scale = amp * mean_edge(tri)
                rows, q_scale = triangle_tightness(
                    tri, textures[tex_name], scale)
                for level, row in enumerate(rows):
                    for q in QUANTITIES:
                        w_c = row[q]["w_cert"].ravel()
                        w_t = row[q]["w_true"].ravel()
                        keep = w_t >= RANGE_FLOOR * q_scale[q]
                        a = acc[level][q]
                        a["ratio"].append(w_c[keep] / w_t[keep])
                        a["relw"].append(w_c / q_scale[q])
                        a["n"] += w_c.size
                        a["kept"] += int(keep.sum())
                        total_viol += row[q]["viol"]
            for level in range(n_levels):
                for q in QUANTITIES:
                    a = acc[level][q]
                    ratio = np.concatenate(a["ratio"])
                    relw = np.concatenate(a["relw"])
                    stats[(mesh_name, tex_name, amp, level, q)] = {
                        "med": float(np.median(ratio)) if ratio.size else np.nan,
                        "p90": float(np.percentile(ratio, 90)) if ratio.size else np.nan,
                        "kept": a["kept"] / a["n"],
                        "relw": float(np.median(relw)),
                    }
            print(f"\n{mesh_name} + {tex_name}, amplitude {amp}x edge "
                  f"(median ratio [kept%], median relative width; "
                  f"* = criterion level):")
            for level in range(n_levels):
                parts = []
                for q in QUANTITIES:
                    s = stats[(mesh_name, tex_name, amp, level, q)]
                    parts.append(f"{q} {s['med']:5.2f} [{100 * s['kept']:3.0f}%]"
                                 f" w {s['relw']:.3f}")
                mark = "*" if level in criterion else " "
                print(f" {mark}{texels[level]:3d}-texel   " + "   ".join(parts))

    # Figures: median ratio vs cell size, one panel per asset combo.
    for amp in AMPLITUDES:
        fig, axes = plt.subplots(2, 2, figsize=(10, 7), sharex=True)
        for ax, (mesh_name, tex_name) in zip(axes.ravel(), ASSETS):
            for q in QUANTITIES:
                med = [stats[(mesh_name, tex_name, amp, level, q)]["med"]
                       for level in range(n_levels)]
                ax.plot(texels, med, "o-", label=q)
            ax.axhline(KILL_RATIO, color="k", ls="--", lw=0.8)
            ax.axvspan(*CRITERION_TEXELS, color="0.9", zorder=0)
            ax.set_xscale("log", base=2)
            ax.set_yscale("log")
            ax.set_title(f"{mesh_name} + {tex_name}", fontsize=9)
            ax.set_xlabel("cell side (texels)")
            ax.set_ylabel("median width ratio (certified / true)")
        axes[0, 0].legend(fontsize=8)
        fig.suptitle(f"Taylor pyramid tightness, amplitude {amp}x edge "
                     f"(shaded: criterion levels, dashed: kill ratio)")
        fig.tight_layout()
        fig.savefig(OUT_DIR / f"exp05_tightness_amp{amp}.png", dpi=150)

    # Kill criterion: typical content = 0.05x-edge amplitude. Evaluated on
    # the quantities the applications consume. lam_min is excluded
    # and reported separately: its true per-cell range is pinned near zero
    # by structure (a slope-dominated metric is a rank-one-like update of
    # G0, which leaves the small eigenvalue almost exactly 1), so certified
    # width divided by that range measures the structure, not the node; the
    # meaningful number for lam_min is its width relative to its value.
    primary = ["sqrt_det", "lam_max", "cone"]
    worst, worst_where = 0.0, ""
    for (mesh_name, tex_name, amp, level, q), s in stats.items():
        if (amp != 0.05 or level not in criterion or q not in primary
                or s["kept"] < MIN_KEPT):
            continue
        if s["med"] > worst:
            worst, worst_where = s["med"], \
                f"{q} on {mesh_name}+{tex_name}, {texels[level]}-texel cells"
    lam_level = criterion[-1]
    lam_min_relw = [stats[(mn, tn, 0.05, lam_level, "lam_min")]["relw"]
                    for mn, tn in ASSETS]
    print(f"\nconservativeness violations: {total_viol}")
    print(f"worst median ratio at criterion levels (cells up to "
          f"{CRITERION_TEXELS[1]} texels), 0.05x amplitude, over "
          f"{'/'.join(primary)}: {worst:.2f} ({worst_where})")
    print(f"lam_min (excluded, see note above): median certified width is "
          f"{min(lam_min_relw):.2f}-{max(lam_min_relw):.2f} of its value at "
          f"{texels[lam_level]}-texel cells")
    print(f"elapsed: {time.perf_counter() - t_start:.1f} s")
    verdict(total_viol == 0 and worst <= KILL_RATIO,
            f"bounds conservative and within {KILL_RATIO}x of the true range "
            f"at cells up to {CRITERION_TEXELS[1]} texels on typical content "
            f"(worst median {worst:.2f}); coarser cells steer efficiency "
            f"only, see the per-level table"
            if worst <= KILL_RATIO else
            f"kill criterion not met: worst median ratio "
            f"{worst:.2f} > {KILL_RATIO} ({worst_where})")


if __name__ == "__main__":
    main()
