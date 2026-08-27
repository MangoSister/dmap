"""Dense vectorized pointwise evaluation over a tile, for true ranges.

The tightness study compares certified per-cell bounds against the true
range of each quantity over the cell. This module evaluates the pointwise
quantities (metric entries, sqrt(det G), stretch eigenvalues, unnormalized
normal) on a grid of samples covering every leaf cell, vectorized over the
whole grid; per-cell reductions and the level-by-level 2x2 poolings are
plain array operations.

The math duplicates metric.py on purpose: metric.py is the scalar reference
implementation checked by the unit tests, this is the batched copy used
where a Python loop per sample would be too slow. A unit test pins the two
against each other.
"""

import numpy as np


def cell_sample_points(n_cells, m, eps=1e-9):
    """Sample coordinates covering each of n_cells cells with m points per
    axis, endpoints inset by eps: bilinear extrema sit on cell corners, but
    the gradient is discontinuous across cell edges, and a sample exactly on
    an edge would be evaluated with the neighbor cell's gradient while being
    attributed to this cell. The inset keeps every sample's one-sided values
    inside its own cell at the cost of an O(eps) underestimate of the range.

    Returns x of shape (n_cells * m,); the same array serves both axes.
    Reshape a per-point quantity to (n_cells, m, n_cells, m) to reduce
    per cell.
    """
    w = 1.0 / n_cells
    offs = np.linspace(0.0, 1.0, m)
    offs[0], offs[-1] = eps, 1.0 - eps
    return ((np.arange(n_cells)[:, None] + offs[None, :]) * w).ravel()


def bilinear_grid(values, scale, X, Y):
    """Vectorized bilinear h, h_u, h_v at points (X, Y) in the unit square.

    Matches interpolant.BilinearInterpolant: values[j, i] sits at
    (i/(W-1), j/(H-1)); gradients are analytic per cell.
    """
    V = np.asarray(values, dtype=np.float64)
    H, W = V.shape
    fx = np.clip(X, 0.0, 1.0) * (W - 1)
    fy = np.clip(Y, 0.0, 1.0) * (H - 1)
    i = np.minimum(fx.astype(np.int64), W - 2)
    j = np.minimum(fy.astype(np.int64), H - 2)
    s, t = fx - i, fy - j

    v00, v10 = V[j, i], V[j, i + 1]
    v01, v11 = V[j + 1, i], V[j + 1, i + 1]
    h = ((1 - s) * (1 - t) * v00 + s * (1 - t) * v10
         + (1 - s) * t * v01 + s * t * v11)
    hu = ((1 - t) * (v10 - v00) + t * (v11 - v01)) * (W - 1)
    hv = ((1 - s) * (v01 - v00) + s * (v11 - v10)) * (H - 1)
    return scale * h, scale * hu, scale * hv


def pointwise_fields(tri, values, scale, X, Y):
    """Pointwise quantities at sample points (X, Y): dict with h, hu, hv,
    G00, G01, G11, det, sqrt_det, lam_min, lam_max, and the unnormalized
    normal n of shape X.shape + (3,)."""
    h, hu, hv = bilinear_grid(values, scale, X, Y)

    M = (tri.m0 + X[..., None] * tri.Mu + Y[..., None] * tri.Mv)
    Mlen = np.linalg.norm(M, axis=-1)
    N = M / Mlen[..., None]
    Nu = (tri.Mu - N * (N @ tri.Mu)[..., None]) / Mlen[..., None]
    Nv = (tri.Mv - N * (N @ tri.Mv)[..., None]) / Mlen[..., None]

    e1, e2 = tri.e1, tri.e2
    bt00, bt01 = -(Nu @ e1), -(Nv @ e1)
    bt10, bt11 = -(Nu @ e2), -(Nv @ e2)
    B00, B01, B11 = bt00, 0.5 * (bt01 + bt10), bt11
    C00 = (Nu * Nu).sum(axis=-1)
    C01 = (Nu * Nv).sum(axis=-1)
    C11 = (Nv * Nv).sum(axis=-1)
    a0, a1 = N @ e1, N @ e2

    G00 = (tri.G0[0, 0] - 2 * h * B00 + h * h * C00 + hu * hu + 2 * hu * a0)
    G01 = (tri.G0[0, 1] - 2 * h * B01 + h * h * C01 + hu * hv
           + hu * a1 + hv * a0)
    G11 = (tri.G0[1, 1] - 2 * h * B11 + h * h * C11 + hv * hv + 2 * hv * a1)

    det = G00 * G11 - G01 * G01
    A = np.linalg.inv(tri.G0)
    T = A[0, 0] * G00 + 2 * A[0, 1] * G01 + A[1, 1] * G11
    D = det / (tri.G0[0, 0] * tri.G0[1, 1] - tri.G0[0, 1] * tri.G0[1, 0])
    disc = np.maximum(T * T - 4 * D, 0.0)
    sd = np.sqrt(disc)

    Su = e1 + hu[..., None] * N + h[..., None] * Nu
    Sv = e2 + hv[..., None] * N + h[..., None] * Nv
    n = np.cross(Su, Sv)

    return {"h": h, "hu": hu, "hv": hv,
            "G00": G00, "G01": G01, "G11": G11,
            "det": det, "sqrt_det": np.sqrt(np.maximum(det, 0.0)),
            "lam_max": 0.5 * (T + sd), "lam_min": 0.5 * (T - sd),
            "n": n}


def per_cell_range(q, n_cells, m):
    """(min, max) per leaf cell, shape (n_cells, n_cells), for a quantity
    sampled on the cell_sample_points grid (Y along axis 0)."""
    blocks = q.reshape(n_cells, m, n_cells, m)
    return blocks.min(axis=(1, 3)), blocks.max(axis=(1, 3))


def pool_range(lo, hi):
    """Fold per-cell ranges one level up (2x2 pooling)."""
    m = lo.shape[0] // 2
    return (lo.reshape(m, 2, m, 2).min(axis=(1, 3)),
            hi.reshape(m, 2, m, 2).max(axis=(1, 3)))


def level_ranges(q, n_cells, m, n_levels):
    """Per-cell (min, max) at every pyramid level, leaf first."""
    lo, hi = per_cell_range(q, n_cells, m)
    out = [(lo, hi)]
    for _ in range(n_levels - 1):
        lo, hi = pool_range(lo, hi)
        out.append((lo, hi))
    return out
