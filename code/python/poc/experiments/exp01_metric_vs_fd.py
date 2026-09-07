"""Experiment 1 — Is the metric formula right?

Question: does the closed-form metric (metric note §3) agree with central
finite differences of the displaced surface S = P + hN, at the expected
convergence order, on random oblique triangles with a synthetic analytic
displacement?

Expectation: FD error falls as O(eps^2) until roundoff, so the minimum over
the eps sweep should reach ~1e-9 relative error and the observed order in
the pre-roundoff range should be close to 2.
"""

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from _common import OUT_DIR, verdict
from dmapref import displacement
from dmapref.metric import metric_at
from dmapref.reference_surface import fd_metric
from dmapref.synthetic import random_oblique_triangle


def max_relative_error(eps, triangles, field, points):
    worst = 0.0
    for tri in triangles:
        for (u, v) in points:
            G = metric_at(tri, field, u, v)
            G_fd = fd_metric(tri, field, u, v, eps=eps)
            rel = np.linalg.norm(G - G_fd) / np.linalg.norm(G)
            worst = max(worst, rel)
    return worst


def main():
    rng = np.random.default_rng(42)
    triangles = [random_oblique_triangle(rng, tilt=0.6) for _ in range(20)]
    field = displacement.sinusoid(amp=0.25, fx=3.0, fy=4.0)
    points = [(0.2, 0.3), (0.5, 0.1), (0.1, 0.6), (0.3, 0.3)]

    eps_sweep = np.array([1e-2, 1e-3, 1e-4, 1e-5, 1e-6])
    errors = np.array([max_relative_error(e, triangles, field, points)
                       for e in eps_sweep])
    for e, err in zip(eps_sweep, errors):
        print(f"  eps = {e:8.0e}   max relative error = {err:.3e}")

    # Convergence order from the first two (pre-roundoff) sweep points.
    order = np.log(errors[0] / errors[1]) / np.log(eps_sweep[0] / eps_sweep[1])
    floor = errors.min()
    print(f"  observed order (eps 1e-2 -> 1e-3): {order:.2f}")
    print(f"  best agreement: {floor:.3e}")

    fig, ax = plt.subplots(figsize=(5, 4))
    ax.loglog(eps_sweep, errors, "o-", label="max relative error")
    ax.loglog(eps_sweep, errors[0] * (eps_sweep / eps_sweep[0]) ** 2,
              "--", color="gray", label="slope 2 reference")
    ax.set_xlabel("finite-difference step eps")
    ax.set_ylabel("max relative Frobenius error")
    ax.set_title("Closed-form metric vs finite differences")
    ax.legend()
    fig.tight_layout()
    fig.savefig(OUT_DIR / "exp01_metric_vs_fd.png", dpi=150)

    passed = floor < 1e-8 and 1.8 < order < 2.2
    verdict(passed, f"FD converges to the formula (order {order:.2f}, "
                    f"floor {floor:.1e}); figure in out/exp01_metric_vs_fd.png")


if __name__ == "__main__":
    main()
