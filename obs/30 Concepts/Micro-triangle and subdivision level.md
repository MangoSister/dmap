---
title: Micro-triangle and subdivision level
tags: [concept, subdivision, lod, format]
---

# Micro-triangle and subdivision level

The shared vocabulary of sub-triangle geometry — used identically by the displacement and opacity halves of this vault, which is why [[Micromap]] is the bridge node between them.

## The subdivision

A base triangle splits into 4 by midpoint edge splits, recursively. At **per-face subdivision level `k`** there are `4^k` micro-triangles, and an unmodified base edge carries `2^k` segments. Because the split is uniform, **micro-triangle positions are implicit** — index determines position, and nothing about the subdivision needs storing.

Micro-vertices get their attributes by barycentric interpolation from the base face: position, displacement direction, and uv all interpolate; the direction is then scaled by a per-µ-vertex scalar and added.

## Level assignment

The level is chosen **per base triangle**, and the two strategies in [[Maggiordomo et al. 2023 — Micro-Mesh Construction|Micro-Mesh Construction]] are worth distinguishing:

- **Uniform µ-triangle area** — a global level plus a per-triangle correction for its area. Cheap, closed-form.
- **Adaptive** — simulate a one-level increase, project the new µ-vertices onto the input, and keep candidate increases in a priority queue sorted by predicted geometric-error reduction. More accurate, more expensive.

[[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] replace both with a **visual-guided criterion**: the relative image-space error improvement a finer level actually delivers, measured across cameras — arguing that surface-sampling metrics "may fail to handle the fine-grained details".

For opacity, the analogous question is answered geometrically. [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] define the **highest useful subdivision level** as the point where a sub-triangle's longest edge falls below one pixel: `log₂ max(|e₀|,|e₁|,|e₂|) ≤ n`. [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] pick traversal depth at runtime from screen-space area as `⌊log₁₆ A⌋`.

## The ±1 constraint

Adjacent base faces may differ by **at most one** subdivision level. This is not a soft guideline but a format requirement, and it is what makes the edge decimation flags able to close cracks — see [[Watertightness and cracks]].

Every level-assignment algorithm therefore ends with a **correction phase** that conservatively raises the lower level of every offending pair until none remain, then sets the decimation flags.

## Level of detail

µ-meshes carry **four watertight LODs**. Hardware ray queries can apply an **LoD bias** treating every µ-mesh as if its level were reduced by up to three — 1/64 the micro-triangles — and shader code can reduce arbitrarily, all the way down to the base mesh. As long as the reduction is uniform, the surface stays **bit-exact watertight**.

The tessellation-free methods handle LoD differently, since they have no stored levels:

- [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] stops traversal above a target mip level for integer LoD, and because the surface is linear in height, **fractional LoD is just a blend of two consecutive integer-LoD surfaces** — implemented by fetching the parent texel.
- [[Thonat et al. 2023 — RMIP|RMIP]]'s texture layout lets **hardware trilinear sampling** deliver fractional LoD directly.
- [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] deliberately does *not* implement LoD for displacement, judging distance-based mip selection ill-suited to high-frequency geometry, and instead adds two exact fast paths for flat regions.

## Quantisation

Scalar displacements are stored as **11-bit** normalised values in the µ-mesh format — "roughly the precision that 32-bit float affords around the value +1". They are also **85–90% of final storage**, so the bit count has an almost linear effect on memory.

The measured surprise: **7 bits already suffices** to avoid noticeable artefacts. 11 is conservative. This is also why shell-volume minimisation pays twice — shorter displacement vectors mean the same bit count resolves finer detail. See [[Shell, prism and prismoid]] and [[Base mesh quality objectives]].
