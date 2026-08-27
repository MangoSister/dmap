"""Mini heat method for geodesic distance (Crane et al. 2013).

Three steps on a lattice over the triangle parameter domain, with one
constant 2x2 metric per face (see laplacian.py for where G comes from):

  I.   heat:    solve (M - t*L) u = delta_source
  II.  direct:  X = -grad(u) / |grad(u)|_G   per face
  III. distance: solve L phi = div(X), phi(source) = 0

Gradients and divergence are metric-aware. For a function linear on a
parameter-space face with corners b0, b1, b2, the covariant gradient is
g = F^-T (u1-u0, u2-u0) with F = [b1-b0, b2-b0]; the contravariant vector
is G^-1 g, and |grad u|_G^2 = g^T G^-1 g. The divergence at a vertex uses
Crane et al.'s cotan formula with inner products <e, X> = e^T G X.
"""

import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as spla

from .laplacian import (cotan_assembly, cotans_from_lengths, heron_area,
                        metric_edge_lengths_per_face)


def face_gradient(params, face, values, G):
    """Contravariant gradient components of a linear function on one face."""
    A, B, C = face
    F = np.column_stack([params[B] - params[A], params[C] - params[A]])
    g_cov = np.linalg.solve(F.T, np.array([values[B] - values[A],
                                           values[C] - values[A]]))
    return np.linalg.solve(G, g_cov), g_cov


def solve_heat_method(params, faces, G_faces, source, t_factor=1.0):
    """Geodesic distance from a source vertex. Returns (phi, stats)."""
    n_verts = len(params)
    lengths = metric_edge_lengths_per_face(params, faces, G_faces)
    L, M, stats = cotan_assembly(n_verts, faces, lengths)

    # Step I: one backward-Euler heat step; t = (mean edge length)^2.
    t = t_factor * float(lengths.mean()) ** 2
    delta = np.zeros(n_verts)
    delta[source] = 1.0
    u = spla.spsolve((M - t * L).tocsc(), delta)

    # Step II: normalized negative gradient per face.
    X = np.zeros((len(faces), 2))
    for f, face in enumerate(faces):
        X_con, g_cov = face_gradient(params, face, u, G_faces[f])
        norm = np.sqrt(max(g_cov @ X_con, 0.0))
        if norm > 0.0:
            X[f] = -X_con / norm

    # Step III: integrated divergence, then the Poisson solve.
    div = np.zeros(n_verts)
    for f, (A, B, C) in enumerate(faces):
        G = G_faces[f]
        la, lb, lc = lengths[f]
        area = heron_area(la, lb, lc)
        if area is None:
            continue
        cot = cotans_from_lengths(la, lb, lc, area)  # angles at (A, B, C)
        corners = (A, B, C)
        for i in range(3):
            j, k = corners[(i + 1) % 3], corners[(i + 2) % 3]
            e1 = params[j] - params[corners[i]]   # edge opposite angle at k
            e2 = params[k] - params[corners[i]]   # edge opposite angle at j
            cot1 = cot[(i + 2) % 3]
            cot2 = cot[(i + 1) % 3]
            div[corners[i]] += 0.5 * (cot1 * (e1 @ G @ X[f]) + cot2 * (e2 @ G @ X[f]))

    phi = _solve_pinned(L, div, source)

    # Distance increases away from the source; fix the overall sign if the
    # solve returned the decreasing branch.
    if phi.mean() < 0.0:
        phi = -phi
    return phi, stats


def _solve_pinned(L, b, pin):
    """Solve L x = b with x[pin] = 0 (removes the constant nullspace)."""
    n = L.shape[0]
    keep = np.ones(n, dtype=bool)
    keep[pin] = False
    Lk = L[keep][:, keep].tocsc()
    x = np.zeros(n)
    x[keep] = spla.spsolve(Lk, b[keep])
    return x
