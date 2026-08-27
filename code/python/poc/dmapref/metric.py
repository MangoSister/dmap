"""The induced metric of a displaced surface.

Implements "The induced metric of a displaced surface" §3 (master formula)
and §6 (pseudocode). For S = P + h*N with unit N:

    G = G0 - 2h*B0 + h^2*C0 + gh*gh^T + gh*a^T + a*gh^T

where gh = (h_u, h_v) and the coefficients are pure base geometry. Two traps
from the notes, both honored here:
- B0 must be the symmetrized matrix (a Gram matrix is symmetric);
- C0 is computed directly from Nu, Nv, never from B0: the Weingarten
  identity C0 = B0 G0^-1 B0 fails for interpolated vertex normals
  ("Obliquity and the integrability defect" §3).
"""

import numpy as np


def normal_frame_at(tri, u, v):
    """Unit direction N and its parameter derivatives Nu, Nv at (u, v).

    N = M/|M| with M the interpolated vertex normal; differentiating the
    normalization gives Nu = (I - N N^T) Mu / |M| (projector form).
    """
    M = tri.M(u, v)
    Mlen = np.linalg.norm(M)
    N = M / Mlen
    Nu = (tri.Mu - N * (N @ tri.Mu)) / Mlen
    Nv = (tri.Mv - N * (N @ tri.Mv)) / Mlen
    return N, Nu, Nv


def base_forms_at(tri, u, v):
    """Base coefficients (G0, B0, C0, a) of the master formula at (u, v)."""
    N, Nu, Nv = normal_frame_at(tri, u, v)
    e1, e2 = tri.e1, tri.e2
    Bt = -np.array([[e1 @ Nu, e1 @ Nv],
                    [e2 @ Nu, e2 @ Nv]])   # unsymmetrized B~; its skew part is delta
    B0 = 0.5 * (Bt + Bt.T)
    C0 = np.array([[Nu @ Nu, Nu @ Nv],
                   [Nv @ Nu, Nv @ Nv]])
    a = np.array([e1 @ N, e2 @ N])
    return tri.G0, B0, C0, a


def offset_metric(G0, B0, C0, h):
    """Q(h): the metric of the constant-offset surface at height h."""
    return G0 - 2.0 * h * B0 + h * h * C0


def metric_from_forms(G0, B0, C0, a, h, gh):
    """The master formula."""
    gh = np.asarray(gh, dtype=np.float64)
    return (offset_metric(G0, B0, C0, h)
            + np.outer(gh, gh) + np.outer(gh, a) + np.outer(a, gh))


def metric_at(tri, field, u, v):
    """Metric at (u, v) for a displacement field over the identity chart
    (field coordinates = triangle coordinates)."""
    G0, B0, C0, a = base_forms_at(tri, u, v)
    return metric_from_forms(G0, B0, C0, a, field.h(u, v), field.grad(u, v))


# --- determinant identities (metric note §5) --------------------------------

def det2(A):
    return A[0, 0] * A[1, 1] - A[0, 1] * A[1, 0]


def det_metric_rank_one(Q, gh):
    """det G by the matrix determinant lemma. Valid only when a = 0
    (the update G - Q = gh gh^T is then rank one)."""
    return det2(Q) * (1.0 + gh @ np.linalg.solve(Q, gh))


def det_metric_sylvester(Q, gh, a):
    """det G by Sylvester's identity for the rank-two update
    G = Q + U C U^T with U = [gh a] and C = [[1, 1], [1, 0]]."""
    U = np.column_stack([gh, a])
    C = np.array([[1.0, 1.0], [1.0, 0.0]])
    return det2(Q) * det2(np.eye(2) + C @ (U.T @ np.linalg.solve(Q, U)))


def det_offset_quartic_coeffs(G0, B0, C0):
    """Coefficients c[0..4] of det Q(h) = sum_k c[k] h^k.

    Uses the 2x2 bilinear expansion det(X + Y) = det X + det Y + m(X, Y)
    with m(X, Y) = X00*Y11 + X11*Y00 - X01*Y10 - X10*Y01, applied to
    Q = G0 + h*(-2 B0) + h^2*C0.
    """
    def m(X, Y):
        return (X[0, 0] * Y[1, 1] + X[1, 1] * Y[0, 0]
                - X[0, 1] * Y[1, 0] - X[1, 0] * Y[0, 1])

    B = -2.0 * B0
    return np.array([
        det2(G0),
        m(G0, B),
        det2(B) + m(G0, C0),
        m(B, C0),
        det2(C0),
    ])
