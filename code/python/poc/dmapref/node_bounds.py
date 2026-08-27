"""Propagation of a pyramid node to certified metric bounds.

Implements "Taylor-model bound pyramid" §6: substitute the node's affine
forms of (h, h_u, h_v) into the master formula

    G = G0 - 2h*B0 + h^2*C0 + gh*gh^T + gh*a^T + a*gh^T

and read off certified intervals for the entries of G, det G, sqrt(det G),
the eigenvalues of G0^{-1} G, and a normal cone. The base forms vary over
the cell through the interpolated normal, so they enter as entrywise
interval enclosures (CellEnclosure); the h-∇h correlation, which is the
node's contribution, lives entirely in the shared affine symbols.

Everything is batched: one call handles all cells of a pyramid level, with
scalars as arrays of shape (...,), vectors (..., 3), 2x2 forms as four
scalar entries.

Ablation variants (master plan §7, Phase 1) share the two propagation
routines and differ only in how (h, h_u, h_v) are modelled:
  A0 taylor_forms      + propagate_affine    -- ours
  A1 min-max channels  + propagate_interval  -- independent interval storage
  A2 taylor_intervals  + propagate_interval  -- Taylor node collapsed to
                                               intervals (isolates shared
                                               symbols in propagation)
  A3 boxed_forms       + propagate_affine    -- interval storage, affine
                                               propagation, no shared plane
                                               (isolates the joint storage)
"""

from dataclasses import dataclass

import numpy as np

from .affine import (AffineForm, EPS_GU, EPS_GV, EPS_H, EPS_U, EPS_V,
                     affine_sqrt, iv_add, iv_center_rad, iv_const, iv_from_cr,
                     iv_mul, iv_recip, iv_scale, iv_square, iv_sqrt, iv_sub)


# --- enclosure of the base forms over a cell --------------------------------

@dataclass
class CellEnclosure:
    """Entrywise (center, radius) enclosures of the base forms over a cell.

    G0 is exact (Pu = e1, Pv = e2 are constant on a flat triangle). B0, C0,
    a, N, Nu, Nv vary through the interpolated normal; M(u, v) is linear in
    (u, v), so conservative component intervals follow from interval
    arithmetic on M and |M|.
    """

    G0: np.ndarray          # (2, 2) exact
    G0inv: np.ndarray       # (2, 2) exact
    e1: np.ndarray          # (3,) exact
    e2: np.ndarray          # (3,) exact
    B0c: dict               # {(i,j): array} centers, entries 00, 01, 11
    B0r: dict
    C0c: dict
    C0r: dict
    ac: np.ndarray          # (2, ...) centers of a
    ar: np.ndarray
    Nc: np.ndarray          # (3, ...) component centers of N
    Nr: np.ndarray
    Nuc: np.ndarray
    Nur: np.ndarray
    Nvc: np.ndarray
    Nvr: np.ndarray


