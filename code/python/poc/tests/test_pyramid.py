"""Taylor pyramid: leaf exactness, fold conservativeness, and certified
metric bounds ("Taylor-model bound pyramid" §3, §4, §6)."""

import numpy as np

from dmapref.dense_reference import (bilinear_grid, cell_sample_points,
                                     level_ranges, pointwise_fields)
from dmapref.interpolant import BilinearInterpolant
from dmapref.metric import metric_at
from dmapref.node_bounds import (boxed_forms, cell_enclosure,
                                 propagate_affine, propagate_interval,
                                 taylor_forms, taylor_intervals)
from dmapref.pyramid import MinMaxPyramid, TaylorPyramid, minmax_from_taylor
from dmapref.synthetic import face_normal_triangle, random_oblique_triangle

RNG = np.random.default_rng(11)


def sample_grid(n_cells, m):
    x = cell_sample_points(n_cells, m)
    return np.meshgrid(x, x, indexing="xy")


def containing_cell(pyr, level, X, Y):
    w = pyr.cell_width(level)
    m = pyr.levels[level]["h0"].shape[0]
    ci = np.minimum((X / w).astype(np.int64), m - 1)
    cj = np.minimum((Y / w).astype(np.int64), m - 1)
    return ci, cj


def assert_node_containment(values, scale, tol=1e-10):
    """Dense h and gradient samples lie inside every ancestor node's slab
    and gradient intervals."""
    pyr = TaylorPyramid(values, scale)
    X, Y = sample_grid(pyr.n_leaf, 5)
    h, hu, hv = bilinear_grid(values, scale, X, Y)
    for level in range(pyr.n_levels):
        lv = pyr.levels[level]
        ci, cj = containing_cell(pyr, level, X, Y)
        w = pyr.cell_width(level)
        u0, v0 = (ci + 0.5) * w, (cj + 0.5) * w
        plane = (lv["h0"][cj, ci] + lv["gu"][cj, ci] * (X - u0)
                 + lv["gv"][cj, ci] * (Y - v0))
        assert (np.abs(h - plane) <= lv["r"][cj, ci] + tol).all()
        assert (np.abs(hu - lv["gu"][cj, ci]) <= lv["ru"][cj, ci] + tol).all()
        assert (np.abs(hv - lv["gv"][cj, ci]) <= lv["rv"][cj, ci] + tol).all()


def test_leaf_is_exact():
    """Leaf remainders equal the true suprema (attained at cell corners),
    not just bound them (note §3)."""
    values = RNG.random((9, 9))
    scale = 0.7
    pyr = TaylorPyramid(values, scale)
    field = BilinearInterpolant(values, scale)
    leaf = pyr.levels[0]
    n, w = pyr.n_leaf, pyr.cell_width(0)
    for i, j in [(0, 0), (3, 5), (7, 7), (2, 6)]:
        corners = [(i * w, j * w), ((i + 1) * w, j * w),
                   (i * w, (j + 1) * w), ((i + 1) * w, (j + 1) * w)]
        u0, v0 = (i + 0.5) * w, (j + 0.5) * w
        res = max(abs(field.h(u, v) - (leaf["h0"][j, i]
                                       + leaf["gu"][j, i] * (u - u0)
                                       + leaf["gv"][j, i] * (v - v0)))
                  for u, v in corners)
        assert abs(res - leaf["r"][j, i]) < 1e-12
        # The gradient is discontinuous across cell edges; approach the
        # supremum (attained on the cell's v-edges) from inside the cell.
        eps = 1e-9 * w
        dev_u = max(abs(field.grad(u0, j * w + eps)[0] - leaf["gu"][j, i]),
                    abs(field.grad(u0, (j + 1) * w - eps)[0] - leaf["gu"][j, i]))
        assert abs(dev_u - leaf["ru"][j, i]) < 1e-6


def test_containment_random_texture():
    assert_node_containment(RNG.random((33, 33)), scale=0.4)


def test_containment_step_texture():
    """Adversarial child disagreement: a hard step edge."""
    values = np.zeros((17, 17))
    values[:, 9:] = 1.0
    assert_node_containment(values, scale=1.0)


def test_ramp_gives_zero_remainders_and_zero_width_bounds():
    """A pure plane is explained exactly at every level (note §2); on a
    face-normal triangle the propagated metric bounds collapse to a point."""
    jj, ii = np.mgrid[0:17, 0:17]
    values = 0.3 * ii / 16 - 0.2 * jj / 16
    pyr = TaylorPyramid(values, scale=1.0)
    for lv in pyr.levels:
        assert (lv["r"] < 1e-14).all()
        assert (lv["ru"] < 1e-13).all() and (lv["rv"] < 1e-13).all()

    tri = face_normal_triangle(RNG)
    for level in range(pyr.n_levels):
        U0, V0 = pyr.centers(level)
        s = pyr.half_extent(level)
        enc = cell_enclosure(tri, U0, V0, s, s)
        out = propagate_affine(*taylor_forms(pyr.levels[level], s, s), enc)
        assert (out["sqrt_det"][1] - out["sqrt_det"][0] < 1e-10).all()
        # arccos near 1 turns 1e-13 roundoff radii into ~1e-7 angles.
        assert (out["cone"]["half_angle"] < 1e-5).all()


def _bound_check(out, truth, level, tol_rel=1e-9):
    """Certified intervals contain the true per-cell ranges."""
    for key in ["G00", "G01", "G11", "sqrt_det", "lam_min", "lam_max"]:
        if key.startswith("G"):
            iv = out["G"][(0, 0) if key == "G00" else
                          (0, 1) if key == "G01" else (1, 1)]
        else:
            iv = out[key]
        t_lo, t_hi = truth[key][level]
        scale = np.maximum(np.abs(t_lo), np.abs(t_hi)) + 1.0
        assert (iv[0] <= t_lo + tol_rel * scale).all(), key
        assert (iv[1] >= t_hi - tol_rel * scale).all(), key


