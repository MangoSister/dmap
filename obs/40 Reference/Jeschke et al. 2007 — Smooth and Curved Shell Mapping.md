---
title: Interactive smooth and curved shell mapping
authors: [Stefan Jeschke, Stephan Mantler, Michael Wimmer]
year: 2007
venue: Eurographics Symposium on Rendering 2007
tags: [reference, not-in-vault, shell-mapping]
---

# Jeschke et al. 2007 — Interactive Smooth and Curved Shell Mapping

**Not in vault** — summarised from citations in [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]], [[Thonat et al. 2023 — RMIP]], [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] and [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]]. Not read directly.

Replaces [[Porumbescu et al. 2005 — Shell Maps|Shell Maps']] tetrahedral decomposition with a smooth per-prism mapping (Coons patches), giving a shell that is **`C¹` inside a prism but only `C⁰` between prisms** — a continuity property that [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] inherits and lists as a limitation.

Two technical contributions cited repeatedly here:

- The **analytic world→canonical inversion**, which requires solving a **cubic** per step. [[Thonat et al. 2023 — RMIP|RMIP]] found iterative Newton inversion faster and more stable than this closed form.
- **Distance-map ray marching** for empty-space skipping, whose accuracy is bounded by the 3D distance map's resolution — the cost [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] avoids by solving in texture space instead.