def cell_enclosure(tri, U0, V0, su, sv):
    """CellEnclosure for cells centred at (U0, V0) with half-extents
    (su, sv). U0, V0 are arrays of a common batch shape."""
    U0 = np.asarray(U0, dtype=np.float64)
    V0 = np.asarray(V0, dtype=np.float64)
    Mu, Mv = tri.Mu, tri.Mv
    col = (3,) + (1,) * U0.ndim                   # shape for constant 3-vectors

    # M is affine in (u, v): exact component centers, and componentwise plus
    # Euclidean deviation bounds over the cell.
    Mc = (tri.m0.reshape(col) + Mu.reshape(col) * U0 + Mv.reshape(col) * V0)
    comp_rad = np.broadcast_to(
        (su * np.abs(Mu) + sv * np.abs(Mv)).reshape(col), Mc.shape)
    M_iv = iv_from_cr(Mc, comp_rad)

    R = su * np.linalg.norm(Mu) + sv * np.linalg.norm(Mv)
    Mlen_c = np.sqrt((Mc ** 2).sum(axis=0))
    Mlen_iv = (np.maximum(Mlen_c - R, 1e-12), Mlen_c + R)
    inv_len = iv_recip(Mlen_iv)

    # N = M / |M|, per component.
    N_iv = iv_mul(M_iv, _bcast_iv(inv_len, 3))

    # Nu = (Mu - N (N·Mu)) / |M|; same for Nv (metric.py projector form).
    def deriv_iv(Md):
        dot = _iv_dot_const(N_iv, Md)
        proj = iv_mul(N_iv, _bcast_iv(dot, 3))
        Md_iv = iv_const(np.broadcast_to(Md.reshape(col), Mc.shape))
        return iv_mul(iv_sub(Md_iv, proj), _bcast_iv(inv_len, 3))

    Nu_iv = deriv_iv(Mu)
    Nv_iv = deriv_iv(Mv)

    # B~ = -[[e1·Nu, e1·Nv], [e2·Nu, e2·Nv]]; B0 = sym(B~) (metric.py traps).
    e1, e2 = tri.e1, tri.e2
    bt = {(0, 0): iv_scale(_iv_dot_const(Nu_iv, e1), -1.0),
          (0, 1): iv_scale(_iv_dot_const(Nv_iv, e1), -1.0),
          (1, 0): iv_scale(_iv_dot_const(Nu_iv, e2), -1.0),
          (1, 1): iv_scale(_iv_dot_const(Nv_iv, e2), -1.0)}
    B0_iv = {(0, 0): bt[(0, 0)],
             (0, 1): iv_scale(iv_add(bt[(0, 1)], bt[(1, 0)]), 0.5),
             (1, 1): bt[(1, 1)]}

    C0_iv = {(0, 0): _iv_dot(Nu_iv, Nu_iv, square=True),
             (0, 1): _iv_dot(Nu_iv, Nv_iv),
             (1, 1): _iv_dot(Nv_iv, Nv_iv, square=True)}

    a_iv = [_iv_dot_const(N_iv, e1), _iv_dot_const(N_iv, e2)]

    B0c, B0r = _cr_dict(B0_iv)
    C0c, C0r = _cr_dict(C0_iv)
    ac, ar = np.stack([iv_center_rad(a_iv[k])[0] for k in range(2)]), \
        np.stack([iv_center_rad(a_iv[k])[1] for k in range(2)])
    Nc, Nr = iv_center_rad(N_iv)
    Nuc, Nur = iv_center_rad(Nu_iv)
    Nvc, Nvr = iv_center_rad(Nv_iv)

    return CellEnclosure(tri.G0, np.linalg.inv(tri.G0), e1, e2,
                         B0c, B0r, C0c, C0r, ac, ar,
                         Nc, Nr, Nuc, Nur, Nvc, Nvr)


def _bcast_iv(iv, k):
    """Broadcast a scalar-batch interval to k vector components."""
    lo, hi = iv
    return (np.broadcast_to(lo, (k,) + lo.shape).copy(),
            np.broadcast_to(hi, (k,) + hi.shape).copy())


def _iv_dot_const(vec_iv, c):
    """Dot of a component-interval 3-vector with a constant 3-vector."""
    lo, hi = vec_iv
    out_lo = np.zeros(lo.shape[1:])
    out_hi = np.zeros(lo.shape[1:])
    for k in range(3):
        t = iv_scale((lo[k], hi[k]), c[k])
        out_lo, out_hi = out_lo + t[0], out_hi + t[1]
    return out_lo, out_hi


def _iv_dot(x_iv, y_iv, square=False):
    """Dot of two component-interval 3-vectors."""
    out = None
    for k in range(3):
        xk = (x_iv[0][k], x_iv[1][k])
        yk = (y_iv[0][k], y_iv[1][k])
        t = iv_square(xk) if square else iv_mul(xk, yk)
        out = t if out is None else iv_add(out, t)
    return out


def _cr_dict(iv_dict):
    centers, rads = {}, {}
    for key, iv in iv_dict.items():
        centers[key], rads[key] = iv_center_rad(iv)
    return centers, rads


# --- (h, hu, hv) models: the ablation axis ----------------------------------

