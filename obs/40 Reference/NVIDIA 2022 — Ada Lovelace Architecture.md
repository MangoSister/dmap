---
title: NVIDIA Ada GPU Architecture Whitepaper
authors: [NVIDIA]
year: 2022
tags: [reference, not-in-vault, hardware, dmm, omm, ada]
---

# NVIDIA 2022 — Ada Lovelace Architecture (DMM and OMM engines)

**Not in vault** — summarised from citations in [[Maggiordomo et al. 2023 — Micro-Mesh Construction]], [[Dou et al. 2024 — Differentiable Micro-Mesh Construction]] and [[Barczak et al. 2024 — DGF]], plus vendor documentation. Not read directly.

The definitional source for the micro-mesh format. Ada's 3rd-generation RT Core added **two** new fixed-function units:

- **Opacity Micromap Engine** — resolves per-sub-triangle opacity during traversal without invoking any-hit. See [[Any-hit shader cost]].
- **Displaced Micro-Mesh Engine** — natively ray traces a compressed, self-bounding displacement representation, building the BVH over coarse base triangles rather than the displaced micro-geometry.

It is the source of the numbers the construction papers quote second-hand: **7–15× BVH build-time improvement and 5–20× BVH size savings** for DMM, the **11-bit** displacement quantisation, the prismoid min/max fitting, and the LoD bias allowing hardware to drop up to three subdivision levels.

> [!warning] Only one of the two survived
> The opacity engine's format was ratified as `VK_KHR_opacity_micromap` in 2026 and shipped in DXR 1.2. The displacement engine's software stack was **withdrawn in February 2025**. Whether Blackwell silicon physically removed the DMM unit is not confirmed by any primary NVIDIA statement I could find — see [[Graphics API and hardware support timeline]].

NVIDIA's micro-mesh developer page still advertises DMM with no deprecation notice, so it should not be treated as current.
