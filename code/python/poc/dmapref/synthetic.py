"""Synthetic triangles with controlled normal fields, for tests and
experiments. Each regime row of metric note §4 has a factory here."""

import numpy as np

from .mesh import Triangle


def random_oblique_triangle(rng, tilt=0.5):
    """Random triangle with vertex normals tilted off the face normal:
    the interpolated-normal regime (a != 0, B0, C0 != 0)."""
    q = rng.normal(size=(3, 3))
    n_face = np.cross(q[1] - q[0], q[2] - q[0])
    n_face = n_face / np.linalg.norm(n_face)
    m = []
    for _ in range(3):
        v = n_face + tilt * rng.normal(size=3)
        m.append(v / np.linalg.norm(v))
    return Triangle(q[0], q[1], q[2], m[0], m[1], m[2])


def face_normal_triangle(rng):
    """All vertex normals equal to the face normal: a = 0, B0 = C0 = 0
    (the classical height-field regime)."""
    q = rng.normal(size=(3, 3))
    n_face = np.cross(q[1] - q[0], q[2] - q[0])
    n_face = n_face / np.linalg.norm(n_face)
    return Triangle(q[0], q[1], q[2], n_face, n_face, n_face)


def sphere_triangle(rng, R=2.0):
    """Vertices on a sphere with radial normals: the normal field is a
    legitimate Gauss map (of the sphere), so the integrability defect is
    exactly zero (obliquity note §2)."""
    q = rng.normal(size=(3, 3))
    q = R * q / np.linalg.norm(q, axis=1, keepdims=True)
    m = q / R
    return Triangle(q[0], q[1], q[2], m[0], m[1], m[2])
