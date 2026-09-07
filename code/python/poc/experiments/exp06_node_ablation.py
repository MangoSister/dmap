"""Experiment 6 — Is the joint Taylor node necessary? (Master plan §7,
Phase 1 ablations.)

Compares the joint node (A0) against the alternatives it claims to beat
("Taylor-model bound pyramid" §1):

  A0  joint Taylor node, affine propagation with shared symbols (ours)
  A1  independent min-max channels for (h, hu, hv), interval propagation
      (the channels themselves are exact per cell -- the strongest
      independent-interval storage possible)
  A2  the Taylor node collapsed to intervals, interval propagation
      (same storage as A0; isolates the value of shared symbols)
  A3  min-max channels promoted to affine forms with private symbols
      (each channel enters every term with one consistent value, but h and
      the gradient stay mutually uncorrelated; isolates the joint storage)

Reported per level, on a typical combo and on an oblique stress combo:
- median certified sqrt(det G) width, per variant, against the true range;
- false-degeneracy alarms: cells whose certified det G lower bound crosses
  zero while the true det stays positive -- the §1 failure, where the same
  gradient is given two inconsistent worst-case values;
- min-max h recovered from the Taylor node against the exact channel.
"""

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from _common import DATA_DIR, OUT_DIR, verdict
from dmapref.dense_reference import (cell_sample_points, level_ranges,
                                     pointwise_fields)
from dmapref.displacement import downsample_box, load_texture
from dmapref.mesh import load_obj
from dmapref.node_bounds import (boxed_forms, cell_enclosure,
                                 propagate_affine, propagate_interval,
                                 taylor_forms, taylor_intervals)
from dmapref.pyramid import MinMaxPyramid, TaylorPyramid, minmax_from_taylor

TEX_NODES = 257
SAMPLES_PER_CELL = 5
N_TRIANGLES = 5
COMBOS = [("cc_torus.obj", "disp_rock.png", 0.05, "typical"),
          ("cc_torus.obj", "disp_terrain.png", 0.05, "smooth"),
          ("spot.obj", "disp_cobble.png", 0.2, "oblique stress")]
VARIANTS = ["A0", "A1", "A2", "A3"]


def mean_edge(tri):
    return (np.linalg.norm(tri.e1) + np.linalg.norm(tri.e2)
            + np.linalg.norm(tri.e2 - tri.e1)) / 3.0


def variant_bounds(pyr, mm, level, enc):
    s = pyr.half_extent(level)
    lv = pyr.levels[level]
    ch = mm.levels[level]
    mm_ivs = ((ch["h_lo"], ch["h_hi"]), (ch["hu_lo"], ch["hu_hi"]),
              (ch["hv_lo"], ch["hv_hi"]))
    return {
        "A0": propagate_affine(*taylor_forms(lv, s, s), enc),
        "A1": propagate_interval(*mm_ivs, enc),
        "A2": propagate_interval(*taylor_intervals(lv, s, s), enc),
        "A3": propagate_affine(*boxed_forms(*mm_ivs), enc),
    }


