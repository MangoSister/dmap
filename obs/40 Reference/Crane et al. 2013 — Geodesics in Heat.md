---
title: "Geodesics in Heat: A New Approach to Computing Distance Based on Heat Flow"
authors: [Keenan Crane, Clarisse Weischedel, Max Wardetzky]
affiliation: Caltech; University of Göttingen
year: 2013
venue: ACM TOG 32(5), Article 152
doi: 10.1145/2516971.2516977
tags: [reference, geodesics, heat-method, laplacian, pde, geometry-processing]
status: read
---

# Crane et al. 2013 — Geodesics in Heat

Read directly from the PDF (below). Added 2026-08-11 as the algorithmic backbone of application B1 in [[Project — Conservative metric queries without tessellation|the metric-queries project]].

## Goal

Geodesic distance to a point or curve $\gamma$, computed **robustly, fast, and repeatedly**. The prevailing alternative solves the eikonal equation $|\nabla\varphi| = 1$ — nonlinear and hyperbolic, so fast marching/sweeping use serial priority-queue updates, resist parallelization and prefactorization, and restart from scratch for every new source.

## Method

Replace the nonlinear problem with **two standard linear elliptic solves** plus a normalisation. The insight: Varadhan's formula recovers distance from the heat kernel, but is hopelessly sensitive to errors in the kernel's *magnitude*. Only the heat gradient's **direction** is trustworthy — heat flows away from the source along shortest paths — so use heat for direction only and rebuild the magnitude by integration:

1. **Heat step** — integrate $\dot f = \Delta f$ for one small fixed time $t$, discretised as a single backward Euler solve $(\mathbf{M} - t\mathbf{L})\,f = \delta_\gamma$ (lumped mass $\mathbf{M}$, cotan Laplacian $\mathbf{L}$).
2. **Normalise** — $X = -\nabla f / \lVert \nabla f \rVert$, a unit field pointing along geodesics.
3. **Poisson step** — solve $\mathbf{L}\,\varphi = \nabla \!\cdot\! X$: the scalar potential closest to having gradient $X$. $\varphi$ is the distance (up to an additive shift; exact as $t \to 0$).

(Paper notation: their $u$ is this vault's $f$, per [[Laplace–Beltrami on displaced surfaces]]; their $\phi$ is $\varphi$.)

Both matrices are sparse, symmetric positive (semi-)definite, and **independent of the source** — prefactor once, and every new $\gamma$ costs two back-substitutions. Measured an order of magnitude faster than fast marching at comparable accuracy for repeated queries.

## The property this vault leans on

**The method is substrate-agnostic by design**: it "can be applied to virtually any type of geometric discretization" — the paper demonstrates simplicial meshes, general polygonal meshes (Alexa–Wardetzky Laplacian), and connectivity-free point clouds — because it needs only a gradient, a divergence, and a Laplacian. So the *entire* burden of running it on a new representation is producing those three operators. On a displacement map that is exactly what the metric supplies: cotan weights are intrinsic (edge lengths only), lengths come from $\ell^2 = e^\top \mathbf{G} e$, and — decisive for this representation — **the heat method needs only first derivatives of $h$** (no Christoffel symbols, no second-derivative pyramid). See [[Laplace–Beltrami on displaced surfaces]] §2–§3, which builds precisely these operators, and [[The induced metric of a displaced surface]].

Also consumed downstream: [[Sugimoto et al. 2024 — Projected Walk on Spheres]] runs this same three-step pipeline through a pointwise Monte Carlo solver instead of a global one (their §5.2.2) — the two routes of the project's PDE application are two discretisations of this one algorithm.

![[Geodesics_in_Heat.pdf]]