def taylor_forms(level, su, sv):
    """A0: the joint Taylor node as affine forms over shared symbols
    (pyramid note §2). The plane coefficients g appear in both h and the
    gradient forms; that shared appearance is the stored correlation."""
    h = AffineForm.from_symbol(level["h0"], {EPS_U: level["gu"] * su,
                                             EPS_V: level["gv"] * sv,
                                             EPS_H: level["r"]})
    hu = AffineForm.from_symbol(level["gu"], {EPS_GU: level["ru"]})
    hv = AffineForm.from_symbol(level["gv"], {EPS_GV: level["rv"]})
    return h, hu, hv


def taylor_intervals(level, su, sv):
    """A2: the Taylor node collapsed to independent intervals (recovery of
    note §5). Same storage as A0, no shared symbols in propagation."""
    half = np.abs(level["gu"]) * su + np.abs(level["gv"]) * sv + level["r"]
    return (iv_from_cr(level["h0"], half),
            iv_from_cr(level["gu"], level["ru"]),
            iv_from_cr(level["gv"], level["rv"]))


def boxed_forms(h_iv, hu_iv, hv_iv):
    """A3: independent interval channels promoted to affine forms with one
    private symbol each (TFDM-style). Each channel enters every term of the
    formula with one consistent value, but h and the gradient stay mutually
    uncorrelated."""
    hc, hr = iv_center_rad(h_iv)
    uc, ur = iv_center_rad(hu_iv)
    vc, vr = iv_center_rad(hv_iv)
    h = AffineForm.from_symbol(hc, {EPS_H: hr})
    hu = AffineForm.from_symbol(uc, {EPS_GU: ur})
    hv = AffineForm.from_symbol(vc, {EPS_GV: vr})
    return h, hu, hv


# --- propagation ------------------------------------------------------------

def propagate_affine(h, hu, hv, enc):
    """Certified bounds from affine (h, hu, hv) models (note §6),
    intersected with the plain interval evaluation of the same models.

    Both routes are valid enclosures of the same stored node, with opposite
    strengths: the affine route keeps correlation across products (the same
    deviation drawn consistently in det G = G11 G22 - G12^2), while exact
    interval squares are tighter than any centred form once a deviation
    straddles zero (x^2 >= 0 is invisible to an affine form). The
    intersection dominates each alone. Returns a dict of intervals plus the
    normal cone."""
    gh = [hu, hv]
    h2 = h.square()
    G = {}
    for (i, j) in [(0, 0), (0, 1), (1, 1)]:
        offset = (-2.0 * h.times_interval(enc.B0c[(i, j)], enc.B0r[(i, j)])
                  + h2.times_interval(enc.C0c[(i, j)], enc.C0r[(i, j)]))
        slope = gh[i].square() if i == j else gh[i] * gh[j]
        coupling = (gh[i].times_interval(enc.ac[j], enc.ar[j])
                    + gh[j].times_interval(enc.ac[i], enc.ar[i]))
        # AffineForm first: numpy scalar + AffineForm would build an object
        # array instead of dispatching to __radd__.
        G[(i, j)] = offset + slope + coupling + enc.G0[i, j]

    det = G[(0, 0)] * G[(1, 1)] - G[(0, 1)].square()
    det_iv = det.interval()

    out = {"G": {k: v.interval() for k, v in G.items()},
           "det": det_iv,
           "sqrt_det": iv_sqrt(det_iv)}
    weyl = _eigen_intervals(out["G"], enc)
    disc_route = _eigen_affine(G, det, enc)
    for key in ["lam_max", "lam_min"]:
        out[key] = _iv_intersect(weyl[key], disc_route[key])
    out["cone"] = _normal_cone_affine(h, hu, hv, enc)

    boxed = propagate_interval(h.interval(), hu.interval(), hv.interval(),
                               enc)
    for k in [(0, 0), (0, 1), (1, 1)]:
        out["G"][k] = _iv_intersect(out["G"][k], boxed["G"][k])
    for key in ["det", "sqrt_det", "lam_min", "lam_max"]:
        out[key] = _iv_intersect(out[key], boxed[key])
    _lam_division_refine(out, out["det"], enc)
    tighter = boxed["cone"]["half_angle"] < out["cone"]["half_angle"]
    out["cone"] = {
        "axis": np.where(tighter[None], boxed["cone"]["axis"],
                         out["cone"]["axis"]),
        "half_angle": np.where(tighter, boxed["cone"]["half_angle"],
                               out["cone"]["half_angle"])}
    return out


