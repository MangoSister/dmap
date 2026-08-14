---
title: "NeuBase: Spline Surfaces with Neural Basis Functions"
authors: [Anshul Mendiratta, Lei Yang, Xin Li, John Keyser, Scott Schaefer, Wenping Wang]
affiliation: Texas A&M University; The University of Hong Kong
year: 2026
venue: ACM TOG 45(4), Article 88 — SIGGRAPH 2026
doi: 10.1145/3811355
tags: [paper, neural-surface, subdivision-surface, spline, deformation, offset-field]
status: read
---

# NeuBase: Spline Surfaces with Neural Basis Functions

The outlier of this vault — **no ray tracing content at all** — but it belongs here because it is the same architectural idea as a micro-mesh, arrived at from the geometric modelling side: **a coarse base surface plus an offset field along interpolated normals**.

## Problem

Neural parametric surface representations encode geometry *directly* with a network. That hurts fitting accuracy — the net must capture global shape and fine detail at once — and destroys the classical spline properties (linearity, locality, smoothness, affine equivariance), so the surface cannot be edited by moving control vertices without retraining.

NeuBase keeps a classical Catmull–Clark base surface and learns only the **basis functions of an additive offset field**.

## Core method

The surface is `S(u) = Σᵢ (pᵢ + oᵢ·Nᵢ(u))·Bᵢ(u)` where `Bᵢ` is the Catmull–Clark basis (evaluated with Stam's exact scheme), `oᵢ` is the **fixed unit normal** of the control mesh at `pᵢ`, and `Nᵢ` is a learned **neural basis function**.

The construction that makes it work:

- Each control vertex gets a learnable feature vector `cᵢ`, interpolated by the **same** CC basis functions to form a **spline feature manifold** — which is what makes the learned functions smooth.
- An MLP maps that manifold to `K` **neural channel functions**, and `Nᵢ(u) = φ_{κ(i)}(u)·Bᵢ(u)`. Multiplying by `Bᵢ` forces `Nᵢ` to **inherit exactly the CC local support**.

**The scaling trick.** Naively `K = |P|`, one channel per control vertex, which doesn't scale. Instead, build a graph connecting control vertices that influence the *same* patch, and colour it so no two vertices influencing one patch share a channel, with channels balanced. That is exactly **equitable graph colouring**, and by the Hajnal–Szemerédi theorem `K = deg(G)+1` colours always suffice — typically 60–80 channels regardless of mesh size. On the Elephant (4000 control vertices) this is 74 channels at 224 KB, versus 4000 channels at 1240 KB for a modest accuracy gain.

**What this buys.** Because the surface is *linear* in the control positions and offsets with the basis functions fixed after training, control-mesh edits update the surface with **no retraining** — training-free deformation at 25–40 fps in Blender. The offset basis functions are exempt from partition-of-unity because the base surface already provides affine equivariance.

## Results

75 shapes from the MPZ dataset, 32 bits/parameter, typically 130–260 KB per shape.

| Method | P2S (×10⁻⁴) | Hausdorff (×10⁻³) | Normal error | Train |
|---|---|---|---|---|
| NGF | 8.401 | 14.16 | 6.41° | ~8 min |
| NeuPPS | 5.472 | 13.21 | 4.64° | ~25 min |
| **NeuBase** | **4.556** | **12.83** | **4.25°** | ~13 min |

Against plain Catmull–Clark surfaces at **matched storage**, point-to-surface error is consistently around **a quarter** of CC's (Armadillo 14.72 → 4.085 at 153 KB both). Bayon Lion compresses 25.74 MB of input to **0.19 MB**. Deformation preserves detail better than ARAP, which tends to smooth it away.

## Limitations

- **Offsets are fixed unit normals.** For planar patches all offsets can become collinear, degenerating the offset field to a scalar displacement along one normal. Making them learnable improves fitting but costs the intuitive control that is the paper's whole point.
- **The offset field cannot be subdivided.** CC control meshes can be subdivided without changing the limit surface, enabling finer local edits — but this does not extend to the offset field, since it uses neural rather than CC bases. Refining requires retraining.
- Coarse quadrangulation (QuadriFlow) sometimes fails on challenging shapes.

## Relation to other work

Cites [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] and [[Dou et al. 2024 — Differentiable Micro-Mesh Construction]] together, under *displaced subdivision surfaces*, as the triangular instance of the coarse-base-plus-offset family. NeuBase differs in using a **quadrilateral** Catmull–Clark base and **learned basis functions** rather than a stored per-patch displacement map.

None of the ray-tracing papers here are cited, and no hardware is discussed — tessellation is CPU-side, ~0.2 s for a 100K-vertex surface, with GPU kernels explicitly out of scope.

**Why it earns a place in this graph.** Read next to [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes]], the two papers describe the same structural weakness from opposite sides. NeuBase's offset field is bound to one base configuration and cannot be refined without retraining; the animated-DMM paper's displacements are bound to one rigging pose and cannot change amplitude or sign under deformation. Both are **precomputed offset fields tied to a base they no longer control**. And both answers have the same shape: keep the offset fixed and move the variation into the base — NeuBase into the control mesh, the DMM paper into a per-micro-vertex interpolated matrix.

See [[Base mesh quality objectives]] and [[Shell, prism and prismoid]].

![[NeuBase.pdf]]