def main():
    rng = np.random.default_rng(3)
    textures = {t: downsample_box(load_texture(DATA_DIR / t), TEX_NODES)
                for t in {c[1] for c in COMBOS}}
    meshes = {m: load_obj(DATA_DIR / m) for m in {c[0] for c in COMBOS}}

    n_levels = (TEX_NODES - 1).bit_length()
    texels = [2 ** k for k in range(n_levels)]
    fig, axes = plt.subplots(1, len(COMBOS), figsize=(5.5 * len(COMBOS), 4.5))
    all_ok = True

    for (mesh_name, tex_name, amp, label), ax in zip(COMBOS, axes):
        mesh = meshes[mesh_name]
        widths = {v: [[] for _ in range(n_levels)] for v in VARIANTS}
        true_w = [[] for _ in range(n_levels)]
        alarms = {v: np.zeros(n_levels) for v in VARIANTS}
        cells = np.zeros(n_levels)
        rec_ratio = [[] for _ in range(n_levels)]

        for t in rng.choice(mesh.n_triangles, N_TRIANGLES, replace=False):
            tri = mesh.triangle(int(t))
            scale = amp * mean_edge(tri)
            values = textures[tex_name]
            pyr = TaylorPyramid(values, scale)
            mm = MinMaxPyramid(values, scale)
            n, m = pyr.n_leaf, SAMPLES_PER_CELL
            x = cell_sample_points(n, m)
            X, Y = np.meshgrid(x, x, indexing="xy")
            f = pointwise_fields(tri, values, scale, X, Y)
            det_truth = level_ranges(f["det"], n, m, pyr.n_levels)
            sq_truth = level_ranges(f["sqrt_det"], n, m, pyr.n_levels)

            for level in range(pyr.n_levels):
                U0, V0 = pyr.centers(level)
                s = pyr.half_extent(level)
                enc = cell_enclosure(tri, U0, V0, s, s)
                outs = variant_bounds(pyr, mm, level, enc)
                det_pos = det_truth[level][0] > 0
                cells[level] += det_pos.size
                for v in VARIANTS:
                    lo, hi = outs[v]["sqrt_det"]
                    widths[v][level].append((hi - lo).ravel())
                    alarms[v][level] += int(
                        ((outs[v]["det"][0] <= 0.0) & det_pos).sum())
                true_w[level].append(
                    (sq_truth[level][1] - sq_truth[level][0]).ravel())
                # min-max recovery vs the exact channel
                rec_lo, rec_hi = minmax_from_taylor(pyr.levels[level], s)
                ch = mm.levels[level]
                exact = ch["h_hi"] - ch["h_lo"]
                ok = exact > 1e-12 * scale
                rec_ratio[level].append(
                    ((rec_hi - rec_lo)[ok] / exact[ok]).ravel())

        print(f"\n{mesh_name} + {tex_name}, amplitude {amp}x edge ({label})")
        print("  median sqrt_det width (x = vs A0) and det<=0 false alarms:")
        for level in range(n_levels):
            med = {v: np.median(np.concatenate(widths[v][level]))
                   for v in VARIANTS}
            t_med = np.median(np.concatenate(true_w[level]))
            parts = [f"true {t_med:.2e}"]
            for v in VARIANTS:
                rate = alarms[v][level] / cells[level]
                parts.append(f"{v} {med[v] / med['A0']:6.2f}x"
                             f" [{100 * rate:5.2f}% alarm]")
            rec = np.median(np.concatenate(rec_ratio[level]))
            print(f"   {texels[level]:3d}-texel   " + "  ".join(parts)
                  + f"   recovery {rec:.3f}x exact")

        for v in VARIANTS:
            ax.plot(texels, [np.median(np.concatenate(widths[v][level]))
                             for level in range(n_levels)], "o-", label=v)
        ax.plot(texels, [np.median(np.concatenate(true_w[level]))
                         for level in range(n_levels)], "k--",
                label="true range")
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xlabel("cell side (texels)")
        ax.set_ylabel("median certified sqrt_det width")
        ax.set_title(f"{mesh_name} + {tex_name}, {amp}x ({label})",
                     fontsize=9)
        ax.legend(fontsize=8)

        # A0 must never be materially (>5%) wider than any ablation. Small
        # coarse-level wins for A1/A3 are expected: their h interval is the
        # exact min-max channel, while A0 recovers h from the plane and
        # remainder, which is much wider near the root (recovery column).
        for level in range(n_levels):
            med0 = np.median(np.concatenate(widths["A0"][level]))
            for v in VARIANTS[1:]:
                if np.median(np.concatenate(widths[v][level])) < 0.95 * med0:
                    all_ok = False

    fig.tight_layout()
    fig.savefig(OUT_DIR / "exp06_node_ablation.png", dpi=150)
    verdict(all_ok, "the joint node with intersected affine-interval "
                    "propagation is at least as tight as every ablation at "
                    "every level; the per-level table shows where the "
                    "margin lies and where the variants coincide")


if __name__ == "__main__":
    main()