def _eigen_affine(G, det, enc):
    """Eigenvalues of G0^{-1} G through the closed 2x2 formula, all in
    affine arithmetic: T and the discriminant share symbols, and the affine
    sqrt keeps them, so lam- = (T - sqrt(disc))/2 cancels the correlated
    variation that pins the small eigenvalue (interval evaluation of the
    same formula loses it and comes out orders of magnitude wider)."""
    A = enc.G0inv
    T = (G[(0, 0)].scale(A[0, 0]) + G[(0, 1)].scale(2.0 * A[0, 1])
         + G[(1, 1)].scale(A[1, 1]))
    det_G0 = enc.G0[0, 0] * enc.G0[1, 1] - enc.G0[0, 1] * enc.G0[1, 0]
    D = det.scale(1.0 / det_G0)
    sd = affine_sqrt(T.square() - D.scale(4.0))
    return {"lam_max": (T + sd).scale(0.5).interval(),
            "lam_min": (T - sd).scale(0.5).interval()}


def _iv_intersect(x, y):
    """Intersection of two valid enclosures (still a valid enclosure)."""
    lo = np.maximum(x[0], y[0])
    hi = np.minimum(x[1], y[1])
    return lo, np.maximum(hi, lo)


def _lam_division_refine(out, det_iv, enc):
    """Refine the eigenvalue intervals through lam_min * lam_max =
    det G / det G0, where both factors are certified nonnegative. The
    determinant interval is the tightest product the propagation has, so
    the quotient can beat the direct eigenvalue enclosures."""
    det_G0 = enc.G0[0, 0] * enc.G0[1, 1] - enc.G0[0, 1] * enc.G0[1, 0]
    D_lo, D_hi = iv_scale(det_iv, 1.0 / det_G0)
    mn, mx = out["lam_min"], out["lam_max"]
    ok = (D_lo >= 0.0) & (mx[0] > 0.0) & (mn[0] > 0.0)
    mn_ref = (np.where(ok, D_lo / np.where(ok, mx[1], 1.0), -np.inf),
              np.where(ok, D_hi / np.where(ok, mx[0], 1.0), np.inf))
    mx_ref = (np.where(ok, D_lo / np.where(ok, mn[1], 1.0), -np.inf),
              np.where(ok, D_hi / np.where(ok, mn[0], 1.0), np.inf))
    out["lam_min"] = _iv_intersect(mn, mn_ref)
    out["lam_max"] = _iv_intersect(mx, mx_ref)


def propagate_interval(h_iv, hu_iv, hv_iv, enc):
    """Plain interval evaluation of the master formula (ablations A1, A2).
    Interval squares are exact, so this is interval arithmetic at its best;
    what it cannot do is keep one consistent worst case for the same
    gradient across the slope and coupling terms (pyramid note §1)."""
    gh = [hu_iv, hv_iv]
    h2 = iv_square(h_iv)
    G = {}
    for (i, j) in [(0, 0), (0, 1), (1, 1)]:
        B0 = iv_from_cr(enc.B0c[(i, j)], enc.B0r[(i, j)])
        C0 = iv_from_cr(enc.C0c[(i, j)], enc.C0r[(i, j)])
        ai = iv_from_cr(enc.ac[i], enc.ar[i])
        aj = iv_from_cr(enc.ac[j], enc.ar[j])
        slope = iv_square(gh[i]) if i == j else iv_mul(gh[i], gh[j])
        entry = iv_add(iv_scale(iv_mul(h_iv, B0), -2.0), iv_mul(h2, C0))
        entry = iv_add(entry, slope)
        entry = iv_add(entry, iv_add(iv_mul(gh[i], aj), iv_mul(gh[j], ai)))
        G[(i, j)] = iv_add((np.full_like(entry[0], enc.G0[i, j]),
                            np.full_like(entry[0], enc.G0[i, j])), entry)

    det_iv = iv_sub(iv_mul(G[(0, 0)], G[(1, 1)]), iv_square(G[(0, 1)]))
    out = {"G": G, "det": det_iv, "sqrt_det": iv_sqrt(det_iv)}
    out.update(_eigen_intervals(G, enc))
    _lam_division_refine(out, det_iv, enc)
    out["cone"] = _normal_cone_interval(h_iv, hu_iv, hv_iv, enc)
    return out


