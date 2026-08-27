"""Sanity checks from the concept notes, as unit tests."""

import numpy as np

from dmapref import displacement
from dmapref.metric import (base_forms_at, det2, det_metric_rank_one,
                            det_metric_sylvester, det_offset_quartic_coeffs,
                            metric_at, metric_from_forms, normal_frame_at,
                            offset_metric)
from dmapref.reference_surface import fd_metric
from dmapref.synthetic import face_normal_triangle, random_oblique_triangle

RNG = np.random.default_rng(7)


def test_normal_frame_is_orthogonal():
    tri = random_oblique_triangle(RNG)
    N, Nu, Nv = normal_frame_at(tri, 0.3, 0.4)
    assert abs(np.linalg.norm(N) - 1) < 1e-14
    assert abs(N @ Nu) < 1e-14
    assert abs(N @ Nv) < 1e-14


def test_face_normal_case():
    """Metric note §4: over a flat base with a fixed normal, area depends on
    slope, not height, and det G = det G0 (1 + gh^T G0^-1 gh)."""
    tri = face_normal_triangle(RNG)
    G0, B0, C0, a = base_forms_at(tri, 0.2, 0.3)
    assert np.allclose(B0, 0) and np.allclose(C0, 0) and np.allclose(a, 0)

    gh = np.array([0.4, -0.7])
    G_low = metric_from_forms(G0, B0, C0, a, h=0.0, gh=gh)
    G_high = metric_from_forms(G0, B0, C0, a, h=5.0, gh=gh)
    assert np.allclose(G_low, G_high)  # h drops out completely

    expected = det2(G0) * (1 + gh @ np.linalg.solve(G0, gh))
    assert np.isclose(det2(G_low), expected)


def test_determinant_identities_agree():
    tri = random_oblique_triangle(RNG)
    G0, B0, C0, a = base_forms_at(tri, 0.25, 0.35)
    h, gh = 0.13, np.array([0.5, -0.2])
    Q = offset_metric(G0, B0, C0, h)
    G = metric_from_forms(G0, B0, C0, a, h, gh)

    # Sylvester handles the full rank-two update.
    assert np.isclose(det_metric_sylvester(Q, gh, a), det2(G), rtol=1e-12)

    # With a = 0 the rank-one lemma applies and all three agree.
    G_perp = metric_from_forms(G0, B0, C0, np.zeros(2), h, gh)
    assert np.isclose(det_metric_rank_one(Q, gh), det2(G_perp), rtol=1e-12)
    assert np.isclose(det_metric_sylvester(Q, gh, np.zeros(2)), det2(G_perp), rtol=1e-12)


def test_offset_determinant_quartic():
    tri = random_oblique_triangle(RNG)
    G0, B0, C0, _ = base_forms_at(tri, 0.3, 0.3)
    c = det_offset_quartic_coeffs(G0, B0, C0)
    for h in (-0.4, 0.0, 0.17, 0.8):
        direct = det2(offset_metric(G0, B0, C0, h))
        assert np.isclose(np.polyval(c[::-1], h), direct, rtol=1e-12)


def test_metric_matches_finite_differences():
    tri = random_oblique_triangle(RNG)
    field = displacement.sinusoid(amp=0.2)
    for (u, v) in [(0.2, 0.3), (0.5, 0.1), (0.1, 0.6)]:
        G = metric_at(tri, field, u, v)
        G_fd = fd_metric(tri, field, u, v, eps=1e-6)
        assert np.allclose(G, G_fd, rtol=1e-6, atol=1e-9)
