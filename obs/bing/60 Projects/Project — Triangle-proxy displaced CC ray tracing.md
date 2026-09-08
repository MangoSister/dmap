---
title: Project — Triangle-proxy displaced CC ray tracing
tags: [project, ray-tracing, displacement, catmull-clark, proxy, dda, eurographics]
status: proposal
created: 2026-08-15
target-venue: Eurographics 2027 full paper
---

# Project — Triangle-proxy displaced CC ray tracing

> [!abstract] Verdict
> The triangle-proxy route can support a strong Eurographics paper, but the weak version is borderline: ordinary CC-to-proxy baking + heuristic ray segmentation + standard DDA can be dismissed as a combination of known ingredients. The paper becomes convincing if its center is a **certified piecewise-linearization of nonlinear shell rays**, supported by an output-sensitive hierarchical DDA and, ideally, a **traversal-aware proxy construction**. The goal is not universal fastest performance; it is a better quality-time-memory tradeoff with clear wins in the intended regime.

## 1. Project route

The input is a displaced Catmull–Clark (CC) surface. Offline, it is converted into:

1. a coarse triangle proxy;
2. shared proxy vertex displacement directions/normals;
3. a residual displacement map over the proxy; and
4. a min/max hierarchy over that map.

At runtime, each proxy triangle defines the shell map

$$
F_\triangle(a,b,h)
= P(a,b)+hN(a,b),
$$

where both $P$ and $N$ are barycentrically interpolated. A world-space ray becomes a rational-quadratic curve in shell/texture space, as in [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]]. Instead of repeatedly solving nonlinear ray-boundary and ray-microtriangle equations, the proposed method:

1. decomposes the nonlinear ray into well-behaved intervals;
2. replaces each interval with certified linear segments;
3. traverses the displacement hierarchy using hierarchical 2D DDA; and
4. intersects the original world ray with leaf microtriangles generated on demand.

“Tessellation-free” means **no displacement-resolution microtessellation or microgeometry BLAS is stored**. The method openly uses a coarse triangle proxy; it is not direct intersection of the analytic CC limit patch.

## 2. Paper-strength assessment

### 2.1 Borderline version

The following package is probably insufficient or marginal for an EG full paper:

- fixed or conventionally tessellated proxy geometry;
- scalar residual obtained by a simple projection;
- midpoint-based curve subdivision with a hard segment cap;
- zero-width DDA along the approximate chord; and
- performance evaluated only against Ogaki's nonlinear traversal.

A reviewer can summarize that version as:

> Standard CC-to-displacement baking, followed by an approximate linearization of Ogaki's ray and a standard DDA.

Conversion, linear approximation, and DDA all have substantial prior art individually. DDA is an accelerator, not a standalone research contribution. A speedup over Ogaki alone is also insufficient because RMIP and PDM are stronger performance competitors.

### 2.2 Strong version

The following package is plausibly strong enough for EG:

1. **Certified linearization of nonlinear shell rays.** Analytically partition the rational curve into texture-space monotone pieces; derive a true maximum-deviation or boundary-coverage bound; and guarantee that no displacement cell crossed by the nonlinear ray is omitted.
2. **Output-sensitive hierarchical DDA.** After certification, reduce traversal to cheap min/max lookups and neighbor stepping, with work tied to the number of segments and relevant crossed cells rather than repeated nonlinear node tests.
3. **Traversal-aware proxy construction.** Choose proxy density, connectivity, and displacement directions to minimize predicted traversal difficulty—residual thickness, shell curvature, UV footprint, and conditioning—subject to an explicit geometric-error constraint.
4. **A complete GPU system and equal-quality evaluation.** Demonstrate the time-memory-quality Pareto frontier against Ogaki, TFDM, RMIP, PDM, dense triangle BLAS, and DMM/pre-tessellation.

One major traversal contribution plus a meaningful traversal-aware converter is enough. The DDA then completes and validates the system.

## 3. Core technical contribution: certified piecewise shell rays

The current research opportunity is not merely “approximate a curve using line segments.” It is to construct the **minimum or near-minimum set of segments that conservatively covers the nonlinear ray's grid traversal**.

A publishable construction should include:

