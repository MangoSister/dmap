---
title: "DJM: Compact Base Meshes for Displacement Mapping using Triangle Jacobians"
authors: [Congyi Zhang, Nicholas Vining, Yanhong Lin, Alireza Khatami, Ziyu Sun, Xiaohu Guo, Wenping Wang, Alla Sheffer]
affiliation: UT Dallas; NVIDIA / UBC; HKU; Texas A&M
year: 2026
venue: ACM TOG (preprint, venue fields still placeholder)
arxiv: 2606.22880
project: https://www.cs.ubc.ca/labs/imager/tr/2026/djm/
tags: [paper, construction, base-mesh, distortion, bijectivity, geometry-processing]
status: read
---

# DJM: Compact Base Meshes for Displacement Mapping using Triangle Jacobians

The current state of the art in base-mesh construction, and the direct successor to [[Maggiordomo et al. 2023 — Micro-Mesh Construction]].

> [!warning] Venue is unconfirmed
> This copy is a preprint: the ACM reference format reads "1, 1 (June 2026)", the DOI is a placeholder, and **the PDF never names SIGGRAPH 2026**. Web search suggested SIGGRAPH 2026; treat that as unverified. arXiv 2606.22880v1, 22 June 2026. Code and inputs are available at the project page.

## Problem

A base mesh should have as few triangles as possible while still admitting a displacement function that accurately reproduces the input — which requires the base→input mapping to be **bijective** and to have **low parametric distortion**. Existing constructors, plain QEM and Maggiordomo et al., "do not explicitly optimize for bijectivity and low distortion, leading to significant reconstruction artefacts at coarser base mesh resolutions."

## Core method

**The Displacement Jacobian Metric.** Rewriting the displacement in unnormalised form yields a **closed-form Jacobian**, `J(q) = I + t·A(BᵀB)⁻¹Bᵀ`, in terms of the base vertices, the corner displacement directions and the height. Distortion is then just `Det(J)` measured against 1, and **bijectivity is readable from the same construction**. That is the whole contribution: the quantities Maggiordomo et al. approximate with heuristics — normal deviation and approximate bijectivity — become exactly computable.

**Progressive relaxation.** Rather than blending distortion into the collapse cost — "the challenge here would be to meaningfully balance the different terms as they measure hard to compare properties" — they start with strict distortion and shape thresholds, use plain QEM to *order* collapses, collapse until stuck, then relax the thresholds slightly and repeat. Followed by an edge-flipping pass ordered by worst Jacobian determinant, which also uses vertex valence as a shape proxy.

**No ray casting.** This is the second substantive break from prior work. The input↔base correspondence is initialised to the identity and **tracked through every collapse and flip** (successive self-parameterisation). Ray casting is abandoned because "it is unstable if the ray is tangential or exactly through an edge" and "it is also ambiguous: we do not *a priori* know whether the input mesh is in front or behind a base triangle". Maggiordomo's method bakes displacements by ray-casting *after* the base mesh exists, and is therefore "susceptible to catastrophic errors when the ray-tracing fails to intersect the surface at the desired location."

The inverse barycentric displacement problem is solved by **Gauss–Newton** with LU and a QR fallback, explicitly rejecting the closed-form cubic as "highly unstable due to numerical issues" — and distinguishing itself from [[Thonat et al. 2023 — RMIP|RMIP]]: "instead of using their cross-product equation, we have found that directly solving the problem in this manner yields faster and more robust convergence."

Also fixes **Schwarz-lantern artefacts** by flipping the subdivision pattern per base triangle when it improves normal similarity.

## Results

109 high-resolution inputs, 89 from Maggiordomo's own dataset.

RMS Hausdorff (mean / median / max) at 64× subdivision:

| | Micro-Mesh | QEM | **DJM** |
|---|---|---|---|
| RMS | 0.498 / 0.284 / 3.091 | 5.921 / 2.277 / 36.479 | **0.320 / 0.252 / 1.168** |
| Subdivision faces | 98,142 | 94,936 | **82,642** |

It wins on **every** cell of the error table at all three subdivision budgets, and at 64× and 16× also produces fewer faces and smaller files. The headline robustness claim is the max-error gap — **0.427 vs 2.581** at 256×.

Against neural baselines: beats NGF on Chamfer (0.252 vs 0.609 ×10⁻³), L2 and RMS with ~23% fewer parameters, and Pentapati et al. 2025 on Chamfer (0.237 vs 0.327) with under 20k versus 60k parameters.

Runtime: ~15 minutes for the base mesh plus ~28 minutes for displacement maps.

## Limitations

Closed meshes only, following the default QEM implementation. Bijectivity constraints "may 'bake-in' artifacts in the input mesh". Bijectivity is maintained per-operation, not guaranteed globally — "This is not necessarily a global guarantee of bijectivity due to numerical issues, but like other work it is sufficient for our purposes."

## Relation to other work

It **replaces two of the four** [[Base mesh quality objectives]] with an exact quantity, keeps quadric error only as an ordering and triangle shape as a separate threshold, and **sidesteps prismoid/shell volume entirely** — there is no prism or enclosure-volume term anywhere in the method.

On [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] it declines to compete: their method "is complementary to ours and can be applied as a DJM post-process (**as no code is provided we cannot conduct this experiment**)". So there are **zero numbers** against the differentiable approach, and Dou et al. is positioned as a refinement stage rather than a rival constructor.

Cites [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] and [[Thonat et al. 2023 — RMIP|RMIP]] in passing, and [[Karis et al. 2021 — Nanite|Nanite]] for cluster hierarchies. Does **not** cite [[Barczak et al. 2024 — DGF|DGF]], [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al. 2024]], or any opacity work.

Hardware content is minimal — it notes a maximum subdivision depth of 5 "corresponding to hardware limitations", and says nothing about the withdrawal of displacement micromaps.

![[DJM_Compact_Base_Meshes_for_Displacement Mapping.pdf]]
