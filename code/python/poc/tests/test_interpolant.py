"""Interpolant correctness: analytic gradients against finite differences,
and the C1 property of the B-spline."""

import numpy as np

from dmapref.interpolant import BilinearInterpolant, BSplineInterpolant

RNG = np.random.default_rng(11)
VALUES = RNG.random((9, 9))


def fd_grad(interp, x, y, eps=1e-7):
    return np.array([
        (interp.h(x + eps, y) - interp.h(x - eps, y)) / (2 * eps),
        (interp.h(x, y + eps) - interp.h(x, y - eps)) / (2 * eps),
    ])


def interior_points():
    """Points away from cell boundaries (where bilinear grad is undefined)."""
    return [(0.13, 0.22), (0.41, 0.68), (0.86, 0.31)]


def test_bilinear_reproduces_nodes():
    interp = BilinearInterpolant(VALUES)
    H, W = VALUES.shape
    for j in (0, 3, 8):
        for i in (0, 5, 8):
            assert np.isclose(interp.h(i / (W - 1), j / (H - 1)), VALUES[j, i])


def test_bilinear_gradient_matches_fd():
    interp = BilinearInterpolant(VALUES, scale=0.7)
    for (x, y) in interior_points():
        assert np.allclose(interp.grad(x, y), fd_grad(interp, x, y), atol=1e-5)


def test_bspline_gradient_matches_fd():
    interp = BSplineInterpolant(VALUES, scale=0.7)
    for (x, y) in interior_points():
        assert np.allclose(interp.grad(x, y), fd_grad(interp, x, y), atol=1e-5)


def test_bspline_is_c1_across_spans():
    """Gradient continuous across a span boundary; bilinear is not."""
    interp = BSplineInterpolant(VALUES)
    n_span = VALUES.shape[1] - 2
    x_boundary = 3 / n_span   # an interior span boundary in x
    eps = 1e-9
    g_left = interp.grad(x_boundary - eps, 0.4)
    g_right = interp.grad(x_boundary + eps, 0.4)
    assert np.allclose(g_left, g_right, atol=1e-6)