- event roots for the actual texture coordinates $u(h),v(h)$, not only barycentric $a(h),b(h)$;
- monotone interval decomposition;
- a certified deviation bound over every emitted interval, not only a midpoint estimate;
- explicit treatment of denominator singularities;
- no silent fixed-segment truncation;
- conservative handling of grid-boundary ties; and
- a curve-tube or supercover DDA that visits every cell the true curve could enter.

There are two possible correctness contracts:

1. **Certified tube:** derive a UV-height tube enclosing the curve and traverse all cells intersecting that tube.
2. **Boundary-equivalent subdivision:** recursively split until it is proven that the chord and nonlinear curve cross the same ordered grid boundaries.

The first is probably simpler and more robust on the GPU. Its extra neighboring-cell work should be measured against the cost of finer subdivision.

The desired complexity story is:

$$
T_{\text{ours}}
\approx
T_{\text{setup}}
+ K\,T_{\text{segment}}
+ C\,T_{\text{minmax}}
+ L\,T_{\text{leaf}},
$$

where $K$ is the number of certified segments, $C$ the conservatively visited cells, and $L$ the leaf tests. The paper should show when this is lower than repeatedly solving nonlinear intersection equations throughout a quadtree.

## 4. Traversal-aware CC-to-proxy conversion

Plain conversion is primarily preprocessing, not automatically a contribution. The stronger direction is to optimize the proxy for both representation quality and ray-tracing cost:

$$
\min_{\text{proxy}}
\quad
M(\text{proxy})
+ \lambda\,\widehat T_{\text{traversal}}(\text{proxy},d)
$$

subject to

$$
d_H\!\left(S_{\text{proxy}+d},S_{\text{CC target}}\right)\le \varepsilon,
$$

plus watertight shared-edge constraints.

The traversal predictor can include:

- residual shell thickness;
- curvature or normal variation of the triangle shell;
- expected segment count;
- UV footprint in displacement texels;
- prism overlap and expected OptiX candidate count;
- shell-Jacobian conditioning; and
- memory cost of proxy vertices, triangles, and residual data.

This creates a genuine connection between representation and intersection. The converter is no longer “bake a map after simplification”; it chooses the proxy that makes the proposed traversal cheap.

### 4.1 Exactness caveat

For a proxy base $P(u,v)$ and displacement direction $N(u,v)$, a scalar map represents a target $T$ exactly only when the target is a single-valued normal graph:

$$
T(\phi(u,v))=P(u,v)+d(u,v)N(u,v)
$$

for a bijective correspondence $\phi$. The common projection

$$
d=\langle T-P,N\rangle
$$

retains only the normal component. The remaining tangential error is

$$
e_{\mathrm{tan}}=(I-NN^\mathsf T)(T-P).
$$

Therefore the converter must do at least one of the following:

- find an appropriate normal-line correspondence;
- refine or optimize the proxy until tangential error is below tolerance;
- accept and report an error-controlled approximation; or
- use vector displacement where scalar displacement is invalid.

Do not claim mathematical identity from a dot-product residual alone.

### 4.2 Watertightness

At a shared proxy edge, both triangles must use the same interpolated displacement direction and the same scalar edge samples, with orientation handled explicitly. Edge ownership, filtering, quantization, and min/max construction must preserve this equality. Otherwise the conversion can introduce the very cracks the CC source avoided.

## 5. Expected performance by baseline

| Baseline | Expected result |
|---|---|
| [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki nonlinear traversal]] | Plausible clear win in thin, well-conditioned shells because per-node polynomial work and cubic leaf solving are replaced by segment generation, cached min/max tests, DDA, and linear microtriangle tests. |
| [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] | Most plausible baseline to beat; affine bounds and quadtree traversal are computationally expensive. |
| [[Thonat et al. 2023 — RMIP|RMIP]] | Uncertain. Its ray-adaptive oblong bounds are designed to reduce long texture-space traversals. Standard DDA may lose on large proxy triangles and grazing rays. |
| [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] | Uncertain. PDM is approximate but cheap. Compare only after sweeping its step size to matched oracle error. |
| Dense triangle BLAS / DMM | Unlikely to beat universally in static raw tracing. The proposed wins should be memory, build/update latency, editability, or quality at equal memory. |

The strongest defensible performance claim is:

> At matched geometric accuracy, certified piecewise shell rays and hierarchical DDA accelerate thin, well-conditioned proxy shells over nonlinear traversal and TFDM, remain competitive with RMIP/PDM, and improve the memory/update tradeoff over dense tessellation.

