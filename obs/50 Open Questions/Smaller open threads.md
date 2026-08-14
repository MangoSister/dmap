---
title: Smaller open threads
tags: [open-question, research-direction, misc]
priority: 10
---

# Smaller open threads

> [!note] Analysis, not a claim from any paper. See [[MOC — Open Questions]].

Gaps worth recording but not worth a note each — several are explicit future-work items that were simply never picked up.

**Tessellation-free vector displacement.** Both [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] and [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] list it as future work; neither did it, and nobody since. The formulation difficulty is real and specific: a [[Min-max mipmap and conservative bounds|min-max mipmap]] bounds a scalar interval, but vector displacement needs bounds on a 3D region, so the whole bounding argument must be rebuilt. That is what makes it a genuine research problem rather than an engineering task — and it would unlock overhangs and undercuts, which scalar displacement cannot represent at all ([[Base mesh quality objectives]]).

**Recursive nesting for unbounded detail.** [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] proposes nesting the structure inside itself for "infinite fractal detail". Never attempted. Interesting for procedural terrain and natural surfaces where detail genuinely is scale-free.

**Streaming and out-of-core micro-geometry.** TFDM notes that LoD support means the whole MIP pyramid need not be resident, and points at sparse textures. Nobody built it. This is the natural bridge to cloud rendering, web delivery (WebGPU has no micromap equivalent at all), and geospatial digital twins.

**Geospatial and terrain analysis.** [[Thonat et al. 2023 — RMIP|RMIP]] mentions "high-resolution geo mapping" in the same closing sentence that mentions physics. A displaced surface over a coarse base with hierarchical min-max bounds *is* a digital elevation model with an acceleration structure. Viewshed analysis, line-of-sight, solar exposure and flood modelling are all ray or proximity queries against exactly this structure — and the [[Proximity and contact queries against micro-geometry|proximity]] work would serve them directly.

**Formal verification of micro-geometry formats.** [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] proved their `uv2index` equivalent to the Vulkan reference in ACL2 **and found a discrepancy at subdivision level 16** — the reference cannot handle values above 16 bits. That is a specification bug found by machine proof. Given these are hardware formats where indexing errors are silent and cross-vendor, systematic verification of indexing and watertightness invariants is nearly untouched, and the one attempt found something.

**Micromaps beyond triangles.** The same authors note `uv2index` should generalise to other dimensions, shapes and subdivision schemes. Quad domains would suit [[Mendiratta et al. 2026 — NeuBase|Catmull–Clark]] surfaces; the question is whether the hardware indexing story survives.

**Evaluation methodology.** Every paper here reports milliseconds and megabytes. None reports perceptual quality under motion, artist iteration cost, or bake failure rates — despite artefacts under animation being the actual reason [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|animated DMMs]] needed fixing, and despite [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] demonstrating that a shading-normal artefact was present in Micro-Mesh and RMIP output all along and simply unremarked.
