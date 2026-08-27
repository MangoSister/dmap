"""Cotan Laplacian and heat method sanity on the identity metric."""

import numpy as np

from dmapref.heat_method import solve_heat_method
from dmapref.laplacian import cotan_assembly, metric_edge_lengths_per_face
from dmapref.reference_surface import grid_index, tessellation_grid


def identity_lattice(n):
    params, faces = tessellation_grid(n)
    G_faces = np.tile(np.eye(2), (len(faces), 1, 1))
    return params, faces, G_faces


def test_grid_index():
    n = 8
    params, _ = tessellation_grid(n)
    for (i, j) in [(0, 0), (3, 2), (0, 8), (8, 0)]:
        assert np.allclose(params[grid_index(n, i, j)], (i / n, j / n))


def test_identity_metric_annihilates_linear_functions():
    """With G = I the lattice is a flat triangulation of the parameter
    triangle; the cotan Laplacian of a linear function is zero at interior
    vertices."""
    n = 10
    params, faces, G_faces = identity_lattice(n)
    lengths = metric_edge_lengths_per_face(params, faces, G_faces)
    L, M, stats = cotan_assembly(len(params), faces, lengths)
    assert stats["invalid_triangles"] == 0

    f = 0.3 + 1.7 * params[:, 0] - 0.9 * params[:, 1]
    residual = L @ f
    interior = [grid_index(n, i, j)
                for j in range(1, n) for i in range(1, n - j)]
    assert np.max(np.abs(residual[interior])) < 1e-12


def test_lumped_mass_totals_domain_area():
    params, faces, G_faces = identity_lattice(8)
    lengths = metric_edge_lengths_per_face(params, faces, G_faces)
    _, M, _ = cotan_assembly(len(params), faces, lengths)
    assert np.isclose(M.diagonal().sum(), 0.5)  # area of the unit triangle


def test_heat_method_identity_metric_approximates_euclidean():
    """On the flat identity metric, geodesic distance is plain Euclidean
    distance in parameter space. The toy solver should be within a few
    percent away from the boundary."""
    n = 24
    params, faces, G_faces = identity_lattice(n)
    source = grid_index(n, 0, 0)
    phi, stats = solve_heat_method(params, faces, G_faces, source)
    assert stats["invalid_triangles"] == 0

    exact = np.linalg.norm(params - params[source], axis=1)
    mask = exact > 0.2   # skip the source neighborhood where t smoothing dominates
    rel = np.abs(phi[mask] - exact[mask]) / exact[mask]
    assert np.median(rel) < 0.05
