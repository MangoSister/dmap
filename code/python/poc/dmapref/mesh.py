"""OBJ loading and per-triangle base geometry.

The base primitive is a flat triangle with unit vertex normals. A point on
the base is P(u, v) = q0 + u*e1 + v*e2 with barycentric-style coordinates
(u, v) in the triangle domain {u >= 0, v >= 0, u + v <= 1}. The displacement
direction is the normalized interpolated vertex normal.

Per-triangle constants follow "The induced metric of a displaced surface" §6.
"""

from dataclasses import dataclass

import numpy as np


@dataclass
class Triangle:
    """One base triangle: vertices q0..q2 and unit vertex normals m0..m2."""

    q0: np.ndarray
    q1: np.ndarray
    q2: np.ndarray
    m0: np.ndarray
    m1: np.ndarray
    m2: np.ndarray

    def __post_init__(self):
        self.e1 = self.q1 - self.q0
        self.e2 = self.q2 - self.q0
        self.Mu = self.m1 - self.m0
        self.Mv = self.m2 - self.m0
        self.G0 = np.array([
            [self.e1 @ self.e1, self.e1 @ self.e2],
            [self.e2 @ self.e1, self.e2 @ self.e2],
        ])

    def P(self, u, v):
        """Base position at (u, v)."""
        return self.q0 + u * self.e1 + v * self.e2

    def M(self, u, v):
        """Interpolated (unnormalized) vertex normal at (u, v)."""
        return self.m0 + u * self.Mu + v * self.Mv

    def N(self, u, v):
        """Unit displacement direction at (u, v)."""
        M = self.M(u, v)
        return M / np.linalg.norm(M)


class Mesh:
    """Triangle mesh with positions and unit vertex normals."""

    def __init__(self, positions, normals, faces, face_normal_indices):
        self.positions = positions            # (V, 3)
        self.normals = normals                # (Vn, 3), unit
        self.faces = faces                    # (T, 3) vertex indices
        self.face_normal_indices = face_normal_indices  # (T, 3) into normals

    @property
    def n_triangles(self):
        return len(self.faces)

    def triangle(self, t):
        vi = self.faces[t]
        ni = self.face_normal_indices[t]
        return Triangle(
            self.positions[vi[0]], self.positions[vi[1]], self.positions[vi[2]],
            self.normals[ni[0]], self.normals[ni[1]], self.normals[ni[2]],
        )


def load_obj(path):
    """Load an OBJ file. Polygons are fan-triangulated.

    If the file has no vertex normals, area-weighted vertex normals are
    computed, and the normal indices equal the vertex indices.
    """
    positions, normals, faces, face_normal_indices = [], [], [], []

    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.split()
            if not parts:
                continue
            if parts[0] == "v":
                positions.append([float(x) for x in parts[1:4]])
            elif parts[0] == "vn":
                normals.append([float(x) for x in parts[1:4]])
            elif parts[0] == "f":
                corners = [_parse_corner(c) for c in parts[1:]]
                for k in range(1, len(corners) - 1):  # fan triangulation
                    tri = [corners[0], corners[k], corners[k + 1]]
                    faces.append([c[0] for c in tri])
                    face_normal_indices.append([c[2] for c in tri])

    positions = np.asarray(positions, dtype=np.float64)
    faces = np.asarray(faces, dtype=np.int64)

    if normals:
        normals = np.asarray(normals, dtype=np.float64)
        face_normal_indices = np.asarray(face_normal_indices, dtype=np.int64)
        if (face_normal_indices < 0).any():
            raise ValueError(f"{path}: has vn but faces lack normal indices")
    else:
        normals = _area_weighted_vertex_normals(positions, faces)
        face_normal_indices = faces.copy()

    normals = normals / np.linalg.norm(normals, axis=1, keepdims=True)
    return Mesh(positions, normals, faces, face_normal_indices)


def _parse_corner(token):
    """Parse an OBJ face corner 'v', 'v/t', 'v//n', or 'v/t/n' to 0-based
    (vertex, texcoord, normal) indices; missing entries are -1."""
    fields = token.split("/")
    v = int(fields[0]) - 1
    t = int(fields[1]) - 1 if len(fields) > 1 and fields[1] else -1
    n = int(fields[2]) - 1 if len(fields) > 2 and fields[2] else -1
    return (v, t, n)


def _area_weighted_vertex_normals(positions, faces):
    """Cross products summed per vertex; the cross-product length is twice
    the triangle area, which provides the area weighting for free."""
    normals = np.zeros_like(positions)
    for a, b, c in faces:
        n = np.cross(positions[b] - positions[a], positions[c] - positions[a])
        normals[a] += n
        normals[b] += n
        normals[c] += n
    return normals
