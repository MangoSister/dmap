"""The Taylor-model bound pyramid and its min-max ablation counterpart.

Implements "Taylor-model bound pyramid" §2-§5 for the decided bilinear
interpolant. A pyramid is built over a (2^L + 1)^2 node grid on the unit
square (the tile), giving 2^L x 2^L leaf cells at level 0 and one root cell
at level L. Node semantics over a cell with centre (u0, v0) and half-extents
(s, s), writing du = u - u0, dv = v - v0:

    |h - (h0 + gu*du + gv*dv)| <= r,   |h_u - gu| <= ru,   |h_v - gv| <= rv,
    h_min <= h <= h_max.

Eight numbers per node. The first six are the Taylor model: a plane plus a
remainder, whose slab is O(s^2) thick. The last two are the ordinary min-max
channel, whose box is O(s) tall but axis-aligned. Neither replaces the
other: the slab is the tighter region in space at fine cells, the box the
tighter scalar interval at coarse cells (master plan §2.2).

Grid convention matches interpolant.py: values[j, i] sits at
(u, v) = (i/(W-1), j/(H-1)), so axis 0 of every level array is v and
axis 1 is u.
"""

import numpy as np
from numpy.lib.stride_tricks import sliding_window_view

BUILDS = ("fold", "direct")

CHANNELS = ("h0", "gu", "gv", "r", "ru", "rv", "h_min", "h_max")


def texel_coeffs(V, n_leaf):
    """Per-texel bilinear coefficients (c0, c1, c2, c3), note §3:
    h = c0 + c1*du + c2*dv + c3*du*dv in coordinates centred on the texel."""
    w = 1.0 / n_leaf
    v00, v10 = V[:-1, :-1], V[:-1, 1:]
    v01, v11 = V[1:, :-1], V[1:, 1:]
    c0 = 0.25 * (v00 + v10 + v01 + v11)
    c1 = 0.5 * ((v10 - v00) + (v11 - v01)) / w
    c2 = 0.5 * ((v01 - v00) + (v11 - v10)) / w
    c3 = (v00 - v10 - v01 + v11) / (w * w)
    return c0, c1, c2, c3


def _check_node_grid(V):
    n = V.shape[0] - 1
    if V.shape[0] != V.shape[1] or n < 1 or (n & (n - 1)) != 0:
        raise ValueError("pyramid needs a (2^L + 1) x (2^L + 1) node grid")
    return n


