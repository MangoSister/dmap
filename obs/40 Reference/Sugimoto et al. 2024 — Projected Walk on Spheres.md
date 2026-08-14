---
title: "Projected Walk on Spheres: A Monte Carlo Closest Point Method for Surface PDEs"
authors: [Ryusuke Sugimoto, Nathan King, Toshiya Hachisuka, Christopher Batty]
affiliation: University of Waterloo
year: 2024
venue: SIGGRAPH Asia 2024 Conference Papers
doi: 10.1145/3680528.3687599
tags: [reference, walk-on-spheres, monte-carlo, surface-pde, closest-point-method, local-feature-size]
status: read
---

# Sugimoto et al. 2024 — Projected Walk on Spheres (PWoS)

Read directly from the PDF (below). Added 2026-08-11 as the algorithmic backbone of application B2 in [[Project — Conservative metric queries without tessellation|the metric-queries project]] — and as the paper whose stated geometric dependencies are exactly what a conservative hierarchy can certify.

## Goal

Solve surface PDEs — Poisson $\Delta_{\mathcal S} f = f_{\mathcal S}$ and screened Poisson, with Dirichlet data on boundary curves $C \subset \mathcal S$ — **pointwise and discretization-free**: no meshing of the surface, no global linear system, evaluation only where the answer is wanted (their Fig. 1 evaluates diffusion curves only at visible pixels). Assumes nothing about the surface beyond **a closest-point query** $\mathrm{cp}_{\mathcal S}$ and an unoriented normal direction; demonstrated on meshes, oriented point clouds, and mixed-codimension geometry.

## Method

The **closest point extension** (from the closest point method, Ruuth–Macdonald): extend $f$ off the surface as constant along normals inside a tubular neighbourhood $\mathcal N(\mathcal S)$; there, $\Delta_{\mathcal S} f$ on the surface equals the ordinary Cartesian $\Delta$ of the extension (up to a compensation term $g$ they set to zero — a known bias source for complex source terms, named as future work). Then run walk-on-spheres in the ambient space **with a projection each step** (their Alg. 1):

- at $x$: radius $r = \min(\,\text{lfs}(x),\ \text{distance to the extended Dirichlet boundary}\,)$;
- sample $y$ uniformly on the 3D sphere of radius $r$; accumulate the source term from $N_V$ ball samples $z_i$, **projected**: $f_{\mathcal S}(\mathrm{cp}_{\mathcal S}(z_i))$;
- recurse at $\mathrm{cp}_{\mathcal S}(y)$; stop when within $\varepsilon$ of the boundary and read the boundary value at $\mathrm{cp}_C$.

Convergence $O(1/\sqrt{N_P})$ over $N_P$ independent walks. Extensions: screened Poisson via Yukawa potential with Russian-roulette termination; divergence-form sources and a gradient estimator (their §3.3.2 — used to run [[Crane et al. 2013 — Geodesics in Heat|the heat method]] entirely inside PWoS, §5.2.2); an optional mean-value filter over a discrete basis (biased, efficiency only).

## The two geometric dependencies — and how the paper meets them

1. **Closest-point queries, every step.** Their implementation uses Houdini's built-in mesh closest-point — i.e., in practice the surface is discretized after all; "discretization-free" refers to the *solver*, not the geometry access.
2. **A conservative lower bound on local feature size**, to keep spheres inside $\mathcal N(\mathcal S)$. Their §3.1 builds it by preprocessing: scatter points, extract a **medial-axis point cloud** by shrinking tangent balls (Ma et al. 2012), prune it scale-axis-style (factor $s$), shift tangent-ball pairs, multiply by **0.9 as a safety factor**, and clamp by a threshold $\lambda$ at sharp corners. This is *heuristic* conservatism, and the paper documents both failure directions: an under-estimate is valid but slow — their Fig. 3 shows average walk length on a unit sphere growing **31 → 1819 steps** as the lfs estimate shrinks from 0.99 to 0.0625 — while aggressive pruning ($s = 1.15$) produces **visible residual bias** (their Fig. 4c).

## Why this vault cares

The correctness-critical quantity of the whole method — the lfs lower bound — is estimated by a lossy preprocessing pipeline, and its tightness directly controls cost. That is precisely the profile of a consumer for **certified hierarchical bounds**: conservative closest-point pruning (a wrongly pruned node means a wrong projection, hence bias — conservativeness is unbiasedness here), certified distance-to-boundary on the safe side, and a certified lfs as *local curvature reach* ∧ *global self-separation* — the global half being a hierarchical node-pair query no local estimate can see. See [[The induced metric of a displaced surface|the metric note §8]] for the reach decomposition and the [[Project — Conservative metric queries without tessellation|project note, application B2]]. Also note PWoS cannot run on an implicit displaced surface at all today — there is no $\mathrm{cp}_{\mathcal S}$ to call without tessellating first.

![[PWoS.pdf]]