def _eigen_intervals(G_iv, enc):
    """Eigenvalues of G0^{-1} G by Weyl's inequality.

    G0^{-1} G is similar to the symmetric Gh = S G S with S = G0^{-1/2}, so
    for G = Gc + E with |E| <= Delta entrywise,

        lam_i(S G S) in lam_i(S Gc S) ± ||S E S||_2,

    and ||S E S||_2 <= || |S| Delta |S| ||_2 (entrywise bound, then the
    spectral norm is monotone on symmetric nonnegative matrices). The
    centre eigenvalues are exact closed forms, so there is no interval
    discriminant: the trace/determinant route squares T against 4D and the
    cancellation makes lam bounds orders of magnitude too wide."""
    Gc = {k: 0.5 * (v[0] + v[1]) for k, v in G_iv.items()}
    Gr = {k: 0.5 * (v[1] - v[0]) for k, v in G_iv.items()}
    S = _inv_sqrt_2x2(enc.G0)
    Sa = np.abs(S)

    def congruence(M, A):
        """(A M A) entries for symmetric 2x2 M given by a dict, A constant."""
        m00, m01, m11 = M[(0, 0)], M[(0, 1)], M[(1, 1)]
        r0 = [A[0, 0] * m00 + A[0, 1] * m01, A[0, 0] * m01 + A[0, 1] * m11]
        r1 = [A[1, 0] * m00 + A[1, 1] * m01, A[1, 0] * m01 + A[1, 1] * m11]
        return {(0, 0): r0[0] * A[0, 0] + r0[1] * A[1, 0],
                (0, 1): r0[0] * A[0, 1] + r0[1] * A[1, 1],
                (1, 1): r1[0] * A[0, 1] + r1[1] * A[1, 1]}

    Ghc = congruence(Gc, S)
    Dh = congruence(Gr, Sa)

    mean = 0.5 * (Ghc[(0, 0)] + Ghc[(1, 1)])
    half = np.sqrt(np.maximum(
        0.25 * (Ghc[(0, 0)] - Ghc[(1, 1)]) ** 2 + Ghc[(0, 1)] ** 2, 0.0))
    rho = (0.5 * (Dh[(0, 0)] + Dh[(1, 1)])
           + np.sqrt(np.maximum(
               0.25 * (Dh[(0, 0)] - Dh[(1, 1)]) ** 2 + Dh[(0, 1)] ** 2, 0.0)))
    return {"lam_max": (mean + half - rho, mean + half + rho),
            "lam_min": (mean - half - rho, mean - half + rho)}


def _inv_sqrt_2x2(G0):
    """Inverse square root of a symmetric positive definite 2x2 matrix."""
    lam, V = np.linalg.eigh(G0)
    return (V * (1.0 / np.sqrt(lam))) @ V.T


def cone_from_box(center, radius):
    """Half-angle of the direction cone of a vector box c ± r: any vector in
    the box makes cos(angle) >= (|c| - R)/(|c| + R) with the axis c, where
    R = |r|_2. The bound only holds while R < |c| (it needs c·n >= 0);
    a box whose deviation reaches the centre length can contain any
    direction and gets the full half-angle pi."""
    c_len = np.sqrt((center ** 2).sum(axis=0))
    R = np.sqrt((radius ** 2).sum(axis=0))
    cos_lo = np.where(R < c_len, (c_len - R) / np.maximum(c_len + R, 1e-300),
                      -1.0)
    axis = center / np.maximum(c_len, 1e-300)
    return {"axis": axis, "half_angle": np.arccos(np.clip(cos_lo, -1.0, 1.0))}


