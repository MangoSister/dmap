"""Experiment 7 — What does the Taylor pyramid cost? (Master plan §2.2:
"six channels instead of two, with the same pyramid topology".)

Measures, on random value grids (cost does not depend on content):
- build time of the Taylor pyramid against the 2-channel min-max height
  pyramid (the TFDM baseline) at several tile sizes;
- memory per pyramid (sum of level arrays), expected 3x the 2-channel one;
- per-node metric-bound query time (enclosure + affine propagation),
  batched over one level.
"""

import time

import numpy as np

from _common import verdict
from dmapref.node_bounds import cell_enclosure, propagate_affine, taylor_forms
from dmapref.pyramid import MinMaxPyramid, TaylorPyramid
from dmapref.synthetic import random_oblique_triangle

SIZES = [129, 257, 513]
REPS = 5


def build_time(cls, values, reps=REPS):
    best = np.inf
    for _ in range(reps):
        t0 = time.perf_counter()
        cls(values, scale=0.1)
        best = min(best, time.perf_counter() - t0)
    return best


def pyramid_bytes(pyr, keys):
    return sum(lv[k].nbytes for lv in pyr.levels for k in keys)


def main():
    rng = np.random.default_rng(0)
    print(f"{'tile':>6} {'taylor build':>13} {'min-max build':>14} "
          f"{'taylor MB':>10} {'2-ch MB':>8} {'ratio':>6}")
    ratios = []
    for n in SIZES:
        values = rng.random((n, n))
        t_taylor = build_time(TaylorPyramid, values)
        t_minmax = build_time(MinMaxPyramid, values)
        pyr = TaylorPyramid(values, 0.1)
        mm = MinMaxPyramid(values, 0.1)
        b_taylor = pyramid_bytes(pyr, ["h0", "gu", "gv", "r", "ru", "rv"])
        b_minmax = pyramid_bytes(mm, ["h_lo", "h_hi"])   # TFDM baseline
        ratios.append(b_taylor / b_minmax)
        print(f"{n - 1:>5}c {1e3 * t_taylor:>11.1f}ms {1e3 * t_minmax:>12.1f}ms "
              f"{b_taylor / 2 ** 20:>10.2f} {b_minmax / 2 ** 20:>8.2f} "
              f"{ratios[-1]:>6.2f}")
    print("(min-max build here folds 6 channels for the A1 ablation; the "
          "2-channel memory column is the TFDM height-only baseline)")

    # Per-node query cost: enclosure + propagation, batched over a level.
    tri = random_oblique_triangle(rng)
    pyr = TaylorPyramid(rng.random((257, 257)), 0.1)
    level = 2                               # 64x64 = 4096 cells
    U0, V0 = pyr.centers(level)
    s = pyr.half_extent(level)
    n_cells = U0.size
    best_enc, best_prop = np.inf, np.inf
    for _ in range(REPS):
        t0 = time.perf_counter()
        enc = cell_enclosure(tri, U0, V0, s, s)
        t1 = time.perf_counter()
        propagate_affine(*taylor_forms(pyr.levels[level], s, s), enc)
        t2 = time.perf_counter()
        best_enc = min(best_enc, t1 - t0)
        best_prop = min(best_prop, t2 - t1)
    print(f"\nbound query, batched over {n_cells} cells: "
          f"enclosure {1e6 * best_enc / n_cells:.1f} us/node, "
          f"propagation {1e6 * best_prop / n_cells:.1f} us/node "
          f"(numpy reference; the C++ core is the production answer)")

    ok = all(2.5 < r < 3.5 for r in ratios)
    verdict(ok, f"memory is {ratios[0]:.2f}x the 2-channel min-max pyramid "
                f"(6 channels as designed); build is one vectorized "
                f"mipmap-style pass")


if __name__ == "__main__":
    main()
