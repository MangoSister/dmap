---
title: Direct Ray Tracing of Displacement Mapped Triangles
authors: [Brian Smits, Peter Shirley, Michael Stark]
year: 2000
venue: Eurographics Workshop on Rendering 2000
tags: [reference, not-in-vault, displacement]
---

# Smits et al. 2000 — Direct Ray Tracing of Displacement Mapped Triangles

**Not in vault** — summarised from citations in [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]], [[Thonat et al. 2023 — RMIP]], [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]] and [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]]. Not read directly.

The earliest work in this vault's lineage to ray trace displaced triangles **without pre-tessellating**: march the ray through a **bounding prism** in barycentric space, using constant per-triangle bounds.

Both halves of that description recur everywhere here — the [[Shell, prism and prismoid|bounding prism]] as the per-base-triangle container, and marching as the fallback once hierarchical bounds stop paying off ([[Thonat et al. 2023 — RMIP|RMIP]]'s texel marching, [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]]'s scanning-triangle march). What later papers add is a *hierarchy* over the displacement data, which this paper lacks. See [[Tessellation-free vs pre-tessellation]].
