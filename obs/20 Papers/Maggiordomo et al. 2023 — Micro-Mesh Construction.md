---
title: Micro-Mesh Construction
authors: [Andrea Maggiordomo, Henry Moreton, Marco Tarini]
affiliation: University of Milan; NVIDIA
year: 2023
venue: ACM TOG 42(4), Article 121 — SIGGRAPH 2023
doi: 10.1145/3592440
project: https://micromesh.di.unimi.it/
tags: [paper, micro-mesh, construction, decimation, compression]
status: read
---

# Micro-Mesh Construction

The reference method for turning an existing multi-million-triangle asset into a **displaced micro-mesh (µ-mesh)**. Every other construction or compression paper in this vault measures against it: [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] call it "Staged" and use its 121-mesh dataset; [[Barczak et al. 2024 — DGF|DGF]] takes all its DMM error, size and encoding-time figures from this paper's tables.

## Problem

µ-meshes are a hardware-supported primitive (see [[NVIDIA 2022 — Ada Lovelace Architecture]]), but *constructing* them was unexplored, and existing remeshers and displacement bakers are ill-suited. The paper identifies the requirements and resolves four conflicting goals — a coarse base mesh, reprojectability, isotropy, and small prismoid volume. See [[Base mesh quality objectives]].

## The µ-mesh format (as this paper defines it)

A **base mesh** carries positions *and* displacement vectors at its vertices. Each base face is divided into a regular grid of `4^k` µ-triangles by a **per-face subdivision level `k`**. At each µ-vertex, base position and direction are barycentrically interpolated, the direction scaled by a **per-µ-vertex scalar displacement**, and added. See [[Micro-triangle and subdivision level]].

**Bit-exact watertightness** comes from shared base positions, shared direction vectors, duplicated displacement values on shared edges, and a per-edge **decimation control bit** that halves the segments along that edge — which imposes the constraint that adjacent subdivision levels may differ by **at most one**.

Scalar displacements are stored per base face in a **µ-map** of `(2^k+1)(2^(k−1)+1)` **11-bit** normalised values. The displaced surface is bounded by the **prismoid** between the base face and the face formed by the displacement-vector tips — a property exploited in both BVH construction and traversal. Four watertight LODs are supported, and hardware ray queries may apply an **LoD bias** dropping subdivision level by up to three (1/64 the µ-triangles) while staying bit-exact watertight.

## Pipeline

**0. Input preparation.** Local topology transformations until no edge exceeds 5× the average length.

**1. Displacement directions.** The **visibility value** of a base vertex is `V(v) = max_d min_{n∈N} (d·n)` over the adjacent face normals; the maximiser is the displacement direction. Only strictly positive visibility is admissible, since negative values imply vanishing interpolated directions inside faces. Naïve area-weighted vertex normals produce visible artefacts. Computed by a **Welzl-style algorithm** over an active subset of 2 or 3 normals — **more than 26× faster** than the quadratic-programming formulation of Jiang et al. [2020] (0.64 s vs 17.24 s for 1M instances).

**2. Base mesh by coarsening.** Edge collapses with quadric error metrics, modified by a tangential smoothing term. The aggregate cost divides geometric error by penalty factors for **normal deviation**, **aspect ratio** and **visibility**, so a zero in any of the three makes the cost diverge and the collapse is never performed. Scheduling is two-phase: an approximate randomised phase with an adaptive cost threshold, switching to an exact priority queue below 1M faces — justified because collapse order matters less far from the final base mesh. Isotropy is enforced *adaptively*, tracking each face's best-ever aspect ratio rather than a hard floor, which avoids locking simplification early. The system determines its own coarsest base mesh.

**3. Subdivision levels.** Either *uniform µ-triangle area* (a global level plus a per-triangle area correction) or *adaptive*, which simulates a one-level increase, projects the new µ-vertices onto the input, and keeps candidate level increases in a priority queue sorted by predicted error reduction. Both operate per-face and can violate the ±1 constraint, so a correction phase conservatively raises levels until no offending pair remains, then sets edge decimation flags.

**4. Displacement baking.** Tessellate, then **ray-cast each µ-vertex along its interpolated direction against the original mesh**. Only coherently oriented faces are intersected, for robustness against self-intersections. Outliers — rays travelling too far, or missing — are cleared and **interpolated from valid neighbours**, exploiting the smoothness of the displacement field.

**5. Prismoid tightening.** For each base vertex, take the min and max displacement over the star of adjacent faces and shift/rescale the base position and direction length accordingly. Using **local** rather than global bounds shrinks total displacement volume by more than **3×** (2.15×10⁶ → 0.63×10⁶ units³) — and this single step yields a **3–4× rendering speedup**.

**Extensions:** detailed boundaries (redefining boundary directions tangentially and adding "flap" triangles for the rays to hit), and a **displacement-aware ARAP** reparameterisation that sums distortion over displaced *micro*-triangles rather than base triangles, recovering 22–40% of the distortion increase displacement causes.

## Results

121 ThreeDScans models, mean 1.7M input triangles: **average 15:1 compression in under 5 minutes per model**, typical isotropy 0.72–0.80, average geometric error under 4×10⁻⁵ of the bounding-box diagonal.

On an RTX 4090 versus an equivalent triangle mesh: **BVH size ÷6, BVH build time ÷4**, ray tracing a median **1.3× slower** (µ-triangles are generated dynamically then intersected conventionally). Scalar displacements are **85–90% of final storage**, and **7 bits already suffices** to avoid noticeable artefacts — 11 is used conservatively.

Against baselines at 25K faces: MeshLab error 2.14×10⁻⁵ / isotropy 0.78, Simplygon 2.20×10⁻⁵ / 0.65, **this method 2.07×10⁻⁵ / 0.83**. MeshLab and Simplygon base meshes produce self-intersections when displaced because they don't guarantee coherently oriented directions. Against bijective shells: distance 6.37×10⁻⁵ vs 9.73×10⁻³, in 40 s vs 2 min.

## Limitations

Tracing is a median 1.3× slower than the equivalent uncompressed mesh. Below ~1/64 base-face reduction the memory gains vanish because displacements dominate. Inputs usually need a preliminary topological cleanup. Non-positive-visibility configurations are tolerated but not solved. The explicitly flagged open problem is **how to share textures or UV maps across levels of detail**, given µ-meshes are intrinsically multi-resolution.

## Relation to other work

The earliest of the three construction/compression papers here and cited by both. [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] attack precisely its *staged* structure — each stage minimising a local surrogate that is not the µ-mesh's true objective. [[Barczak et al. 2024 — DGF|DGF]] attacks the *format*, arguing scalar displacement over a base triangle cannot represent arbitrary topology, and uses this paper's toolchain to produce its DMM failure case.

It is also cited from the rendering side, by [[Thonat et al. 2023 — RMIP|RMIP]] and [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]], and scoped *out* by [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] as displacement rather than opacity.

> [!warning] The format outlived the hardware
> This paper's premise is that hardware natively ray traces µ-meshes. That hardware path was **withdrawn in February 2025** — see [[Graphics API and hardware support timeline]]. The construction algorithms remain valuable; the deployment target changed.

![[micromesh_construction_LOWRES.pdf]]
