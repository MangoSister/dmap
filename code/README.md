# Numpy reference for conservative metric queries

Phase 0–1 reference implementation for the project in
`../obs/60 Projects/Project — Conservative metric queries without tessellation.md`.
This code is the correctness oracle for the later C++ core. Clarity over speed.

## Setup

All Python runs in the conda env `dmap`:

```
conda create -n dmap python=3.12 numpy scipy matplotlib imageio pytest
```

On this machine conda is not on PATH; use the interpreter directly:
`C:\Users\yyp05\miniconda3\envs\dmap\python.exe`.

## Layout

- `dmapref/` — the package. One module per concept note section:
  - `mesh.py` — OBJ loading, per-triangle base geometry (metric note §6 constants)
  - `displacement.py` — texture loading and synthetic analytic displacement fields
  - `interpolant.py` — bilinear and biquadratic B-spline `h(u,v)` with analytic gradients
  - `metric.py` — base forms, the master metric formula, determinant identities
  - `diagnostics.py` — obliquity and integrability defect (obliquity note §4)
  - `reference_surface.py` — dense evaluation of `S = P + hN`, finite differences, tessellation Gram matrices
  - `laplacian.py` — cotangent Laplacian from edge lengths (Laplace–Beltrami note §2)
  - `heat_method.py` — mini heat method for geodesic distance (Crane et al. 2013)
  - `affine.py` — batched affine arithmetic over shared noise symbols, plus interval helpers
  - `pyramid.py` — the Taylor-model bound pyramid (exact bilinear leaf, conservative fold) and the min-max ablation pyramid (pyramid note §2–§5)
  - `node_bounds.py` — base-form cell enclosures and node-to-metric bound propagation with ablation variants (pyramid note §6)
  - `dense_reference.py` — vectorized pointwise truth and per-cell range reductions for the tightness study
- `experiments/` — one script per question (Phase 0: exp01–04, Phase 1: exp05–07); each prints a verdict and saves figures to `experiments/out/`
- `tests/` — pytest sanity checks from the notes
- `data/simple/` — meshes and displacement maps

## Run

From `C:\dmap\code`:

```
C:\Users\yyp05\miniconda3\envs\dmap\python.exe -m pytest tests
C:\Users\yyp05\miniconda3\envs\dmap\python.exe experiments\exp01_metric_vs_fd.py
```
