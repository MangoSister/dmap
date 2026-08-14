---
title: Differentiable Micro-Mesh Construction
authors: [Yishun Dou, Zhong Zheng, Qiaoqiao Jin, Rui Shi, Yuhan Li, Bingbing Ni]
affiliation: Huawei; Shanghai Jiao Tong University
year: 2024
venue: CVPR 2024, pp. 4294–4303
tags: [paper, micro-mesh, construction, differentiable-rendering, optimization]
status: read
---

# Differentiable Micro-Mesh Construction

The vision-side answer to [[Maggiordomo et al. 2023 — Micro-Mesh Construction|Micro-Mesh Construction]]: replace the staged pipeline with a single **end-to-end differentiable optimisation**.

## Problem

µ-mesh construction is **stage-based** — decimate, remesh, subdivide, bake — and each stage minimises a *local surrogate* (quadric error, centroidal Voronoi energy) that is **not the µ-mesh's actual objective**, which is faithful reprojection and minimal shell volume. That risks error accumulation. The sharpest version of the argument: the shell volume is essentially fixed the moment the base mesh is produced, yet its true value is only known **after** displacement baking.

## Core method

**What is optimised.** A µ-mesh is `M = (V, F, O, D)`. The trainable parameters are the **base vertex offsets `O`** and the **per-µ-vertex scalar displacements `D`**; base topology is fixed after initialisation. The key departure:

> we also make the position of base vertex optimizable, in contrast to the staged µ-mesh construction methods that freeze the base vertex once it is initialized or rely on further post process after displacement baking.

**What is made differentiable.** The whole tessellation — Loop 1-to-4 subdivision without vertex update, barycentric interpolation of directions, the Laplacian reparameterisation, and the renderer. `V^ℓ = subdiv(M(V+O, F)) + interp(N)·D^ℓ`, where both `subdiv` and `interp` are naturally differentiable.

Two computation-graph details matter: the **µ-normals used to displace µ-vertices are detached** (they destabilise training otherwise), while the normals of the tessellated mesh **fed to the renderer are not**, because "the gradient of the normal consumed during shading would boost the gradients".

**Laplacian reparameterisation.** Two cotangent Laplacians — one on the base mesh, one on the tessellated µ-mesh at level ℓ — give `Ō = (I + λ₁L)O` and `D̄^ℓ = (I + λ₂L^ℓ)D^ℓ`. This is the diffusion reparameterisation of Nicolet, Jacobson & Jakob [2021] ported to µ-meshes. It biases gradient steps toward smooth solutions **without requiring the final µ-mesh to be smooth** — unlike a Laplacian smoothness *regulariser*, which would force reprojectability to compete with smoothness. The ablation shows this is the single most load-bearing component (removing it costs 2.6 dB).

**Objective.** An **L₁ image loss** under a differentiable rasteriser (`nvdiffrast`) with known cameras and spherical-harmonic shading — rendering consistency as the image-space instantiation of reprojectability, argued to be more robust to mesh resolution than Chamfer distance or QEM. Plus two regularisers:

- **Shell volume `L_sv`** (weight 10) — the sum over base vertices of the **variance of adjacent primitives' displacements**. Chosen because differentiating tetrahedron volume directly is deficient: min/max fitting only exposes the extreme parameters to the optimiser. A useful side effect is smaller displacements, hence **less quantisation loss**.
- **Isotropy `L_iso`** (weight 1) — "as-equilateral-as-possible", the variance of each offset base face's three edge lengths. Local rather than a global edge-length term, so it distracts the global objectives less.

**Progressive optimisation** (curriculum learning): a warm-up at ℓ=0 optimising the base mesh only, then alternating subdivision and optimisation, then a final phase freezing the base offsets and tuning displacements at all levels simultaneously. 1600 iterations, subdivision at 50/150/400/800, UniformAdam.

**Adaptive LoD with a visual-guided stopping criterion.** For each base face and camera, measure the image-space error at two adjacent levels and return the **relative visual improvement** the finer level brings. Used as an early-stop during optimisation rather than as post-hoc pruning. The authors argue Chamfer/QEM criteria "may fail to handle the fine-grained details since they rely on surface sampling" — and the ablation confirms it, though modestly.

No neural network is learned; the µ-mesh is a PyTorch module whose weights *are* the geometry.

## Results

i9-12900K + RTX 3090, on Maggiordomo's 121-mesh dataset split into medium/large/extreme.

| Subset | PSNR (Staged → Ours) | Shell volume | Time |
|---|---|---|---|
| Medium (<1M) | 41.61 → **42.93** | 0.085 → 0.083 | 2.0 → 3.9 min |
| Large (1–2M) | 42.42 → **44.18** | 0.086 → 0.082 | 4.0 → 4.3 min |
| Extreme (>2M) | 42.61 → **44.89** | 0.089 → 0.083 | 5.3 → **4.8 min** |

So **+1.3 to +2.3 dB** with 2–7% lower shell volume, and actually *faster* on the extreme subset — because the staged method spends most of its time in decimation (≈4.6 min) while this method's decimation is a light GPU QEM pass (≈3 s) followed by ≈4.2 min of optimisation. The advantage **grows with decimation ratio**.

Notably, its base-mesh geometry error and isotropy are sometimes *worse* than the baselines. The authors argue directly that these "only have rough correlations to the final µ-mesh quality" and that one should optimise the final objective instead — the paper's thesis in miniature.

Ablation (extreme subset, PSNR): full **44.89**; without reparameterisation 42.25; without progressive schedule 43.65; without `L_iso` 44.52; without `L_sv` 44.60; with Chamfer instead of visual-guided LoD 44.78.

## Limitations

No dedicated limitations section. Implied: requires multi-view rendering supervision with known cameras and a differentiable rasteriser; requires **pre-tessellation**, deviating from the deployment pipeline where tessellation is deferred to hardware; ~4–5 minutes per model, slower than Simplygon. Reports rendering PSNR and shell volume — **not** ray-tracing timings, BVH sizes or build times, so its hardware claims are inherited rather than measured.

## Relation to other work

Almost entirely oriented around [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] — its baseline, its dataset, and its target. It cites none of the ray-tracing papers in this vault, and [[Barczak et al. 2024 — DGF|DGF]] (three months later) does not cite it. Its hardware framing comes from [[NVIDIA 2022 — Ada Lovelace Architecture]]: tighter BLAS via a prismoid per base triangle, occlusion culling, and a lower ray–µ-face miss ratio as the payoff for small [[Shell, prism and prismoid|shell volume]].

The 2026 successor working the same problem from the geometry side is DJM — see [[2026 successors — DJM and NVIDIA foliage]].

![[Dou_Differentiable_Micro-Mesh_Construction_CVPR_2024_paper.pdf]]