def _make_case(n_nodes=17):
    tri = random_oblique_triangle(RNG, tilt=0.4)
    jj, ii = np.mgrid[0:n_nodes, 0:n_nodes]
    values = (0.5 * np.sin(3.0 * ii / n_nodes + 1.0)
              * np.cos(2.0 * jj / n_nodes) + 0.1 * RNG.random((n_nodes, n_nodes)))
    edge = (np.linalg.norm(tri.e1) + np.linalg.norm(tri.e2)) / 2
    return tri, values, 0.1 * edge


def _truth_tables(tri, values, scale, n_cells, m, n_levels):
    X, Y = sample_grid(n_cells, m)
    f = pointwise_fields(tri, values, scale, X, Y)
    truth = {k: level_ranges(f[k], n_cells, m, n_levels)
             for k in ["G00", "G01", "G11", "sqrt_det", "lam_min", "lam_max"]}
    return truth, f, (X, Y)


def test_propagation_is_conservative():
    tri, values, scale = _make_case()
    pyr = TaylorPyramid(values, scale)
    truth, f, (X, Y) = _truth_tables(tri, values, scale, pyr.n_leaf, 5,
                                     pyr.n_levels)
    for level in range(pyr.n_levels):
        U0, V0 = pyr.centers(level)
        s = pyr.half_extent(level)
        enc = cell_enclosure(tri, U0, V0, s, s)
        out = propagate_affine(*taylor_forms(pyr.levels[level], s, s), enc)
        _bound_check(out, truth, level)

        # Normal cone: every sampled normal within the certified half-angle.
        ci, cj = containing_cell(pyr, level, X, Y)
        axis = out["cone"]["axis"][:, cj, ci]
        n = f["n"]
        cosang = ((n[..., 0] * axis[0] + n[..., 1] * axis[1]
                   + n[..., 2] * axis[2])
                  / np.linalg.norm(n, axis=-1))
        ang = np.arccos(np.clip(cosang, -1.0, 1.0))
        assert (ang <= out["cone"]["half_angle"][cj, ci] + 1e-9).all()


def test_ablations_are_conservative():
    """A1/A2/A3 must be valid bounds too, or the ablation is unfair."""
    tri, values, scale = _make_case()
    pyr = TaylorPyramid(values, scale)
    mm = MinMaxPyramid(values, scale)
    truth, _, _ = _truth_tables(tri, values, scale, pyr.n_leaf, 5,
                                pyr.n_levels)
    for level in range(pyr.n_levels):
        U0, V0 = pyr.centers(level)
        s = pyr.half_extent(level)
        enc = cell_enclosure(tri, U0, V0, s, s)
        ch = mm.levels[level]
        a1_ivs = ((ch["h_lo"], ch["h_hi"]), (ch["hu_lo"], ch["hu_hi"]),
                  (ch["hv_lo"], ch["hv_hi"]))
        a2_ivs = taylor_intervals(pyr.levels[level], s, s)
        _bound_check(propagate_interval(*a1_ivs, enc), truth, level)
        _bound_check(propagate_interval(*a2_ivs, enc), truth, level)
        _bound_check(propagate_affine(*boxed_forms(*a1_ivs), enc), truth,
                     level)


def test_minmax_channels_exact_and_recovery_contains():
    values = RNG.random((17, 17))
    scale = 0.5
    mm = MinMaxPyramid(values, scale)
    pyr = TaylorPyramid(values, scale)
    X, Y = sample_grid(mm.n_leaf, 5)
    h, hu, hv = bilinear_grid(values, scale, X, Y)
    tr_h = level_ranges(h, mm.n_leaf, 5, mm.n_levels)
    tr_hu = level_ranges(hu, mm.n_leaf, 5, mm.n_levels)
    for level in range(mm.n_levels):
        ch = mm.levels[level]
        # Exact: the channels equal the true ranges (extrema on cell corners
        # and edges; the sample grid approaches them within eps).
        assert np.allclose(ch["h_lo"], tr_h[level][0], atol=1e-8)
        assert np.allclose(ch["h_hi"], tr_h[level][1], atol=1e-8)
        assert (ch["hu_lo"] <= tr_hu[level][0] + 1e-12).all()
        assert (ch["hu_hi"] >= tr_hu[level][1] - 1e-12).all()
        # Recovery from the Taylor node contains the exact channel.
        s = pyr.half_extent(level)
        lo, hi = minmax_from_taylor(pyr.levels[level], s)
        assert (lo <= ch["h_lo"] + 1e-12).all()
        assert (hi >= ch["h_hi"] - 1e-12).all()


def test_dense_reference_matches_metric_module():
    """dense_reference is a batched copy of metric.py; pin them together."""
    tri = random_oblique_triangle(RNG)
    values = RNG.random((9, 9))
    field = BilinearInterpolant(values, scale=0.3)
    pts = RNG.random((6, 2)) * 0.9 + 0.05
    f = pointwise_fields(tri, values, 0.3, pts[:, 0], pts[:, 1])
    for k, (u, v) in enumerate(pts):
        G = metric_at(tri, field, u, v)
        assert np.allclose([f["G00"][k], f["G01"][k], f["G11"][k]],
                           [G[0, 0], G[0, 1], G[1, 1]], atol=1e-10)
        lam = np.linalg.eigvals(np.linalg.solve(tri.G0, G))
        assert np.isclose(f["lam_min"][k], lam.real.min(), atol=1e-8)
        assert np.isclose(f["lam_max"][k], lam.real.max(), atol=1e-8)
