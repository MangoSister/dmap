"""Cotangent Laplacian from edge lengths alone.

"Laplace-Beltrami on displaced surfaces" §2: the cotan Laplacian is
intrinsic; weights depend only on edge lengths, and lengths come from the
metric via l^2 = e^T G e. Both substrates share one assembly:

- Path A (tessellated): per-face G is the Gram matrix of the piecewise-
  linear micro-triangle (reference_surface.tessellation_metrics);
- Path B (tessellation-free): per-face G is the master formula evaluated at
  the face centroid, or per edge at midpoints.

With a positive-definite per-face G the three lengths always form a valid
triangle (they are lengths of an actual linear embedding), so the
triangle-inequality hazard of the note appears only with per-edge sampling.
Negative cotan weights (metric-obtuse faces) can appear in both modes.
"""

import numpy as np
import scipy.sparse as sp


def metric_edge_lengths_per_face(params, faces, G_faces):
    """Edge lengths (la, lb, lc) opposite corners (A, B, C), from one
    constant metric per face. Returns (T, 3)."""
    lengths = np.empty((len(faces), 3))
    for t, (A, B, C) in enumerate(faces):
        G = G_faces[t]
        for k, (p, q) in enumerate(((B, C), (C, A), (A, B))):
            e = params[q] - params[p]
            lengths[t, k] = np.sqrt(e @ G @ e)
    return lengths


def metric_edge_lengths_per_edge(params, faces, metric_fn):
    """Edge lengths with G sampled at each edge midpoint (the note's remedy
    for sharp metric variation). metric_fn(u, v) -> 2x2. Returns (T, 3)."""
    lengths = np.empty((len(faces), 3))
    for t, (A, B, C) in enumerate(faces):
        for k, (p, q) in enumerate(((B, C), (C, A), (A, B))):
            mid = 0.5 * (params[p] + params[q])
            e = params[q] - params[p]
            G = metric_fn(mid[0], mid[1])
            lengths[t, k] = np.sqrt(e @ G @ e)
    return lengths


def heron_area(la, lb, lc):
    """Triangle area from lengths; None if the triangle inequality fails."""
    s = 0.5 * (la + lb + lc)
    arg = s * (s - la) * (s - lb) * (s - lc)
    return np.sqrt(arg) if arg > 0.0 else None


def cotans_from_lengths(la, lb, lc, area):
    """Cotangents of the angles at (A, B, C); law of cosines
    (Laplace-Beltrami note §2 shared core)."""
    return np.array([
        (lb * lb + lc * lc - la * la),
        (lc * lc + la * la - lb * lb),
        (la * la + lb * lb - lc * lc),
    ]) / (4.0 * area)


def cotan_assembly(n_verts, faces, lengths):
    """Stiffness L (negative semi-definite convention: L[p,p] = -sum of
    weights) and lumped mass M from per-face edge lengths.

    Faces whose lengths violate the triangle inequality are skipped and
    counted; negative cotan weights are counted but kept (the reference
    reports hazards, it does not hide them).

    Returns (L, M, stats).
    """
    rows, cols, vals = [], [], []
    mass = np.zeros(n_verts)
    stats = {"invalid_triangles": 0, "negative_weights": 0}

    for t, (A, B, C) in enumerate(faces):
        la, lb, lc = lengths[t]
        area = heron_area(la, lb, lc)
        if area is None:
            stats["invalid_triangles"] += 1
            continue
        cotA, cotB, cotC = cotans_from_lengths(la, lb, lc, area)
        stats["negative_weights"] += int(cotA < 0) + int(cotB < 0) + int(cotC < 0)

        # Each angle's cotan weights the opposite edge.
        for (p, q, w) in ((B, C, 0.5 * cotA), (C, A, 0.5 * cotB), (A, B, 0.5 * cotC)):
            rows += [p, q, p, q]
            cols += [q, p, p, q]
            vals += [w, w, -w, -w]

        for p in (A, B, C):
            mass[p] += area / 3.0

    L = sp.csr_matrix((vals, (rows, cols)), shape=(n_verts, n_verts))
    M = sp.diags(mass)
    return L, M, stats
