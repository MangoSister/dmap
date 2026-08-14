---
title: Maximum mipmaps for fast, accurate, and scalable dynamic height field rendering
authors: [Art Tevs, Ivo Ihrke, Hans-Peter Seidel]
year: 2008
venue: I3D 2008
tags: [reference, not-in-vault, min-max-mipmap, height-field]
---

# Tevs et al. 2008 — Maximum Mipmaps

**Not in vault** — summarised from citations in [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] and [[Thonat et al. 2023 — RMIP]]. Not read directly.

The direct predecessor of the [[Min-max mipmap and conservative bounds|min-max mipmap]] as an *empty-space-skipping* structure for height fields: store the maximum height per texel per mip level, and use it to skip regions a ray cannot reach.

[[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] describes itself as generalising this from **planar height fields to arbitrary polygon base meshes** — the step that requires affine-arithmetic bounds, since the mapping from texture space to world space is no longer a simple scale.
