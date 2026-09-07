"""Direct evaluation of the displaced surface, for validating the metric.

Three reference computations against which metric.py is checked:
- S(u, v) = P + h*N evaluated pointwise;
- the metric by central finite differences of S;
- the metric of the piecewise-linear tessellated surface (micro-triangle
  Gram matrices), which is what a renderer actually draws (master plan,
  limitation 2).
"""

import numpy as np


def surface_at(tri, field, u, v):
    return tri.P(u, v) + field.h(u, v) * tri.N(u, v)


def fd_metric(tri, field, u, v, eps=1e-5):
    """Metric by central finite differences of S. Error is O(eps^2) plus
    roundoff ~ eps^-2 * machine epsilon; the sweet spot is around 1e-5."""
    Su = (surface_at(tri, field, u + eps, v) - surface_at(tri, field, u - eps, v)) / (2 * eps)
    Sv = (surface_at(tri, field, u, v + eps) - surface_at(tri, field, u, v - eps)) / (2 * eps)
    return np.array([[Su @ Su, Su @ Sv],
                     [Sv @ Su, Sv @ Sv]])


def tessellation_grid(n):
    """Uniform barycentric grid on the triangle domain at subdivision n.

    Returns (params, faces): params is (K, 2) with rows (i/n, j/n) for
    i + j <= n; faces is (T, 3) indices, alternating up/down micro-triangles.
    """
    index = {}
    params = []
    for j in range(n + 1):
        for i in range(n + 1 - j):
            index[(i, j)] = len(params)
            params.append((i / n, j / n))
    faces = []
    for j in range(n):
        for i in range(n - j):
            faces.append((index[(i, j)], index[(i + 1, j)], index[(i, j + 1)]))
            if i + j < n - 1:
                faces.append((index[(i + 1, j)], index[(i + 1, j + 1)], index[(i, j + 1)]))
    return np.asarray(params), np.asarray(faces, dtype=np.int64)


def grid_index(n, i, j):
    """Index of lattice node (i/n, j/n) in tessellation_grid(n)'s ordering
    (row j holds n+1-j nodes)."""
    return j * (n + 1) - j * (j - 1) // 2 + i


def tessellation_metrics(tri, field, n):
    """Per-micro-face metric of the piecewise-linear tessellated surface,
    expressed in the (u, v) coordinates of the base triangle.

    For a micro-face with parameter corners b0, b1, b2 and displaced 3D
    corners S0, S1, S2, the linear surface map has differential
    dS = [E1 E2] F^-1 with E_k = S_k - S0 and F = [b1-b0, b2-b0], so its
    metric is G_pl = F^-T [E_i . E_j] F^-1.

    Returns (G_pl, centroids): (T, 2, 2) and (T, 2).
    """
    params, faces = tessellation_grid(n)
    S = np.array([surface_at(tri, field, u, v) for (u, v) in params])

    G_pl = np.empty((len(faces), 2, 2))
    centroids = np.empty((len(faces), 2))
    for t, (i0, i1, i2) in enumerate(faces):
        b0, b1, b2 = params[i0], params[i1], params[i2]
        E = np.column_stack([S[i1] - S[i0], S[i2] - S[i0]])        # 3x2
        F = np.column_stack([b1 - b0, b2 - b0])                    # 2x2
        Finv = np.linalg.inv(F)
        G_pl[t] = Finv.T @ (E.T @ E) @ Finv
        centroids[t] = (b0 + b1 + b2) / 3.0
    return G_pl, centroids