Do not make “faster than all baselines” a requirement. Different methods dominate different regimes.

## 6. Favorable and unfavorable regimes

### Favorable

- thin residual displacement;
- mild interpolated-normal variation;
- non-grazing rays;
- small-to-moderate proxy-triangle UV footprints;
- sparse min/max overlap and good empty-space skipping;
- dynamic displacement edits where a dense BLAS would need rebuilding; and
- equal-quality comparisons where competing triangle bases require many more proxy primitives.

### Unfavorable

- grazing and silhouette rays;
- thick displacement shells;
- strongly twisted interpolated normals;
- large proxy triangles spanning thousands of texels;
- dense high-frequency maps with little empty space;
- poor UV distortion;
- segment counts large enough to cause local-memory spills or warp divergence; and
- static workloads where hardware microtriangle traversal amortizes preprocessing.

The converter should actively avoid the unfavorable proxy configurations rather than assuming thinness alone solves them.

## 7. Evaluation design

Two separate comparisons are needed.

### 7.1 Algorithm-only comparison

Give Ogaki, TFDM, RMIP, PDM, and the proposed method exactly the same triangle proxy, displacement map, filtering convention, and ray streams. This isolates the contribution of piecewise segmentation + DDA.

Required ablations:

1. Ogaki nonlinear traversal;
2. segments + original quadtree;
3. heuristic segments + DDA;
4. certified segments + zero-width DDA;
5. certified segments + tube/supercover DDA; and
6. each proxy-construction term enabled/disabled.

### 7.2 End-to-end representation comparison

Start from the same displaced CC target. Let every method choose enough proxy tessellation, residual precision, or DMM level to reach the same oracle error. Compare:

- render time;
- total resident and build-scratch memory;
- preprocessing/build/update time;
- miss and false-hit rate;
- position/depth p50, p99, and maximum error;
- normal error;
- silhouettes/Hausdorff distance; and
- quality-time and quality-memory Pareto curves.

Sweep:

- curvature and normal variation;
- residual thickness;
- displacement frequency and sparsity;
- 1K/4K/8K maps;
- front-facing through grazing views;
- coarse through dense proxies;
- primary, closest-hit secondary, and any-hit shadow rays; and
- static versus edited/deforming displacement.

## 8. Go/no-go gates

Before investing in a full converter and large dataset:

1. Make the triangle-shell traversal conservative against an independent dense-microtriangle oracle.
2. Remove silent segment, stack, and DDA-step truncation or provide conservative fallbacks.
3. Demonstrate certified segmentation on adversarial rational curves, including near-grid-boundary and near-singular cases.
4. Sweep one triangle/patch over displacement thickness, normal variation, UV footprint, incidence angle, and map resolution.
5. Beat Ogaki and TFDM at matched error in the intended regime.
6. Establish whether the method is within striking distance of RMIP and PDM before making proxy optimization a major engineering investment.
7. Only then implement traversal-aware proxy fitting and the full CC-derived dataset.

If certification makes traversal too expensive, retain the proxy pipeline but reconsider the inner traversal—e.g. a hybrid that selects DDA for short predicted spans and RMIP/quadtree-style bounding for grazing or long spans.

## 9. Recommended framing

**Working title:** *Fast Tessellation-Free Ray Tracing of CC-Derived Displaced Surfaces via Certified Piecewise Shell Rays.*

**One-sentence claim:**

> We convert displaced subdivision surfaces into traversal-optimized triangle proxy shells and replace nonlinear shell-space traversal with certified piecewise-linear rays and output-sensitive hierarchical DDA, providing a better quality-time-memory tradeoff without storing displacement-resolution geometry.

**Reviewer-visible contributions:**

1. An error-certified, grid-conservative linearization of rational shell-space rays.
2. An output-sensitive hierarchical DDA specialized to those certified segments.
3. A traversal-aware CC-to-proxy construction with watertight residual baking and explicit geometric-error control.
4. A full GPU implementation and equal-quality comparison against the strongest tessellation-free and hardware-assisted baselines.

**Non-claims:**

- not direct analytic intersection of the true CC limit patch;
- not universally faster than RMIP, PDM, or DMM;
- not exactly equivalent to an arbitrary target under scalar displacement unless the normal-graph condition is verified; and
- DDA by itself is not claimed as novel.