class TaylorPyramid:
    """Levels of the eight channel arrays, level 0 = leaves.

    build="fold" is the bottom-up 4-to-1 fold of note §4. build="direct"
    builds every level independently by enumerating the texel nodes the cell
    covers; it agrees with the fold on gu, gv, ru, rv, h_min and h_max, and
    gives a smaller r, because the fold compounds the remainders of its
    children.
    """

    def __init__(self, values, scale=1.0, build="fold"):
        if build not in BUILDS:
            raise ValueError(f"build must be one of {BUILDS}, got {build!r}")
        V = np.asarray(values, dtype=np.float64) * scale
        self.n_leaf = _check_node_grid(V)
        self.n_levels = self.n_leaf.bit_length()   # L + 1 levels, root included
        self.build = build
        if build == "fold":
            self.levels = [self._leaf_level(V)]
            while self.levels[-1]["h0"].shape[0] > 1:
                self.levels.append(self._fold(self.levels[-1]))
        else:
            self.levels = [self._direct_level(V, level)
                           for level in range(self.n_levels)]

    def cell_width(self, level):
        return 2 ** level / self.n_leaf

    def half_extent(self, level):
        return 0.5 * self.cell_width(level)

    def centers(self, level):
        """(U0, V0) meshgrids of cell centres at a level, shape (m, m),
        indexed [j, i] like the level arrays."""
        w = self.cell_width(level)
        m = self.levels[level]["h0"].shape[0]
        c = (np.arange(m) + 0.5) * w
        U0, V0 = np.meshgrid(c, c, indexing="xy")
        return U0, V0

    def _leaf_level(self, V):
        """Exact node per texel cell (note §3): the bilinear patch is
        h = c0 + c1*du + c2*dv + c3*du*dv in centred coordinates, and every
        supremum is attained at a cell corner, which also makes the min-max
        channel the range of the four corners."""
        c0, c1, c2, c3 = texel_coeffs(V, self.n_leaf)
        corners = np.stack([V[:-1, :-1], V[:-1, 1:], V[1:, :-1], V[1:, 1:]])
        s = 0.5 / self.n_leaf
        return {"h0": c0, "gu": c1, "gv": c2,
                "r": np.abs(c3) * s * s, "ru": np.abs(c3) * s,
                "rv": np.abs(c3) * s,
                "h_min": corners.min(axis=0), "h_max": corners.max(axis=0)}

    def _fold(self, child):
        """Conservative 4-to-1 fold (note §4, with the plane built from
        interval hulls rather than the child mean; the note allows any
        plane).

        Gradient: the parent interval is the interval hull of the child
        intervals [g_c ± rho_c] -- the smallest interval containing them;
        the parent slope is its midpoint and the remainder its half-width.
        Value: with the parent slope fixed, the child planes minus the
        parent slope are affine per child quadrant, so their extreme values
        sit at quadrant corners; the parent offset h0 is the midpoint of
        the corner extremes (including each child's r), which minimizes r.
        Height range: the min and max of the children's, which is exact,
        since the range over a union is the union of the ranges.
        """
        m = child["h0"].shape[0] // 2
        # child index (dy, dx) on axes (1, 3); child centre offset from the
        # parent centre is (±s/2, ±s/2) with s the parent half-extent.
        c = {k: a.reshape(m, 2, m, 2) for k, a in child.items()}
        s = 0.5 / m                               # parent half-extent
        dx = (np.array([-0.5, 0.5]) * s)[None, None, None, :]
        dy = (np.array([-0.5, 0.5]) * s)[None, :, None, None]

        def hull(g, rho):
            hi = (g + rho).max(axis=(1, 3))
            lo = (g - rho).min(axis=(1, 3))
            return 0.5 * (hi + lo), 0.5 * (hi - lo)

        gu_p, ru_p = hull(c["gu"], c["ru"])
        gv_p, rv_p = hull(c["gv"], c["rv"])

        # Child plane minus the parent slope, at the child quadrant corners.
        du_g = c["gu"] - gu_p[:, None, :, None]
        dv_g = c["gv"] - gv_p[:, None, :, None]
        base = c["h0"] - c["gu"] * dx - c["gv"] * dy   # child plane at centre
        upper = np.full_like(gu_p, -np.inf)
        lower = np.full_like(gu_p, np.inf)
        for su in (-0.5 * s, 0.5 * s):
            for sv in (-0.5 * s, 0.5 * s):
                corner = base + du_g * (dx + su) + dv_g * (dy + sv)
                upper = np.maximum(upper, (corner + c["r"]).max(axis=(1, 3)))
                lower = np.minimum(lower, (corner - c["r"]).min(axis=(1, 3)))
        h0_p = 0.5 * (upper + lower)
        r_p = 0.5 * (upper - lower)
        return {"h0": h0_p, "gu": gu_p, "gv": gv_p,
                "r": r_p, "ru": ru_p, "rv": rv_p,
                "h_min": c["h_min"].min(axis=(1, 3)),
                "h_max": c["h_max"].max(axis=(1, 3))}

    def _direct_level(self, V, level):
        """One level built from the node grid instead of by folding.

        Step 1 (gradient): on a texel h_u = c1 + c3*dv is linear in dv, so
        its exact range there is c1 ± |c3|*s_texel, and the cell's range is
        the hull of the ranges of its texels. Step 2 (value): with that
        slope fixed, the residual h - gu*du - gv*dv is bilinear per texel,
        so its extremes over the cell sit at the nodes of the CLOSED cell,
        k+1 per side, boundary included. Both steps are exact, so r is the
        smallest remainder the chosen slope admits.
        """
        N, k = self.n_leaf, 1 << level
        m = N >> level                            # cells per side
        _, c1, c2, c3 = texel_coeffs(V, N)
        spread = np.abs(c3) * (0.5 / N)           # texel half-extent

        def hull(c):
            lo = (c - spread).reshape(m, k, m, k).min(axis=(1, 3))
            hi = (c + spread).reshape(m, k, m, k).max(axis=(1, 3))
            return 0.5 * (hi + lo), 0.5 * (hi - lo)

        gu, ru = hull(c1)
        gv, rv = hull(c2)

        # Nodes of every closed cell, shape (m, m, k+1, k+1): axes 2 and 3
        # run over v and u inside the cell. Adjacent cells share a boundary
        # node, so the windows overlap by one. The node offsets from the
        # cell centre are the same for every cell.
        blocks = sliding_window_view(V, (k + 1, k + 1))[::k, ::k]
        d = (np.arange(k + 1) - 0.5 * k) / N
        R = (blocks - gu[..., None, None] * d[None, None, None, :]
             - gv[..., None, None] * d[None, None, :, None])
        hi, lo = R.max(axis=(2, 3)), R.min(axis=(2, 3))
        return {"h0": 0.5 * (hi + lo), "gu": gu, "gv": gv,
                "r": 0.5 * (hi - lo), "ru": ru, "rv": rv,
                "h_min": blocks.min(axis=(2, 3)),
                "h_max": blocks.max(axis=(2, 3))}


