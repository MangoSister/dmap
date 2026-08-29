---
title: Result — A-MVP receiver irradiance study
tags: [project, result, phase2, sampling]
created: 2026-08-28
---

# Result — A-MVP receiver irradiance study

S7 of [[Plan — A-MVP sampling implementation]]; the A-MVP experiment of [[Project — Conservative metric queries without tessellation]] §5A. Code: `code/cpp/src/test_weighted_area_sampling.cpp`, config `code/data/configs/test_weighted_area_sampling.toml`. Raw per-receiver data: `code/data/results/s7/irradiance_{rock,cobble}.csv`. Reasoning and diagnostics in [[Log — Conservative metric queries]] (2026-08-28).

## Setup

- One emissive displaced triangle (cc_torus triangle 8533), 65-node height grid, gaussian-spot emission (peak 4.0, floor 0.1) on 64×64 texels. Sweep: {rock, cobble} × amplitude {0.05, 0.2}.
- Receivers: 4 seeded sphere directions × distances {0.6, 2, 8} mean edges from the displaced center, normals facing the surface. Unoccluded irradiance ∫ E cosθ_r⁺ |cosθ_e| / r² dA.
- One occluded pass (rock only): 4 grazing receivers at 15° elevation, 1.5 mean edges, self-shadow rays against the S2 mesh, visibility in the integrand for samplers and reference alike.
- References by dense per-texel quadrature; every estimate passed the correctness gate (within 4 SE + 1% of its reference; worst normalized deviation 2.8). All estimators are unbiased, so relative MSE at N samples is the reported per-sample relative variance divided by N.
- Ladder: uniform (Ling line casting, hits reweighted from mesh area to smooth area), emission-only table, product table, area-only descent, product descent, receiver-aware descent with and without the midpoint emitter cosine. Descent floor β = 0.05. No weight caching (see cost caveat below).

## Per-sample relative variance

Geometric mean over the 4 directions per distance. Distances in mean edges.

| config | dist | uniform | emission table | product table | area descent | product descent | receiver (midpoint cos) | receiver (no cos) |
|---|---|---|---|---|---|---|---|---|
| rock 0.05 | 0.6 | 36.5 | 0.62 | 0.58 | 2.58 | 0.60 | 2.08 | **0.28** |
| rock 0.05 | 2 | 33.1 | 0.32 | 0.30 | 2.24 | 0.31 | 0.86 | **0.26** |
| rock 0.05 | 8 | 32.9 | 0.28 | **0.26** | 2.22 | 0.27 | 0.99 | 0.26 |
| rock 0.2 | 0.6 | 42.8 | 0.85 | 0.52 | 3.87 | 0.70 | 6.3 | **0.43** |
| rock 0.2 | 2 | 39.3 | 0.60 | **0.33** | 3.38 | 0.46 | 10.6 | 0.41 |
| rock 0.2 | 8 | 39.2 | 0.57 | **0.31** | 3.34 | 0.43 | 15.5 | 0.42 |
| cobble 0.05 | 0.6 | 47.4 | 0.78 | 0.61 | 2.95 | 0.68 | 4.05 | **0.45** |
| cobble 0.05 | 2 | 43.8 | 0.54 | **0.40** | 2.66 | 0.45 | 8.3 | 0.42 |
| cobble 0.05 | 8 | 43.7 | 0.52 | **0.38** | 2.65 | 0.43 | 7.7 | 0.42 |
| cobble 0.2 | 0.6 | 42.7 | 0.93 | **0.36** | 3.92 | 0.86 | 11.4 | 0.57 |
| cobble 0.2 | 2 | 39.2 | 0.71 | **0.20** | 3.59 | 0.61 | 20.6 | 0.57 |
| cobble 0.2 | 8 | 39.1 | 0.69 | **0.19** | 3.59 | 0.58 | 29.2 | 0.58 |
| rock 0.05 occl | 1.5 | 53.9 | 0.83 | 0.80 | 4.15 | 0.77 | 0.67 | **0.58** |
| rock 0.2 occl | 1.5 | 42.1 | 0.54 | **0.52** | 4.10 | 0.63 | 0.53 | 0.55 |

Draw + evaluation cost per sample (unoccluded, roughly constant across configs): uniform ~100 ns/line, tables ~150 ns, area/product descent ~900–1000 ns, receiver-aware descent ~1150–1290 ns. Under the occluded pass the shadow ray adds ~330–730 ns to every sampler.

## Findings against the expected shape (plan S7, stated before measuring)

