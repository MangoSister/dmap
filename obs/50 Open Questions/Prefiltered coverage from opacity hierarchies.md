---
title: Prefiltered coverage from opacity hierarchies
tags: [open-question, research-direction, opacity, antialiasing, lod, priority]
priority: 3
rests-on: "Chernaik et al. 2026, stated open challenge"
---

# Prefiltered coverage from opacity hierarchies

> [!note] This is analysis, not a claim from any paper
> The enabling structure and the open challenge are Chernaik et al.'s; the applications are mine. See [[MOC — Open Questions]].

> [!success] Confirmed open — and sharpened — 2026-08-10
> I flagged this as the highest concurrent-work risk, because [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|NVIDIA's foliage paper]] sits right on top of it. Having now read that paper: **it does not do this, and it explicitly chose the opposite.** Details in *The baseline to beat* below. This direction is stronger than when I wrote it, not weaker — it now has a named, measured competitor.

**The gap.** [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] added an **aggregate hierarchical opacity** to every interior node of their DAG — the fraction of non-empty leaf sub-triangles in that subtree — using 16 bits that were otherwise free. It is a geometrically correct, ray-traceable coverage mipmap in barycentric space. They then say:

> Efficiently integrating the hierarchical opacity into a rendering system is still a challenge.

The structure was published two months ago and is essentially unexploited. This is the only direction here whose enabling data already exists in a shipping-quality implementation.

## What it could fix

**Alpha-test aliasing on foliage** is one of the ugliest surviving artefacts in real-time rendering. A binary alpha test produces hard edges; under minification a leaf either exists or vanishes, and the result crawls and sparkles. The standard mitigations — alpha-to-coverage, hashed alpha testing, MSAA, stochastic transparency — all *estimate* coverage from point samples of a texture.

Aggregate opacity is different in kind: it is the **exact** fraction of covered sub-triangles at a chosen level, obtained during traversal with no extra texture fetch, at the LoD the ray actually needs (they select depth from screen-space area as `⌊log₁₆ A⌋`). That is the right quantity, computed in the right place, already prefiltered.

Note that this is a **quality** contribution rather than a speed one, which is precisely why the field has skipped it. See [[Any-hit shader cost]] for why every prior opacity paper was measured in milliseconds.

## Open sub-problems

**1. Correlation and noise.** Using a coverage value as a per-ray probability trades aliasing for noise. Getting a clean image needs correlated or blue-noise decisions across rays and frames, and interacts with denoisers. Unstudied for this structure.

**2. Coverage is not separable from shading.** A half-covered sub-triangle is not just half-opaque — the surviving half has its own average normal and colour. Correct prefiltering means aggregating *appearance*, not just presence, in the spirit of normal-distribution prefiltering (LEAN/LEADR) or the prefiltered occlusion work Chernaik et al. cite. This is the deep version of the problem and nobody has attempted it on a micromap.

**3. Direction dependence.** Chernaik et al. note their aggregate opacity is **direction-independent**, unlike true prefiltered occlusion. Real foliage occludes anisotropically — a leaf cluster is far more opaque edge-on than face-on. Storing a small directional basis (a few SH coefficients, or a coarse visibility cone) per node is the obvious extension, and the memory is nearly free given their compression ratios.

**4. Transmittance through many layers.** A shadow ray through a canopy currently resolves many partial hits. A single aggregate query that returns transmittance through a subtree would collapse that — but requires handling correlation between layers rather than assuming independence.

## Where it leaves rendering

This is the direction with the clearest non-graphics payoff, because "what fraction of light passes through this canopy" is a real scientific question:

- **Agronomy and forestry** — canopy light interception drives crop-growth and yield models.
- **Remote sensing** — LiDAR and radiative-transfer simulation through vegetation, used to calibrate satellite retrievals.
- **Solar and building design** — shading estimates through trees and perforated façades.

Those communities currently use crude analytic canopy models (Beer–Lambert with a leaf-area index) precisely because explicit geometry is intractable. A hierarchical, compressed, queryable coverage structure over real scanned vegetation is a plausible bridge — and [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|CSM]] compresses a 27-million-triangle scene's opacity data to single-digit megabytes.

## The baseline to beat

[[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|van Antwerpen et al.]] hit exactly this problem at production scale and solved it **stochastically rather than by prefiltering**:

> We explicitly avoid representing partial opacity using the unknown OMM state, thereby avoiding any-hit overhead. Instead, we encode opacity in OMMs using only the two discrete states fully opaque or fully transparent. We use **OMM dithering** to approximate partial opacity, yielding comparable Monte Carlo noise to stochastic hit rejection but without any-hit overhead.

They compute partial coverage at bake time, then **throw the fraction away**, keeping only a probabilistic decision to discard opaque micro-triangles so that aggregate coverage matches (without it, screen coverage is 11% too high and trees render too dark). They also reject prefiltering for shading attributes outright — *"Rather than prefiltering normals and UVs and storing distributions, we rely on the fine resolution of the OMM to capture sufficient point-sampled variation"* — and delegate the resulting minification aliasing to a denoiser, which their limitations section concedes.

Their one mip-like structure, the 32-bit residence mask, is described as *"a compact, 16× sub-sampled mipmap of the higher-resolution opacity information"* — but it is a **binary occupancy mask used to index a sparse shading buffer**, never a filtered coverage value. There is no per-node aggregate opacity anywhere in their hierarchy, no SH, no visibility cones. Their only aggregate-appearance model is a quadratic view-dependent normal nudge **fitted to a randomly oriented triangle soup**, which is precisely the sort of hack a real directional prefilter would replace.

So the research question becomes concrete and comparative: **does prefiltered coverage beat dithering-plus-denoiser?** Dithering trades aliasing for Monte Carlo noise and leans on DLSS; prefiltering would trade it for bias and bounded memory. That is a measurable comparison against a strong, published, production-scale baseline — a far better position than an open-ended claim of novelty.

## Risks

- Aggregate opacity is defined over *sub-triangles*, not over a pixel footprint; the mapping between the two is only approximate at grazing angles.
- The value is only as good as the bake, and micromap quality is hostage to UV layout — see [[Authoring, UV layout and micromap efficiency]].
- The dithering baseline is genuinely good and costs almost nothing at runtime. Prefiltering must justify its memory and its bias against a competitor that is essentially free.
- Note the two papers pull in opposite directions on the 4-state format: [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] observe `unknown` states become rare at high subdivision, while van Antwerpen et al. refuse `unknown` entirely to avoid any-hit. Both are arguments that the useful signal is *coverage*, not *classification*.

Related: [[Micromap]] · [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps]] · [[Hybrid displacement and opacity micromaps]]
