"""Experiment 8 - Why is min-max h recovered from the Taylor node looser
than a dedicated channel, and how much of the gap is the fold?
("Taylor-model bound pyramid" section 5; master plan section 2.2.)

The recovery is  h in h0 +- (|gu|*su + |gv|*sv + r): two terms added as if
their extremes coincided. Phase 1 measured the result (1.06x the exact
channel at texel cells, 5-29x at the root) but not its cause. This splits it:

  slope choice   the tightest possible r for the slope the fold picked,
                 against the true range;
  fold cost      what the fold's r adds on top of that tightest r.

Both use the fact that h is bilinear per texel, so h minus a linear function
is still bilinear per texel and its extremes over any union of texel cells
sit at texel grid NODES. The tightest node for a given slope is therefore
computable exactly by enumeration, with no sampling and no padding - which
is also the candidate construction this experiment prices. That construction
now lives in the library as TaylorPyramid(build="direct").

Soundness checks on it: it must reproduce the closed-form leaf at level 0,
agree with the fold on the gradient channels (which the fold already
computes exactly), never exceed the folded r, and survive a dense
conservativeness spot check.
"""

import numpy as np

from _common import DATA_DIR, verdict
from dmapref.displacement import downsample_box, load_texture
from dmapref.pyramid import MinMaxPyramid, TaylorPyramid

TEX_NODES = 65                       # 64 leaf cells per side
TEXTURES = ["disp_rock.png", "disp_cobble.png"]
SPOT_CHECK_CELLS = 200
SPOT_CHECK_SAMPLES = 64
SEED = 2027


def load_grid(name, n_leaf):
    """A (n_leaf + 1)^2 node grid from a height texture."""
    values = downsample_box(load_texture(DATA_DIR / name), n_leaf)
    return np.pad(values, ((0, 1), (0, 1)), mode="edge")


def bilinear(V, u, v, N):
    """h at (u, v) on the node grid, matching HeightGrid's cell rule."""
    fu, fv = u * N, v * N
    i = np.minimum(fu.astype(int), N - 1)
    j = np.minimum(fv.astype(int), N - 1)
    s, t = fu - i, fv - j
    return ((1 - s) * (1 - t) * V[j, i] + s * (1 - t) * V[j, i + 1]
            + (1 - s) * t * V[j + 1, i] + s * t * V[j + 1, i + 1])


def count_violations(V, N, level, s, rng):
    """Dense samples inside random cells must lie inside the node's slab."""
    m = level["h0"].shape[0]
    violations = 0
    for _ in range(SPOT_CHECK_CELLS):
        i, j = int(rng.integers(m)), int(rng.integers(m))
        u = (i + rng.random(SPOT_CHECK_SAMPLES)) * (2 * s)
        v = (j + rng.random(SPOT_CHECK_SAMPLES)) * (2 * s)
        du, dv = u - (i + 0.5) * 2 * s, v - (j + 0.5) * 2 * s
        model = level["h0"][j, i] + level["gu"][j, i] * du + level["gv"][j, i] * dv
        residual = np.abs(bilinear(V, u, v, N) - model)
        violations += int((residual > level["r"][j, i] + 1e-12).sum())
    return violations


def recovery_ratio(V, N):
    """Median recovered-to-true min-max h width, per level."""
    taylor, minmax = TaylorPyramid(V), MinMaxPyramid(V)
    out = []
    for L in range(taylor.n_levels):
        s = taylor.half_extent(L)
        level = taylor.levels[L]
        recovered = np.abs(level["gu"]) * s + np.abs(level["gv"]) * s + level["r"]
        true_half = 0.5 * (minmax.levels[L]["h_hi"] - minmax.levels[L]["h_lo"])
        ok = true_half > 1e-12
        out.append(np.median(recovered[ok] / true_half[ok]) if ok.any() else 1.0)
    return out


def slope_diagnostic(V, N):
    """Is the stored slope a trend, or the midrange of a huge gradient set?

    The fold sets the parent slope to the midpoint of the gradient interval
    hull, which is what the gradient channel needs. Compare it against the
    cell's actual mean gradient, which is what a height trend would be.
    """
    taylor = TaylorPyramid(V)
    leaf_gu = taylor.levels[0]["gu"]
    rows = []
    for L in range(taylor.n_levels):
        m, k = N >> L, 1 << L
        mean_gu = leaf_gu.reshape(m, k, m, k).mean(axis=(1, 3))
        rows.append((k, np.median(np.abs(taylor.levels[L]["gu"])),
                     np.median(np.abs(mean_gu)), np.median(taylor.levels[L]["ru"])))
    return rows


