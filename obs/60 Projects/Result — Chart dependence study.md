---
title: Result — Chart dependence study
tags: [project, result, phase1, phase2, bounds, parameterization]
created: 2026-09-06
---

# Result — Chart dependence study

Question: does the Taylor pyramid's bound tightness depend on the chart, the affine map from a base triangle's barycentric coordinates to the texture? Phase 1 measured tightness under the identity chart only. Code: `code/python/poc/experiments/exp10_chart_skew.py`; output `experiments/out/exp10_log.txt`. Raised and reasoned in [[Log — Conservative metric queries]] (2026-09-06).

## Setup

- A chart is the constant Jacobian J from texture coordinates (s, t) to barycentric coordinates (u, v). The base triangle parameterized by (s, t) is P(J (s, t)) and M(J (s, t)). Since P and M are affine, this is again a base triangle, with edges [e1 e2] J and normal derivatives [Mu Mv] J, built exactly by construction. Checked before use: √det G scales by |det J| to 7e-15, and the stretch eigenvalue, the normal direction, obliquity, and the normalized integrability defect are chart-invariant to rounding.
- Same texture on the (s, t) tile, same base plane, physical amplitude from the original triangle's mean edge. Then exp05's machinery unchanged: 257-node tile, 9 levels, certified width over true range per level for √det G, λmax, λmin, and the normal cone. `cc_torus` × {rock, cobble} × amplitude {0.05, 0.2} × 5 triangles.
- Charts, all with |det J| = 1 so a cell keeps its footprint area and changes only its shape: identity; anisotropic scale at condition 2 and at condition 4; rotation by 30°; shear [[1, 1], [0, 1]] at condition 2.6; mirror (det −1).

## Results

Worst median ratio (certified width over true range) at the criterion levels, cells up to 8 texels, amplitude 0.05, over √det G, λmax, and the cone. The worst case is the cone on rock at 8-texel cells under every chart.

| chart | condition | worst median | relative to identity |
|---|---|---|---|
| identity | 1.00 | 2.59 | 1.00 |
| scale 2:1 | 2.00 | 2.57 | 0.99 |
| scale 4:1 | 4.00 | 2.54 | 0.98 |
| rotate 30° | 1.00 | 2.50 | 0.96 |
| shear 1.0 | 2.62 | 3.09 | 1.19 |
| mirror | 1.00 | 2.57 | 0.99 |

Conservativeness violations: 0 under every chart, over all four quantities and all nine levels.

Per quantity at 8-texel cells, amplitude 0.05, rock, identity against shear: √det G 2.31 against 2.28; λmax 2.30 against 2.09; cone 2.59 against 3.09. On cobble the cone goes the other way, 2.30 against 2.14. The full per-level tables for every chart are in the output log.

## Findings

1. **Conservativeness is chart-generic.** 0 violations under every chart, as the construction predicts: the reparameterization is exact, and the propagation is written in the parameters without assuming they are orthogonal on the base.
2. **√det G and λmax are chart-indifferent.** Within 4% of the identity chart under every chart, including anisotropic scale at condition 4. The concern that det G = G00 G11 − G01² would cancel more under a skewed chart did not materialize: affine propagation keeps the linear correlations exactly, and the intersection with the interval route covers the rest.
3. **The normal cone is the one sensitive quantity, and only under shear.** 3.09 against 2.59 at 8-texel cells on rock, 19% looser, 3% over the Phase 1 kill ratio of 3. Rotation, anisotropic scale, and mirroring leave it within 4%. Mechanism, reasoned rather than measured: the cone radius is built from per-symbol vector norms of the affine form of S_u × S_v; under shear the two parameter directions are not orthogonal on the base, the two symbols' deviation vectors are correlated, and the componentwise radius overcounts. The fix is to build the enclosure in an orthonormalized parameter frame per triangle, which affine forms allow without loss. This is a numpy propagation item (`node_bounds.py`), not part of the path tracer plan.
4. **λmin has no meaningful per-node certificate under any chart,** ratios in the hundreds to thousands, exactly as Phase 1 found. Nothing new.

## Conclusions

- The Phase 1 kill criterion holds under general charts for the metric quantities. For the normal cone it holds under rotation, scaling, and mirroring, and is missed by 3% under shear at condition 2.6. Until the frame fix lands, consumers of the cone at 8-texel cells (the emitter cosine, which S7 already switched off; the feature-size localization of B2) should expect the shear case.
- The reparameterized-triangle construction is exactly what the path tracer plan's T1 needs: build the base triangle in texture parameterization, and every pointwise formula, the propagation, and the samplers run unchanged; only the domain clip and the tile sharing are new.

Related: [[Result — Phase 1 Taylor pyramid]] · [[Plan — Path tracer with displaced surfaces]] · [[Taylor-model bound pyramid]] · [[Project — Conservative metric queries without tessellation]] · [[Log — Conservative metric queries]]
