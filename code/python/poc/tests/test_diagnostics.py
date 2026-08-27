"""Diagnostics sanity checks (obliquity note §2 and §4)."""

import numpy as np

from dmapref.diagnostics import integrability_delta, obliquity_sin2theta
from dmapref.synthetic import (face_normal_triangle, random_oblique_triangle,
                               sphere_triangle)

RNG = np.random.default_rng(3)


def test_sphere_delta_is_zero():
    tri = sphere_triangle(RNG)
    for (u, v) in [(1 / 3, 1 / 3), (0.1, 0.2), (0.6, 0.3)]:
        assert abs(integrability_delta(tri, u, v, normalized=False)) < 1e-12


def test_face_normal_obliquity_is_zero():
    tri = face_normal_triangle(RNG)
    assert abs(obliquity_sin2theta(tri, 1 / 3, 1 / 3)) < 1e-14


def test_obliquity_in_unit_interval():
    for _ in range(10):
        tri = random_oblique_triangle(RNG)
        s = obliquity_sin2theta(tri, 1 / 3, 1 / 3)
        assert 0.0 <= s <= 1.0