def _normal_cone_affine(h, hu, hv, enc):
    """Unnormalized normal n = Su x Sv as an affine 3-vector
    (Su = e1 + hu*N + h*Nu), then a cone around its centre direction.

    The deviation d = n - n0 is a sum of per-symbol 3-vectors, so
    |d| <= R1 = sum_k |coeff vector of symbol k| + |(rad_x, rad_y, rad_z)|,
    which is usually tighter than the componentwise box norm R2 (the box
    forgets that one symbol moves all three components together). The
    half-angle then comes from two valid bounds, intersected:
    cos >= (|n0| - R)/(|n0| + R), and, since n·axis >= |n0| - R > 0,
    sin <= R_perp / (|n0| - R) with R_perp the same sum taken on the
    components perpendicular to the axis."""
    Su = [hu.times_interval(enc.Nc[k], enc.Nr[k])
          + h.times_interval(enc.Nuc[k], enc.Nur[k]) + enc.e1[k]
          for k in range(3)]
    Sv = [hv.times_interval(enc.Nc[k], enc.Nr[k])
          + h.times_interval(enc.Nvc[k], enc.Nvr[k]) + enc.e2[k]
          for k in range(3)]
    n = [Su[1] * Sv[2] - Su[2] * Sv[1],
         Su[2] * Sv[0] - Su[0] * Sv[2],
         Su[0] * Sv[1] - Su[1] * Sv[0]]

    center = np.stack([f.x0 for f in n])                    # (3, ...)
    coeffs = np.stack([f.coeffs for f in n])                # (3, S, ...)
    rads = np.stack([f.rad for f in n])                     # (3, ...)
    c_len = np.sqrt((center ** 2).sum(axis=0))
    axis = center / np.maximum(c_len, 1e-300)

    R1 = (np.sqrt((coeffs ** 2).sum(axis=0)).sum(axis=0)
          + np.sqrt((rads ** 2).sum(axis=0)))
    dev_comp = np.abs(coeffs).sum(axis=1) + rads
    R2 = np.sqrt((dev_comp ** 2).sum(axis=0))
    R = np.minimum(R1, R2)

    perp = coeffs - axis[:, None] * (coeffs * axis[:, None]).sum(axis=0)
    R_perp = (np.sqrt((perp ** 2).sum(axis=0)).sum(axis=0)
              + np.sqrt((rads ** 2).sum(axis=0)))

    safe = R < c_len
    cos_lo = np.where(safe, (c_len - R) / np.maximum(c_len + R, 1e-300),
                      -1.0)
    ang_cos = np.arccos(np.clip(cos_lo, -1.0, 1.0))
    sin_hi = np.where(safe, R_perp / np.maximum(c_len - R, 1e-300), 1.0)
    ang_sin = np.where(safe & (sin_hi < 1.0),
                       np.arcsin(np.clip(sin_hi, 0.0, 1.0)), np.pi)
    return {"axis": axis, "half_angle": np.minimum(ang_cos, ang_sin)}


def _normal_cone_interval(h_iv, hu_iv, hv_iv, enc):
    def tangent_comp(e_k, w_iv, N_k, dNc_k, dNr_k):
        """One component of e + w*N + h*dN as an interval."""
        t = iv_add(iv_mul(w_iv, N_k), iv_mul(h_iv, iv_from_cr(dNc_k, dNr_k)))
        return t[0] + e_k, t[1] + e_k

    Su = [tangent_comp(enc.e1[k], hu_iv, iv_from_cr(enc.Nc[k], enc.Nr[k]),
                       enc.Nuc[k], enc.Nur[k]) for k in range(3)]
    Sv = [tangent_comp(enc.e2[k], hv_iv, iv_from_cr(enc.Nc[k], enc.Nr[k]),
                       enc.Nvc[k], enc.Nvr[k]) for k in range(3)]
    n = [iv_sub(iv_mul(Su[1], Sv[2]), iv_mul(Su[2], Sv[1])),
         iv_sub(iv_mul(Su[2], Sv[0]), iv_mul(Su[0], Sv[2])),
         iv_sub(iv_mul(Su[0], Sv[1]), iv_mul(Su[1], Sv[0]))]
    center = np.stack([iv_center_rad(c)[0] for c in n])
    radius = np.stack([iv_center_rad(c)[1] for c in n])
    return cone_from_box(center, radius)
