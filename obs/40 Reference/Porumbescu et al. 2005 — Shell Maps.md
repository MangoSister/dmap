---
title: Shell Maps
authors: [Serban D. Porumbescu, Brian Budge, Louis Feng, Kenneth I. Joy]
year: 2005
venue: ACM TOG 24(3) — SIGGRAPH 2005
tags: [reference, not-in-vault, shell-mapping]
---

# Porumbescu et al. 2005 — Shell Maps

**Not in vault** — summarised from citations in [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]], [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]], [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] and [[Dou et al. 2024 — Differentiable Micro-Mesh Construction]]. Not read directly.

The origin of **shell mapping**: construct the shell between a base mesh and its offset mesh, decompose each prism into **tetrahedra**, and ray-march through them to map arbitrary *instanced geometry* — not just a height field — into the shell. See [[Shell, prism and prismoid]].

Two things it contributed that later papers argue with:

- The **tetrahedral decomposition**, rejected by [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] for its initialisation cost and piecewise-linear aliasing.
- The **"buckling" artefact** it names, which [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] cites as the thing its bilinear-patch prism sides avoid.

It is also the source of the fact [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] use for their shell-volume regulariser: a prismoid's volume is the sum of three tetrahedra.
