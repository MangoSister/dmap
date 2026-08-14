---
title: Inverse and differentiable micro-geometry
tags: [open-question, research-direction, differentiable, reconstruction]
priority: 8
---

# Inverse and differentiable micro-geometry

> [!note] Analysis, not a claim from any paper. See [[MOC — Open Questions]].

> [!success] Confirmed open — 2026-08-10, with the landscape clarified
> [[Zhang et al. 2026 — DJM|DJM]] requires a **high-resolution mesh input**; no images, cameras, or rendering loss anywhere. Its base-mesh construction is deliberately non-differentiable — a combinatorial, threshold-gated collapse process — and its only gradient component is a downstream MLP fitting *pre-computed* displacement samples, which the authors stress is easier to supervise than NGF's rendering-loss optimisation. It also endorses the critique that "methods that optimize for rendering loss do not guarantee geometric fidelity."
>
> Two useful consequences. First, **image-based construction remains untaken.** Second, DJM **declines to compare against [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]]** — "complementary to ours and can be applied as a DJM post-process (as no code is provided we cannot conduct this experiment)" — so there are still *zero* published numbers pitting differentiable against combinatorial base-mesh construction. That comparison is itself an unclaimed contribution.

[[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] made displacement construction differentiable. Two obvious continuations are untouched.

## Differentiable opacity micromaps

Nobody has made OMM baking differentiable. Today the bake is a **conservative classification** of an artist-authored alpha texture — sound, but it inherits every authoring flaw downstream, and conservativeness is expensive precisely where the mask is noisy (badly cropped foliage art, which [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]] show yields 93% skip rate and ~1% speedup because almost nothing is *fully opaque*).

A differentiable formulation would let you optimise the mask and the micromap **jointly against target appearance** — accepting small alpha changes that greatly increase the fraction of resolvable sub-triangles. The objective is a genuine trade between fidelity and the `unknown`-state count, and nobody has posed it.

## Images straight to micro-mesh

Every construction method here requires a **multi-million-triangle input mesh**: [[Maggiordomo et al. 2023 — Micro-Mesh Construction|Maggiordomo et al.]] remesh one, [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] optimise against renders of one. But photogrammetry and multi-view reconstruction produce the dense mesh only as an intermediate that is then thrown away by the very next step.

Reconstructing **base mesh plus displacement directly from images** would skip it. The format is arguably a *better* reconstruction target than a dense mesh — it is compact, has built-in LoD, is watertight by construction, and separates low-frequency shape from high-frequency detail in exactly the way multi-view stereo's error characteristics do. [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] already optimise a µ-mesh through a differentiable rasteriser against rendered images; swapping rendered targets for **photographs** is a smaller change than it sounds, and the harder parts (progressive subdivision, Laplacian reparameterisation, shell-volume regularisation) already exist.

The pieces are also converging from the other side: DJM ([[2026 successors — DJM and NVIDIA foliage]]) computes bijective low-distortion displacement maps analytically, and [[Mendiratta et al. 2026 — NeuBase|NeuBase]] fits offset fields with learned bases.

**Risks.** This is where the vision community is most likely to have moved already — it does not cite the graphics literature reliably and vice versa, which is precisely how [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] ended up uncited by [[Barczak et al. 2024 — DGF|DGF]] three months later. Check CVPR/ICCV/3DV before committing.

Related: [[Base mesh quality objectives]] · [[Deformation-aware displacement fields]]
