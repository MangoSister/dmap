---
title: "Cool Patches: A Geometric Approach to Ray/Bilinear Patch Intersections"
authors: [Alexander Reshetov]
affiliation: NVIDIA
year: 2019
venue: Ray Tracing Gems
tags: [reference, not-in-vault, intersection, bilinear-patch]
---

# Reshetov 2019 — Cool Patches

**Not in vault** — summarised from citations in [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]] and [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]]. Not read directly.

An efficient geometric ray/**bilinear patch** intersection test. It matters here because of a structural fact about prisms: when adjacent base-vertex normals are not coplanar, **the side face of a prism is a bilinear patch, not a planar quad**. See [[Shell, prism and prismoid]].

Both [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] and [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] use it to intersect prism sides directly rather than splitting each face into two triangles. PDM lists the payoff explicitly: `C¹` continuity at the interface between two sides, no need to pick a consistent diagonal, and a test that maps well to GPUs. It also sets the ceiling on PDM's watertightness, which is "as good as the ray/bilinear patch algorithm".
