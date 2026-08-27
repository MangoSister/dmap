"""Interpolants: continuous h(x, y) from a grid of texture values.

Two candidates from the master plan's §4 interpolant decision:
- bilinear: matches TFDM; gradient is discontinuous across cell edges;
- biquadratic B-spline: C1 smooth; Niessner & Loop 2013 used biquadratic.

Both expose the same interface: h(x, y) and grad(x, y) over the unit square,
with gradients computed analytically from the interpolant (never stored;
master plan §4 "gradient source" row).

Grid convention: values[j, i] sits at node (x, y) = (i/(W-1), j/(H-1)).
"""

import numpy as np


class BilinearInterpolant:
    """Piecewise-bilinear surface through the grid values."""

    def __init__(self, values, scale=1.0):
        self.values = np.asarray(values, dtype=np.float64)
        self.scale = scale
        self.H, self.W = self.values.shape

    def _cell(self, x, y):
        """Cell index and local coordinates (s, t) in [0, 1]^2."""
        fx = np.clip(x, 0.0, 1.0) * (self.W - 1)
        fy = np.clip(y, 0.0, 1.0) * (self.H - 1)
        i = min(int(fx), self.W - 2)
        j = min(int(fy), self.H - 2)
        return i, j, fx - i, fy - j

    def h(self, x, y):
        i, j, s, t = self._cell(x, y)
        v = self.values
        return self.scale * (
            (1 - s) * (1 - t) * v[j, i] + s * (1 - t) * v[j, i + 1]
            + (1 - s) * t * v[j + 1, i] + s * t * v[j + 1, i + 1]
        )

    def grad(self, x, y):
        i, j, s, t = self._cell(x, y)
        v = self.values
        dh_ds = (1 - t) * (v[j, i + 1] - v[j, i]) + t * (v[j + 1, i + 1] - v[j + 1, i])
        dh_dt = (1 - s) * (v[j + 1, i] - v[j, i]) + s * (v[j + 1, i + 1] - v[j, i + 1])
        return self.scale * np.array([dh_ds * (self.W - 1), dh_dt * (self.H - 1)])


def _quadratic_basis(s):
    """Uniform quadratic B-spline basis on one span, s in [0, 1]."""
    return np.array([0.5 * (1 - s) ** 2, 0.5 * (-2 * s * s + 2 * s + 1), 0.5 * s * s])


def _quadratic_basis_deriv(s):
    return np.array([-(1 - s), 1 - 2 * s, s])


class BSplineInterpolant:
    """Biquadratic uniform B-spline with the grid values as control points.

    C1 everywhere. Approximating, not interpolating: the surface smooths the
    control values, which is acceptable for the interpolant decision (both
    candidates approximate the same texture). Spans are indexed so the unit
    square maps onto the full spline domain; control indices are clamped at
    the borders.
    """

    def __init__(self, values, scale=1.0):
        self.values = np.asarray(values, dtype=np.float64)
        self.scale = scale
        self.H, self.W = self.values.shape
        if self.W < 3 or self.H < 3:
            raise ValueError("B-spline needs at least a 3x3 grid")

    def _span(self, x, n):
        """Span index and local coordinate for n control points (n-2 spans)."""
        f = np.clip(x, 0.0, 1.0) * (n - 2)
        i = min(int(f), n - 3)
        return i, f - i

    def _control_block(self, ix, iy):
        return self.values[iy:iy + 3, ix:ix + 3]

    def h(self, x, y):
        ix, sx = self._span(x, self.W)
        iy, sy = self._span(y, self.H)
        C = self._control_block(ix, iy)
        return self.scale * (_quadratic_basis(sy) @ C @ _quadratic_basis(sx))

    def grad(self, x, y):
        ix, sx = self._span(x, self.W)
        iy, sy = self._span(y, self.H)
        C = self._control_block(ix, iy)
        bx, by = _quadratic_basis(sx), _quadratic_basis(sy)
        dbx, dby = _quadratic_basis_deriv(sx), _quadratic_basis_deriv(sy)
        dh_dx = (by @ C @ dbx) * (self.W - 2)
        dh_dy = (dby @ C @ bx) * (self.H - 2)
        return self.scale * np.array([dh_dx, dh_dy])
