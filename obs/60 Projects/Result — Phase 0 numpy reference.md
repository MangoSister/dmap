---
title: Result — Phase 0 numpy reference
tags: [result, phase0, metric, implementation]
created: 2026-08-26
---

# Result — Phase 0 numpy reference

Phase 0 of [[Project — Conservative metric queries without tessellation]]: the numpy reference implementation, its verification, and the two decisions it was built to make. Code in `C:\dmap\code` (package `dmapref`, experiments in `experiments/`, figures in `experiments/out/`). All 16 unit tests pass; all four experiments pass.

## What was built

- `dmapref/metric.py`: the master formula, base forms per [[The induced metric of a displaced surface]] §6, and the determinant identities (rank-one, Sylvester, quartic det Q). All identities agree to machine precision (unit tests).
- `dmapref/laplacian.py` + `heat_method.py`: cotangent Laplacian from edge lengths alone, one implementation for both substrates ([[Laplace–Beltrami on displaced surfaces]] §2). Path A supplies per-face Gram metrics of the tessellated surface; Path B supplies the formula metric. A mini heat method (heat step, normalize, Poisson) runs on either.
- `dmapref/interpolant.py`: bilinear and biquadratic B-spline `h(u,v)` with analytic gradients.
- `dmapref/diagnostics.py`: obliquity and integrability defect ([[Obliquity and the integrability defect]] §4).
- Sanity checks from the notes are unit tests: face-normal case (h drops out of the metric), sphere with radial normals (δ = 0 to 1e-12), determinant identity agreement, cotan Laplacian annihilating linear functions on the identity metric.

## Experiment 1 — formula vs finite differences: PASS

Central finite differences of S = P + hN converge to the closed-form metric at order 2.00 (measured), reaching 1.2e-10 relative error, on 20 random oblique triangles with an analytic displacement. The formula is right.

## Experiment 2 — the limitation-2 price (smooth vs rendered)

Per-micro-face Gram metrics of the tessellated surface converge monotonically to the formula as tessellation refines, on both assets. The discrepancy at leaf scale (micro-face = texel), as relative area-element error:

| Asset | amplitude 0.05×edge | amplitude 0.2×edge |
|---|---|---|
| cc_torus + disp_rock | mean 0.3%, max 4.7% | mean 2.4%, max 25% |
| spot + disp_cobble | mean 1.3%, max 23% | mean 7.3%, max 100% |

Reading: at moderate displacement the smooth-surface metric prices the rendered surface to about 1%; at aggressive amplitude on a coarse base mesh (spot) the leaf-scale gap is real and must be stated, as the master plan's limitation 2 already does. The gap halves again at one level finer.

## Experiment 3 — interpolant decision: bilinear

Toy heat-method geodesic solve (Path B, formula metric at face centroids) against each interpolant's own 4×-tessellated ground truth, on three cc_torus triangles with disp_rock at 0.2×edge amplitude:

- Mean relative L2 distance error, lattices n = 8/16/32: bilinear 2.98% / 1.49% / 0.88%; B-spline 2.87% / 1.45% / 0.91%. Differences are under 5% of each other at every resolution, both converging.
- Triangle-inequality failures: zero for both (per-face constant metric sampling makes the lengths embeddable by construction; the hazard belongs to per-edge sampling).
- Negative cotan weights: present and comparable for both (~6% of angles at n = 32); accuracy converged regardless. This is the anisotropy hazard of the Laplace–Beltrami note, observed but benign at these amplitudes.

**Decision: bilinear**, recorded in the master plan §4. The C¹ advantage of the B-spline did not materialize in operator accuracy, and bilinear keeps the exact Taylor-pyramid leaf construction and TFDM compatibility. Cheap to revisit if later operator work shows a C¹ need.

## Experiment 4 — diagnostics first light

Obliquity over all base triangles, at the barycenter:

- cc_torus (dense, regular): sin²θ median 1.9e-4, max 2.3e-4 — displacement directions within ~1° of perpendicular.
- spot (coarse artist mesh): sin²θ median 2.3e-3, p99 6.4e-2, max 0.32 — tilts up to ~35°, with a heavy tail.

This is the obliquity note's prediction ("the defect grows with base coarseness") visible in real data on the first try. Normalized δ shows the same contrast (median 0.04 vs 0.48). Application D has signal.

## Caveats

- Single-triangle domains, identity chart per face, Neumann boundaries on the toy solves. Seams, per-face tiling, and the Taylor pyramid are Phases 1+.
- The heat-method ground truth is the same solver on a finer lattice, so exp03 measures operator consistency, not absolute geodesic accuracy.

Related: [[Project — Conservative metric queries without tessellation]] · [[Log — Conservative metric queries]] · [[The induced metric of a displaced surface]] · [[Laplace–Beltrami on displaced surfaces]] · [[Obliquity and the integrability defect]]