def main():
    N = TEX_NODES - 1
    rng = np.random.default_rng(SEED)
    all_ok = True

    # An exact plane must recover exactly: the six numbers can represent a
    # min-max node (slope 0, r = the true half-range), so any looseness comes
    # from the fold's choices, not from the representation.
    grid = np.linspace(0.0, 1.0, N + 1)
    U, Vv = np.meshgrid(grid, grid, indexing="xy")
    plane_ratios = recovery_ratio(0.3 * U + 0.2 * Vv, N)
    all_ok &= max(abs(r - 1.0) for r in plane_ratios) < 1e-9
    print(f"exact plane h = 0.3u + 0.2v: recovered/true = "
          f"{min(plane_ratios):.6f} to {max(plane_ratios):.6f} over all levels")

    for name in TEXTURES:
        V = load_grid(name, N)
        taylor = TaylorPyramid(V)
        direct_pyr = TaylorPyramid(V, build="direct")
        minmax = MinMaxPyramid(V)
        print(f"\n=== {name} ({N} leaf cells per side) ===")
        print(f"{'cell':>5} {'true half T':>12} {'slope term':>11} {'r folded':>10} "
              f"{'r direct':>10} | {'ideal/T':>8} {'folded/T':>9} {'fold cost':>10} "
              f"{'viol':>5}")

        grad_diff = 0.0
        for L in range(taylor.n_levels):
            s = taylor.half_extent(L)
            folded, direct = taylor.levels[L], direct_pyr.levels[L]
            true_half = 0.5 * (minmax.levels[L]["h_hi"] - minmax.levels[L]["h_lo"])
            slope = np.abs(folded["gu"]) * s + np.abs(folded["gv"]) * s
            ok = true_half > 1e-12

            # the fold already computes these channels exactly
            grad_diff = max(grad_diff, max(np.abs(direct[k] - folded[k]).max()
                                           for k in ("gu", "gv", "ru", "rv",
                                                     "h_min", "h_max")))
            all_ok &= bool((direct["r"] <= folded["r"] + 1e-15).all())

            violations = count_violations(V, N, direct, s, rng)
            all_ok &= violations == 0

            positive = direct["r"] > 0
            ideal = (slope[ok] + direct["r"][ok]) / true_half[ok]
            got = (slope[ok] + folded["r"][ok]) / true_half[ok]
            print(f"{1 << L:>5} {np.median(true_half):>12.5f} {np.median(slope):>11.5f} "
                  f"{np.median(folded['r']):>10.5f} {np.median(direct['r']):>10.5f} | "
                  f"{np.median(ideal):>8.2f} {np.median(got):>9.2f} "
                  f"{np.median(folded['r'][positive] / direct['r'][positive]):>10.2f} "
                  f"{violations:>5}")

        # the direct construction must degenerate to the closed-form leaf
        leaf_diff = max(np.abs(direct_pyr.levels[0][k] - taylor.levels[0][k]).max()
                        for k in ("h0", "gu", "gv", "r", "ru", "rv",
                                  "h_min", "h_max"))
        all_ok &= leaf_diff < 1e-14 and grad_diff < 1e-12
        print(f"  level 0 vs the closed-form leaf: max abs diff {leaf_diff:.1e}")
        print(f"  channels the fold already gets exactly (gradient, min-max): "
              f"max abs diff {grad_diff:.1e}")

        print(f"  {'cell':>5} {'|stored slope|':>15} {'|mean gradient|':>16} "
              f"{'hull half-width':>16}")
        for k, stored, mean_grad, hull in slope_diagnostic(V, N):
            print(f"  {k:>5} {stored:>15.4f} {mean_grad:>16.4f} {hull:>16.4f}")

    verdict(all_ok, "the recovery gap has two causes and only one is the "
                    "fold: direct construction is exact, reproduces the leaf "
                    "and the fold's gradient and min-max channels, and "
                    "tightens r, but the slope stays the gradient-hull "
                    "midpoint, so a dedicated min-max h channel is the only "
                    "route to parity with TFDM/RMIP at coarse levels")


if __name__ == "__main__":
    main()