1. **Far-field ordering confirmed.** Uniform is 50–150× worse than every informed sampler (it also pays a ray cast per line). Emission-only trails the product samplers, and the margin grows with amplitude, as metric awareness should predict: at amplitude 0.05 the two tables nearly coincide (√det G ≈ 1), at 0.2 the product table is 1.6–3.7× better than emission-only.
2. **Product descent ties the product table at amplitude 0.05 exactly, and trails by 1.3–3.1× at 0.2.** A β = 0.01 rerun moved the far-field rock 0.2 number only from 0.43 to 0.41 (table 0.31), so the floor is not the cause. The cause is the coarse-level midpoint √det G: branch weights at coarse levels misallocate mass between subtrees, and no later level can correct the product of probabilities already taken. The error grows with amplitude and is absent when √det G is constant. The plan's rule was "notably worse is a bug in the weights"; this is a model limitation of the midpoint composition, not an implementation bug (the pdf contract is validated bit-exactly in S6). Known fix directions: per-node sums of per-texel E·√det G (the fused product bake, which surrenders base independence) or certified interval midpoints.
3. **The midpoint emitter cosine is harmful; dropping it makes receiver-aware descent the best sampler near-field.** With the cosine from the node's midpoint normal, receiver-aware descent is the worst descent everywhere unoccluded (up to 90× worse than the no-cosine variant far-field on cobble 0.2). A single normal cannot summarize a rough cell's cosine integral at coarse levels, and misweighted cells with real mass produce heavy tails. Without the cosine (weights = product × cosθ_r⁺/r² at the cell's spatial center), the variant beats the product table at 0.6 mean edges in 3 of 4 configs (up to 2.1×), matches product descent far-field, and is best or tied in the occluded grazing pass. This sharpens §5A's cone-saturation caveat: the emitter-cosine factor needs a cone-aware bound (certified propagation) or should be omitted; a point estimate is worse than nothing.
4. **Heavy tails are the failure mode of wrong weights with a small floor.** The β = 0.01 diagnostic run tripped the correctness gate twice, both on the midpoint-cosine variant far-field: one estimate 6% low at a deceptively small empirical SE, one power failure. Rare high-weight samples that 65k draws never hit bias both the estimate and the SE downward. The floor is what bounds these tails; β = 0.05 passed everything.
5. **Equal time, current implementation: the tables win unoccluded.** The descent walk costs 6–8× a table's binary search while its best unoccluded variance win is 2.1×, so the product table is the equal-time winner on this single-triangle setup. Two qualifiers. First, no caching: every descent weight is recomputed per sample, including the receiver-independent factors; the recorded caching design (plan S7) would collapse area/product descent to table-speed lookups and cut the receiver-aware walk to its per-receiver arithmetic. Second, with visibility rays in the loop the per-sample cost gap compresses (occluded pass: cost ratio 1.5–2.6× instead of 6–8×), and the equal-time comparison approaches the equal-sample one; in the rock 0.05 occluded pass the no-cosine variant already ties the emission table on efficiency.

## Cost per footprint (64×64 texels)

| | build | memory | 10× tile reuse | 100× reuse | amplitude or geometry edit | emission edit |
|---|---|---|---|---|---|---|
| product table | 0.17 ms | 16 KB | 1.7 ms / 165 KB | 17 ms / 1.6 MB | re-bake all copies | re-bake all copies |
| hierarchy (pyramid + emission sum) | 0.04 ms | 298 KB | 0.04 ms / 298 KB | 0.04 ms / 298 KB | none (node data linear in scale, face constants at query) | 0.01 ms refold |

- **Per footprint the table is ~19× smaller, contrary to the memory-tie expectation in §5A** (1 float per texel versus 6 double Taylor channels plus the emission sum over all levels). The hierarchy's memory case needs tile reuse of roughly 19× or more, about 9× with float32 storage (open decision). Accounting nuance: sampling weights use only h0, gu, gv and the emission sum (~175 KB); the remainder channels (r, ru, rv) are the certificates shared with the other applications of the representation.
- Build and edit costs favor the hierarchy at any reuse factor, and the amplitude-edit column is the structural win: the pyramid never rebuilds (current code bakes the per-face scale for convenience, but every node channel is linear in it, so one unscaled per-tile build serves all faces and amplitudes).

## Conclusions for the gate

- The product samplers deliver the §5A success criterion against baselines (1), (2), (4): 5–19× variance versus emission-only at high amplitude aggregated with area-only, 50–150× versus uniform.
- Against baseline (5), the expected far-field parity holds at low amplitude; at amplitude 0.2 the midpoint weights concede 1.3–3.1×, with a known cause and fix directions. The near-field and occluded wins belong to the receiver-aware variant without the emitter cosine, which only a hierarchy can do.
- The strongest measured case for the hierarchy is dynamics and reuse (edit columns above), plus receiver awareness; its weakest is per-footprint memory and per-sample cost without caching. These now have numbers instead of expectations.

Related: [[Plan — A-MVP sampling implementation]] · [[Log — Conservative metric queries]] · [[Project — Conservative metric queries without tessellation]]
