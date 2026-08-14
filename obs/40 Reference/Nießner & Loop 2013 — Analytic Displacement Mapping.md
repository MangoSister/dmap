---
title: "Analytic Displacement Mapping using Hardware Tessellation"
authors: [Matthias Nießner, Charles Loop]
affiliation: University of Erlangen-Nuremberg; Microsoft Research
year: 2013
venue: ACM TOG 32(3), Article 26
doi: 10.1145/2487228.2487234
tags: [reference, displacement, subdivision, hardware-tessellation, analytic-derivatives, smooth-base]
status: read
---

# Nießner & Loop 2013 — Analytic Displacement Mapping using Hardware Tessellation

Unlike most notes in this folder, this one is **read directly from the PDF** (below), not summarised from citations. Added 2026-08-11 because it is the closest graphics precedent for two claims the metric thread makes, and the counterexample to a third.

> [!warning] The PDF's front matter is corrupted
> This copy's ACM reference block and page headers name a different paper entirely (Pamplona et al. 2009, TOG 28(4)) — a template artifact. The actual publication is ACM TOG 32(3), Article 26, 2013, DOI 10.1145/2487228.2487234.

## What it is

Real-time displacement mapping over a **smooth base**: $f(u,v) = s(u,v) + N_s(u,v)\,D(u,v)$, where $s$ is the **Catmull–Clark limit surface** (analytic, $C^2$ except $C^1$ at extraordinary vertices), $N_s$ its **true analytic normal field**, and $D$ a scalar **biquadratic B-spline** displacement with a Doo–Sabin subdivision structure. Constraining $D$ to be $C^1$ with vanishing first derivatives at extraordinary vertices makes the displaced surface $C^1$ **everywhere**.

Displacement coefficients live in a **tile-based texture format** — a GPU-friendly Ptex variant: one tile per quad face of the base control mesh, power-of-two tile sizes, a one-texel overlap ring instead of run-time adjacency pointers, per-tile **mip pyramids** (Haar or B-wavelet downsampling), and non-uniform tile resolutions reconciled by clamping to consistent mip levels along shared edges.

## The two results that matter here

**Analytic derivatives of the displaced surface, in a pixel shader.** Because $s$, $N_s$ and $D$ are all analytic, the displaced tangents are evaluated directly:

$$
\frac{\partial f}{\partial u} = \frac{\partial s}{\partial u} + \frac{\partial N_s}{\partial u}\,D + N_s\,\frac{\partial D}{\partial u}
$$

with $\partial N_s/\partial u$ computed via the **Weingarten equation** from the base's first and second fundamental forms $(E,F,G,e,f,g)$ — explicitly, in shader code, in 2013. No normal map is ever stored; shading normals come from $\partial f/\partial u \times \partial f/\partial v$ and stay correct under animation and displacement edits. An **approximate variant** drops the Weingarten term ($\partial f/\partial u \approx \partial s/\partial u + N_s\,\partial D/\partial u$, after Blinn 1978) — valid for small displacement, 46–53% faster on their test models, visibly wrong where displacement and base curvature are both large.

**Mip-level selection as anti-swimming LoD.** Hardware tessellation resamples the surface every frame; undersampling a high-frequency $D$ makes the surface "swim" as the sampling pattern shifts. Their fix: choose the displacement **mip level to match the tessellation density** (fractional, blended between levels). The mip pyramid over displacement exists for *signal-matching*, not for bounding — no conservative property anywhere.

## Why this vault cares

1. **It is the escape from the watertightness trilemma.** [[Obliquity and the integrability defect]] shows flat-base formats must choose obliquity or cracks. This paper takes neither: the base is smooth and the displacement direction is the base's *true* normal, so $\mathbf{a} = 0$ **and** the surface is watertight ($C^1$, even). The price moves to base-surface evaluation cost (Stam evaluation or feature-adaptive subdivision) — and it is the road µ-meshes, [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] and [[Thonat et al. 2023 — RMIP|RMIP]] all declined, buying flat-triangle simplicity with obliquity.
2. **The metric was one Gram matrix away, thirteen years ago.** $\partial f/\partial u, \partial f/\partial v$ — Weingarten term included — are exactly the ingredients of the first fundamental form of [[The induced metric of a displaced surface]]. They are used only to form a shading normal; $\mathbf{G}$ itself is never assembled, never named, never bounded. Cite this as the precedent for "the metric is nearly free once derivatives are in hand"; the *conservative bounding* of it ([[Taylor-model bound pyramid]]) is what remained undone.
3. **Per-face tile storage is the seam answer with production precedent.** Their tile format is Ptex adapted to the GPU — the same design the [[Project — Conservative metric queries without tessellation|metric-queries project]] adopts to make seams a non-problem by construction.
4. Their approximate-vs-accurate shading split is the rendering-side shadow of the metric note's regime table: dropping the Weingarten term is dropping the $h\,N_u$ channel — i.e., pretending $\mathbf{B}_0 = \mathbf{C}_0 = 0$.

## Relation to vault notes

[[The induced metric of a displaced surface]] (cites this as classical precedent, §9) · [[Obliquity and the integrability defect]] (the third option, §1) · [[Watertightness and cracks]] · [[Min-max mipmap and conservative bounds]] (their mip pyramid is the non-conservative ancestor on the *displacement* side, as LEADR is on the moments side) · [[Project — Conservative metric queries without tessellation]]

![[niessner2013analytic.pdf]]