class MinMaxPyramid:
    """Independent exact min-max channels for h, h_u, h_v.

    The ablation A1 storage, and the exact min-max reference: for bilinear,
    per-cell h extrema sit at cell corners, h_u = c1 + c3*dv is linear in dv,
    and h_v = c2 + c3*du is linear in du, so the leaf ranges are exact; the
    min/max fold keeps them exact at every level (range of a union).
    """

    def __init__(self, values, scale=1.0):
        V = np.asarray(values, dtype=np.float64) * scale
        self.n_leaf = _check_node_grid(V)
        self.n_levels = self.n_leaf.bit_length()
        self.levels = [self._leaf_level(V)]
        while self.levels[-1]["h_lo"].shape[0] > 1:
            self.levels.append(self._fold(self.levels[-1]))

    def _leaf_level(self, V):
        corners = np.stack([V[:-1, :-1], V[:-1, 1:], V[1:, :-1], V[1:, 1:]])
        _, c1, c2, c3 = texel_coeffs(V, self.n_leaf)
        s = 0.5 / self.n_leaf
        return {"h_lo": corners.min(axis=0), "h_hi": corners.max(axis=0),
                "hu_lo": c1 - np.abs(c3) * s, "hu_hi": c1 + np.abs(c3) * s,
                "hv_lo": c2 - np.abs(c3) * s, "hv_hi": c2 + np.abs(c3) * s}

    def _fold(self, child):
        m = child["h_lo"].shape[0] // 2
        c = {k: a.reshape(m, 2, m, 2) for k, a in child.items()}
        out = {}
        for k, a in c.items():
            out[k] = a.min(axis=(1, 3)) if k.endswith("_lo") else a.max(axis=(1, 3))
        return out


def minmax_from_taylor(level_arrays, s):
    """Min-max h recovered from a Taylor level (note §5):
    h in h0 ± (|gu|*s + |gv|*s + r). Kept as the baseline the stored
    h_min/h_max channel is measured against, not as a substitute for it."""
    half = (np.abs(level_arrays["gu"]) * s + np.abs(level_arrays["gv"]) * s
            + level_arrays["r"])
    return level_arrays["h0"] - half, level_arrays["h0"] + half
