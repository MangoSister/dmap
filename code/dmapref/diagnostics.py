"""Pre-bake base-mesh diagnostics.

From "Obliquity and the integrability defect" §4:
- obliquity sin^2(theta) = a^T G0^-1 a, dimensionless, in [0, 1];
- integrability defect delta = P_u . N_v - P_v . N_u, normalized by
  sqrt(det G0) before comparison across triangles of different size.

Both need only vertex positions and vertex normals; no displacement data.
"""

import numpy as np

from .metric import normal_frame_at, det2

BARYCENTER = (1.0 / 3.0, 1.0 / 3.0)


def obliquity_sin2theta(tri, u, v):
    N, _, _ = normal_frame_at(tri, u, v)
    a = np.array([tri.e1 @ N, tri.e2 @ N])
    return a @ np.linalg.solve(tri.G0, a)


def integrability_delta(tri, u, v, normalized=True):
    _, Nu, Nv = normal_frame_at(tri, u, v)
    delta = tri.e1 @ Nv - tri.e2 @ Nu
    if normalized:
        delta = delta / np.sqrt(det2(tri.G0))
    return delta


def mesh_diagnostics(mesh):
    """Per-triangle diagnostics at the barycenter: (sin2theta, delta_norm)."""
    u, v = BARYCENTER
    sin2 = np.empty(mesh.n_triangles)
    delta = np.empty(mesh.n_triangles)
    for t in range(mesh.n_triangles):
        tri = mesh.triangle(t)
        sin2[t] = obliquity_sin2theta(tri, u, v)
        delta[t] = integrability_delta(tri, u, v)
    return sin2, delta
