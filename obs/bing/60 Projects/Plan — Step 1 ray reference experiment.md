---
title: Plan — Step 1 ray reference experiment
tags: [plan, step-1, ray-tracing, displacement, first-order, certified-bounds, experiment, dataset, metrics]
status: specification-frozen-before-implementation
created: 2026-08-23
updated: 2026-08-23
parent: "[[Plan — Ray application method, prototype, and baselines]]"
implementation-status: planning-only
---

# Plan — Step 1 ray reference experiment

> [!abstract] Step 1 decision
> Step 1 is a **CPU reference experiment**, not the final traversal and not a GPU benchmark. It asks whether the certified first-order mechanism is correct, materially tighter than min/max in the intended regimes, and still useful after rational-ray certification. It uses recursive quadtree traversal and exact analytic microtriangle leaves. It contains **no DDA, CUDA changes, external-baseline timing, Catmull–Clark surface, or paper speed claim**. Passing this step authorizes only the narrow fixed-segmentation GPU slab ablation described in the parent plan.

> [!warning] No implementation before review
> This file freezes the experiment contract before any Step 1 code is changed. The dataset IDs, ray families, metrics, plots, and gates must be reviewed first. Later implementation may fix a bug or an impossible parameterization, but it must record the deviation here rather than silently changing the success criteria after seeing results.

## 1. Questions Step 1 must answer

Step 1 separates five questions that earlier small examples mixed together:

1. **Correctness:** Do the hierarchy, event intervals, certified curve tubes, and exact leaves reproduce the exhaustive analytic oracle without candidate omissions?
2. **H1 — hierarchy tightness:** After subtracting the stored local plane, is the certified residual materially narrower than scalar min/max at useful hierarchy levels?
3. **Fixed-segmentation ray benefit:** With exactly the same global certified ray segments and exact leaves, does first-order slab rejection reduce node or leaf work relative to min/max?
4. **H3 — affordable certification:** Do segment counts, tube inflation, conditioning fallbacks, and grazing-ray behavior remain plausible rather than erasing the pruning benefit?
5. **Directional lazy benefit:** With the same componentwise UV ownership tube, does direct certification of $\mathbf g^T\Delta\mathbf x$ reduce lazy splits and local linearizations relative to componentwise projection?

Questions 2 and 3 isolate the first-order surface representation. Question 5 is a stronger, optional contribution: it tests whether the same first-order slope also improves the ray-side segmentation mechanism. A failure of Question 5 does not automatically kill Questions 2–3.

## 2. Frozen method matrix

All non-oracle variants query the same analytic triangle-shell image of the same piecewise-affine texture microtriangles and call the same exact cubic leaf solver.

| ID | Ray segmentation | Node rejection | Purpose in Step 1 |
|---|---|---|---|
| `ORACLE` | None | None; exhaust every microtriangle | Independent closest-hit and complete hit-set reference |
| `G-MM` | Global certified componentwise tubes | Scalar min/max | Zero-order same-surface control |
| `G-FO` | The exact same global tubes as `G-MM` | First-order plane plus remainder, widened componentwise | Isolates the first-order hierarchy |
| `L-CW` | Lazy node-local chords | First-order, $E_{cw}=|g_u|\epsilon_u+|g_v|\epsilon_v$ | Coupled traversal with componentwise projection |
| `L-DIR` | Lazy node-local chords | First-order, direct bound on $\mathbf g^T\Delta\mathbf x$ | Tests the first-order-specific directional certificate |

The existing independent-height and first-order world AABBs may be recorded as a **diagnostic appendix ablation**, but they are not Step 1 success criteria and are not substitutes for the complete TFDM baseline.

### 2.1 What is deliberately absent

- No DDA or tube-supercover DDA. “Nodes” and “leaves” in this plan mean recursive quadtree work.
- No GPU milliseconds, cache, occupancy, or bandwidth conclusions.
- No RMIP, PDM, TFDM, DMM, or dense-triangle performance comparison yet.
- No change to the exact cubic leaf equation.
- No planar-world-microtriangle result used as truth.
- No area-sampling experiment; that receives its own plan after the ray reference gate.

## 3. Surface, height-grid, and normalization contract

### 3.1 Grid contract

- A case with `cells=N` uses an $(N+1)\times(N+1)$ scalar height grid.
- Every texture cell uses the frozen diagonal and two affine height microtriangles.
- The proxy footprint, texture transform, shell map, edge ownership, and exact leaf surface are identical for every method.
- Main decision runs use `cells=32`; resolution sweeps use `16`, `32`, and `64`.
- The default chord-tube target is $\eta=0.25$ leaf texel. The sensitivity sweep uses $\eta\in\{1,0.5,0.25,0.125,0.0625\}$.

The 32-cell decision grid keeps exhaustive cubic enumeration practical. The resolution sweep checks that the conclusion is not an artifact of that size. It is not intended to predict 4K texture bandwidth.

### 3.2 Scalar image conversion

For file-backed maps:

1. decode at native bit depth;
2. use the red channel as the scalar height and ignore alpha;
3. record the original mode, bit depth, dimensions, and SHA-256 hash;
4. convert integer values to $[0,1]$ using the full native integer range;
5. take the frozen crop defined below;
6. compute its 1st and 99th percentiles; and
7. map those percentiles to $[0.2,0.8]$, clamping outside values.

This equal-amplitude normalization makes Step 1 primarily a test of spatial structure. A later GPU experiment must additionally sweep physical displacement amplitude.

### 3.3 File-backed crop rules

Two deterministic crop modes are allowed:

- **Macro crop:** source window $[W/4,3W/4)\times[H/4,3H/4)$, converted to a float single-channel image and resized to $(N+1)^2$ with Pillow `Image.Resampling.BOX`.
- **Native crop:** the centered $(N+1)\times(N+1)$ source samples with no filtering.

The decision corpus uses all macro crops and only the two declared native crops. No visually favorable crop may be selected after looking at query results.

## 4. Dataset manifest

### 4.1 Procedural height fields

| ID | Definition or construction | Class | Role |
|---|---|---|---|
| `P00-constant` | $h=0.47$ | Null case | Min/max should already be optimal; catches overhead and false claimed gains |
| `P01-ramp-u` | $h=0.2+0.6u$ | Exact plane | Best-case first-order sanity check; residual should be zero up to rounding |
| `P02-ramp-oblique` | $h=0.12+0.58u+0.17v$ | Exact oblique plane | Tests two-component slope and directional cancellation |
| `P03-sine-bump` | Current two-cycle sine/cosine field plus one Gaussian bump | Smooth coherent | Main procedural H1 case |
| `P04-smooth-noise-sNN` | Deterministic white noise followed by six fixed five-point smoothing passes | Piecewise smooth | Seeds `07`, `19`, and held-out `41` |
| `P05-impulse-center` | Constant 0.2 plus one 0.9 center sample | Sparse feature | Tests whether a local feature pollutes coarse parents |
| `P05b-impulse-offcenter` | Same impulse at frozen off-center coordinates $(\lfloor0.31N\rfloor,\lfloor0.67N\rfloor)$ | Sparse feature | Avoids a center-alignment artifact |
| `P06-checker-1` | Alternating 0.2/0.8 samples | Adversarial high frequency | Expected no first-order advantage |
| `P07-checker-4` | Four-cell-wide 0.2/0.8 blocks | Structured discontinuities | Separates discontinuity frequency from amplitude |
| `P08-white-noise-sNN` | IID uniform heights | Adversarial unstructured | Seeds `07`, `19`, and held-out `41` |

The current `ramp`, `smooth`, `smooth_noise`, `impulse`, `checker`, and `noise` generators are reusable. New variants must be deterministic functions of `(dataset_id, cells, seed)`.

### 4.2 File-backed height fields

| ID | Source | Native format | Frozen Step 1 variant |
|---|---|---|---|
| `R00-test-macro` | `data/test_disp.png` | 1024², 8-bit L | Macro crop |
| `R01-rock-macro` | `data/disp_rock.png` | 1024², 8-bit L | Macro crop |
| `R01b-rock-native` | `data/disp_rock.png` | 1024², 8-bit L | Native crop; held-out texture-scale check |
| `R02-cobble-macro` | `data/disp_cobble.png` | 1024², 8-bit L | Macro crop |
| `R03-terrain-macro` | `data/disp_terrain.png` | 1024², 8-bit L | Macro crop |
| `R04-brick-macro` | `data/disp_brick.png` | 1024², RGBA | Macro crop |
| `R05-leather-macro` | `data/disp_leather.png` | 1024², 16-bit scalar | Macro crop |
| `R05b-leather-native` | `data/disp_leather.png` | 1024², 16-bit scalar | Native crop; held-out high-frequency check |
| `R06-sandrock-macro` | `data/disp_sandrock.png` | 1024², RGB | Macro crop |

The 2K/4K rock files are reserved for the later GPU memory/cache sweep; they do not add new spatial content to this CPU gate.

### 4.3 Map descriptors

Each generated grid records the following descriptors before any ray is traced:

- minimum, maximum, mean, and standard deviation;
- RMS and p95 gradient magnitude over the two affine microtriangles per cell;
- RMS discrete Laplacian;
- least-squares plane residual RMS and $R^2$;
- high-frequency energy fraction above one quarter of Nyquist; and
- fraction of cells containing the top 5% of gradient magnitudes.

These values are explanatory covariates, not post-hoc filters. Every declared case stays in the report.

## 5. Triangle-shell manifest

All shells use base positions $[(0,0,0),(1,0,0),(0,1,0)]$ and texture coordinates $[(0,0),(1,0.12),(0.16,1)]$. The current twisted directions are

$$
n_0=(-0.04,-0.02,0.30),\quad
n_1=(0.10,-0.03,0.26),\quad
n_2=(-0.03,0.11,0.27).
$$

Let $\bar n=(n_0+n_1+n_2)/3$. Define

$$
n_i(s_n)=\bar n+s_n(n_i-\bar n).
$$

| ID | $s_n$ | Meaning | Use |
|---|---:|---|---|
| `S00-parallel` | 0 | Parallel displacement directions; shell ray is near the affine control case | Main corpus |
| `S01-twisted` | 1 | Current ordinary twisted shell | Main corpus |
| `S02-stress` | 3 | Strong direction variation, visibly curved shell-space rays | Main corpus and H3 stress |

Every shell/map combination must receive a conservative shell-Jacobian regularity check over the complete height interval. A case that cannot certify regularity is classified as an expected shell fallback, not silently retained as an ordinary case.

A separate sensitivity sweep multiplies all $n_i$ by thickness scale $s_h\in\{0.5,1.0,1.5\}$. This sweep is not mixed into the main aggregate.

## 6. Frozen corpus tiers

### 6.1 Development smoke set

- `cells=16`, $\eta=0.25$.
- Maps: `P00`, `P01`, `P02`, `P03`, `P04-s07`, `P06`, `P08-s07`.
- Shells: `S00`, `S01`.
- 64 rays per case using the first samples of the core ray families.
- Seed 7.

Purpose: fast regression while implementing persistence and plots. These results are not used alone for a go decision.

### 6.2 Frozen decision corpus

- `cells=32`, $\eta=0.25$.
- Procedural maps: `P01`, `P02`, `P03`, `P04-s19`, `P05b`, `P06`, `P07`, `P08-s19`.
- File-backed maps: all seven macro crops `R00`–`R06`.
- Shells: `S00`, `S01`, `S02`.
- 256 rays per map/shell case.
- Frozen ray seed 73.

This is 15 map instances × 3 shells × 256 rays = **11,520 ordinary rays**. Exact float-oracle results are required for all. Two frozen rays per map/shell case are also exhaustively cross-checked by the independent 80-digit solver.

### 6.3 Held-out checks

- `P04-s41`, `P08-s41`, `R01b-rock-native`, and `R05b-leather-native`.
- Shells `S01` and `S02`.
- 256 rays per case with ray seed 101.
- No algorithm threshold may be tuned on these cases.

### 6.4 Resolution, tube, and thickness sweep

Sentinel maps: `P02`, `P03`, `P06`, `P08-s19`, `R01`, and `R05`.

- `cells ∈ {16,32,64}`;
- $\eta\in\{1,0.5,0.25,0.125,0.0625\}$;
- shells `S00`, `S01`, and `S02`;
- 128 frozen rays per case; and
- thickness scale $s_h\in\{0.5,1.0,1.5\}$ in a separate sweep at `cells=32`, $\eta=0.25$.

The oracle, rational-ray coefficients, and valid intervals are cached per ray and reused across $\eta$ values. This sweep produces a Pareto curve rather than selecting the best $\eta$ independently for every map.

### 6.5 Saved-seed fuzz set

- 32 seeds not used by the development or decision corpus.
- 64 random rays for each of four sentinel cases: smooth/twisted, smooth/stress, noise/twisted, and real-rock/stress.
- `cells=16` first; rerun any failing seed at 32 and save the complete minimized witness.

Fuzzing is a correctness gate, not a source of favorable work-count averages.

## 7. Ray-family manifest

Each ordinary map/shell case has exactly 256 rays. Construction is deterministic in barycentric/texture space and then mapped to the represented analytic surface where appropriate.

| ID | Count | Construction | Primary purpose |
|---|---:|---|---|
| `F00-aimed` | 64 | Hammersley targets inside the proxy triangle; incidence angles $0°,25°,50°,70°$ with stratified azimuth | Ordinary closest-hit distribution |
| `F01-free` | 64 | Origins on an expanded shell AABB and directions toward stratified points in a 1.2× expanded AABB | Natural mix of hits and misses without forcing the result |
| `F02-grazing` | 48 | Surface targets with incidence $78°,84°,88°$ and stratified azimuth | Curve length, tube width, and silhouette stress |
| `F03-boundary` | 48 | Grid lines, grid corners, fixed microtriangle diagonals, and proxy edges; exact and `nextafter` offsets on both sides | Watertight ownership and event endpoints |
| `F04-origin-range` | 16 | On-surface origins, inside-shell origins, and hits at/near $t_{min}$ and $t_{max}$ | Ray-range semantics |
| `F05-near-miss` | 16 | Perturb aimed rays across the local displaced normal by frozen small offsets, then classify by oracle | Tight reject/accept decisions |

For an aimed ray at surface point $S(u,v)$ with unit surface normal $n$ and tangent frame $(t_1,t_2)$,

$$
d=-\cos\theta\,n+\sin\theta(\cos\phi\,t_1+\sin\phi\,t_2),
\qquad O=S-Ld,
$$

with $L$ chosen from the conservative shell AABB diagonal. This construction guarantees the unperturbed ray reaches the declared surface point while still allowing earlier intersections, which the exhaustive oracle must resolve.

### 7.1 Separate structural/adversarial suite

The following 96 constructed rays are reported separately from ordinary fallback rates:

| Family | Count | Expected behavior |
|---|---:|---|
| Exact and near tangencies | 16 | Correct isolated/repeated-root handling |
| UV turning points inside the valid interval | 16 | Correct event partition; no monotonicity assumption across the event |
| Denominator root just outside/inside the height range | 16 | Outside cases proceed; inside or uncertified cases fall back |
| Multiple roots in one leaf or multiple hit leaves | 16 | Complete hit set and correct closest $t$ |
| Coincident proxy/ray/grid events | 16 | Closed endpoint ownership without duplicate-induced mismatch |
| Near-singular shell Jacobian | 16 | Conservative primitive fallback when regularity cannot be certified |

Every structural witness stores its construction parameters and final numeric ray, so it remains reproducible if the generator changes.

### 7.2 Incidence and conditioning strata

Results are stratified by:

- incidence: `[0°,30°)`, `[30°,60°)`, `[60°,75°)`, `[75°,85°)`, and `[85°,90°]`;
- shell: `S00`, `S01`, `S02`;
- map class: planar/coherent, smooth, sparse, structured discontinuous, and unstructured high-frequency;
- hit state: miss, one hit, and multiple hits; and
- conditioning: log-spaced bins of the certified lower bound on $|D(h)|$.

For hit rays, incidence is measured at the exact closest hit. Misses retain their generation stratum and are never assigned a fabricated hit normal.

## 8. Correctness protocol

### 8.1 Oracle independence

For every ray, enumerate every represented texture microtriangle, form the analytic cubic, isolate all valid roots, deduplicate boundary-equivalent roots, and choose the smallest valid world-ray $t$. This path shares the surface equation but not hierarchy construction, node rejection, segmentation, or traversal.

The independent 80-digit path cross-checks:

- two ordinary rays per frozen decision case;
- every structural ray at a tractable grid resolution; and
- every minimized failure before it is considered resolved.

### 8.2 Required invariants

1. Every hierarchy node encloses all descendant analytic height triangles.
2. Every oracle-hit ownership class is represented by at least one admitted candidate leaf.
3. Each method's hit/miss result and closest $t$ agree with the oracle after boundary-root deduplication.
4. Dense diagnostic samples remain inside every certified componentwise and directional bound.
5. Every deliberately singular or uncertifiable case takes the declared fallback.
6. No segment, stack, node, or step cap may convert resource exhaustion into a miss.

Dense sampling is only a diagnostic. It does not replace the Bernstein/interval proof.

### 8.3 Numeric comparison

- Hit/miss classification must agree exactly.
- Roots are compared after deduplication with tolerance `max(1e-10, 64 ulp(t_oracle))` in the CPU reference.
- Position error is recorded in units of the shell AABB diagonal.
- A boundary hit may have multiple valid leaf owners; candidate correctness requires at least one equivalent owner, while the hit root itself must remain unique after deduplication.

## 9. Persisted data and metric schema

Step 1 results must be generated from persisted records, not copied from notebook output.

### 9.1 Output layout

```text
experiments/step1_ray_reference/<run_id>/
  run_manifest.json
  maps.jsonl.gz
  cases.jsonl.gz
  nodes.jsonl.gz
  rays.jsonl.gz
  summaries.json
  figures/
  failures/<failure_id>.json
  failures/<failure_id>.png
```

`run_manifest.json` records the Git commit if available, dirty-worktree flag, Python/NumPy versions, platform, command, wall-clock start/end, configuration hash, file hashes, and all seeds.

### 9.2 Map record

Required fields:

```text
grid_id, map_id, source_path, source_hash, source_mode, source_size,
crop_mode, crop_rectangle, resize_filter, normalization_percentiles,
cells, seed, min_h, max_h, mean_h, std_h,
gradient_rms, gradient_p95, laplacian_rms, plane_residual_rms,
plane_r2, high_frequency_fraction, high_gradient_cell_fraction
```

### 9.3 Case record

```text
case_id, grid_id, map_id, shell_id, cells, eta, thickness_scale,
ray_suite_id, ray_seed, shell_regular, min_abs_det_j,
node_count, leaf_count, expected_fallback_class
```

### 9.4 Per-node record

```text
case_id, node_id, level, ix, iy, size, proxy_overlap_fraction,
h_min, h_max, minmax_width,
h0, g_u, g_v, remainder_r, residual_width,
exact_residual_min, exact_residual_max, residual_slack_ratio,
all_node, visited_by_any_ray, visit_count_by_variant
```

Definitions:

- `minmax_width = h_max - h_min`;
- `residual_width = 2r` after subtracting the stored plane; and
- `residual_slack_ratio = 2r / (exact_residual_max-exact_residual_min)` with constant-zero cases reported separately rather than divided by zero.

All-node and ray-visited distributions are both reported to expose selection bias.

### 9.5 Per-ray record

Common fields:

```text
case_id, ray_id, family, subfamily, seed,
origin_xyz, direction_xyz, t_min, t_max,
generated_incidence, measured_hit_incidence,
oracle_hit, oracle_hit_count, oracle_t, oracle_h,
oracle_owner_class, min_abs_D, valid_interval_count,
fallback_expected, fallback_reason
```

For each method variant:

```text
active, hit, hit_t, hit_t_error, hit_position_error,
candidate_omissions, node_tests, internal_nodes, candidate_leaves,
exact_leaf_tests, global_segments, local_linearizations,
uv_splits, bow_splits, directional_bound_evaluations,
eps_u_texels_max, eps_v_texels_max,
componentwise_projected_error_max, directional_projected_error_max,
fallback_taken, fallback_reason
```

Do not overload “segment.” `global_segments` is a completed valid-interval partition; `local_linearizations` counts node-local endpoint-chord constructions during lazy traversal.

## 10. Aggregation rules

- Report per ray with `p50`, `p95`, `p99`, and `max`. Here `p95`, for example, is the value below which 95% of rays in the declared stratum fall.
- Always show the sample count and fallback count beside quantiles.
- Ordinary and deliberately singular/adversarial rays are never pooled for fallback rates.
- Report misses and hits both jointly and separately.
- Use paired comparisons: the same ray under two variants.
- For aggregate method ratios use ratio of totals, such as $\sum L_{FO}/\sum L_{MM}$, not the mean of unstable per-ray ratios.
- For per-ray paired plots use differences when the control count can be zero.
- Bootstrap confidence intervals resample complete map/shell cases, not individual rays, to avoid treating correlated rays as independent assets.
- Development cases are visibly marked and never substituted for the held-out report.
- CPU wall time may be recorded for experiment budgeting but is not a performance result.

## 11. Primary metrics

### 11.1 Hard correctness metrics

- candidate-owner omissions;
- hit/miss mismatches;
- closest-$t$ and position error;
- high-precision root-set mismatches;
- hierarchy-enclosure violations;
- sampled-to-certified tube and directional-bound ratios; and
- expected versus unexpected fallback counts.

### 11.2 H1 hierarchy metrics

- `residual_width / minmax_width` by level;
- residual slack relative to the exact residual range;
- the same ratios for all nodes and for actually visited nodes;
- `G-FO / G-MM` total node tests; and
- `G-FO / G-MM` total candidate leaves and exact cubic leaf tests.

### 11.3 H3 segmentation metrics

- valid curve intervals per ray;
- global segments per active ray;
- maximum certified tube radius in texels;
- local linearizations and split counts;
- unexpected fallback rate versus $|D|$ conditioning;
- candidate leaves per segment; and
- the $\eta$ Pareto relation between segments, nodes, and exact leaves.

### 11.4 Directional-certificate metrics

- $E_{cw}/E_{dir}$ where both are nonzero;
- fraction of node-local intervals for which the direct bound changes split/descend/reject outcome;
- `L-DIR / L-CW` local linearizations, bow splits, node tests, and leaves; and
- directional-bound evaluation count, which later becomes the arithmetic-cost input to the GPU experiment.

### 11.5 Cost-model sensitivity, not timing

Use the operation counts in a deliberately broad proxy model,

$$
W=b_nN_{node}+c_\ell N_{leaf}+c_sN_{linearize},
$$

with node-byte multiplier $b_n\in\{1,1.5,2\}$ and leaf-to-node cost $c_\ell\in\{4,8,16,32\}$. Vary $c_s$ over a declared range. This plot tests whether any plausible cost regime survives; it is not presented as measured speed.

## 12. Required plots

Every plot is regenerated from persisted records with a visible run/configuration ID.

1. **Dataset manifest:** thumbnails plus gradient, plane-residual, and high-frequency descriptors for every declared map.
2. **Correctness dashboard:** omissions, mismatches, high-precision differences, tube ratios, and fallback classification.
3. **Per-level tightness:** distributions of residual/minmax width for all nodes and visited nodes, split by map class.
4. **Paired traversal work:** `G-MM` versus `G-FO` node and exact-leaf counts with the equality line and paired ECDFs.
5. **Ray-family distributions:** p50/p95/p99/max nodes, leaves, segments, and fallbacks for aimed, free, grazing, boundary, and origin/range rays.
6. **Regime heatmap:** first-order work ratio over map class × shell variation × incidence bin.
7. **Certification Pareto:** $\eta$ versus segment count, tube width, nodes, and candidate leaves.
8. **Conditioning plot:** segments and fallbacks versus certified minimum $|D|$, separating injected singular cases.
9. **Directional mechanism:** $E_{cw}/E_{dir}$ distribution and paired `L-CW`/`L-DIR` linearizations, splits, nodes, and leaves.
10. **Resolution/thickness sensitivity:** work ratios for `cells={16,32,64}` and $s_h={0.5,1,1.5}$.
11. **Cost-model sensitivity:** regions in $(b_n,c_\ell,c_s)$ where the first-order variant has lower proxy work.
12. **Automatic failure view:** world ray, shell-space curve/events, certified tubes, visited hierarchy nodes, oracle owners, and method candidates for every minimized failure.

Paper-candidate plots are 3, 4, 6, 7, and 9. The rest are correctness/supplemental diagnostics.

## 13. Explicit gates

### G0 — reproducibility

**Pass only if:** a repeated run with the same manifest produces identical case IDs, ray values, integer counters, hit classifications, and failure IDs. Floating summaries must agree within the declared numeric tolerance.

**Failure action:** fix persistence or nondeterminism before interpreting results.

### G1 — correctness and certification

**Pass only if all are true:**

- zero candidate-owner omissions for every non-fallback method;
- zero hit/miss or closest-hit mismatches against the exhaustive oracle;
- zero high-precision root-set mismatches;
- zero hierarchy-enclosure violations;
- maximum sampled/certified ratio no greater than $1+2\times10^{-8}$;
- every deliberately singular/uncertifiable case takes fallback; and
- unexpected fallback rate is at most 0.1% for ordinary non-grazing rays and at most 1% when ordinary grazing rays are included.

**Failure action:** Step 1 is blocked. No performance-like comparison is valid until the witness is minimized and the certificate or ownership rule is repaired. The threshold is not relaxed to hide a failure.

### G2 — material first-order hierarchy benefit

Define the coherent cohort as `P01`, `P02`, `P03`, `P04`, `R00`, `R01`, `R02`, `R03`, and `R05`. High-frequency cases remain reported but are not expected to improve.

**Go if all are true:**

1. At levels 2–4 measured upward from the leaves of the 32-cell hierarchy, at least 70% of coherent map/shell cases have median `residual_width/minmax_width ≤ 0.75` over visited nodes. Resolution sweeps use the corresponding normalized-depth band.
2. Under fixed global segmentation, at least one coherent procedural map and at least two distinct real map IDs achieve `G-FO/G-MM exact_leaf_tests ≤ 0.85` while `G-FO/G-MM node_tests ≤ 1.05`.
3. Pooled over the coherent frozen decision cohort, exact leaf tests improve by at least 10% and node tests do not regress by more than 5%.
4. No ordinary map/shell case has more than 20% node-test inflation without being identified as a declared high-frequency/adversarial regime.

**Marginal:** pooled exact-leaf improvement is 5–10%, or the benefit occurs only on procedural data. Permit one predeclared revision of the node fold/layout objective, then rerun the complete frozen corpus.

**No-go:** pooled improvement is below 5%, no real crop improves by at least 10%, or useful-level residual bounds are routinely as wide as min/max. Do not port the ray slab as a claimed performance application; the shared hierarchy may still proceed to the area-sampling investigation.

### G3 — affordable curved-ray certification

At default $\eta=0.25$:

**Go if all are true:**

- for ordinary non-grazing `S00/S01` rays, global segments per active ray have p95 ≤ 4, p99 ≤ 8, and max ≤ 32;
- including `S02` and grazing rays, p99 ≤ 16 and max ≤ 64;
- the unexpected fallback limits from G1 hold;
- the resolution/$\eta$ sweep contains at least one global setting with at least 10% pooled exact-leaf reduction and segment p95 ≤ 8; and
- the benefit is not produced by candidate omissions or weakened ownership.

**No-go/revise:** certification consistently removes the G2 advantage, the tails grow without control, or ordinary fallback is common. Investigate a conservative hybrid traversal or tighter tube bound; do not weaken correctness.

### G4 — optional directional lazy-segmentation benefit

This gate controls whether direct directional lazy segmentation is promoted, not whether the basic first-order slab survives.

**Go if all are true on `S01/S02`:**

- at least three coherent map/shell cases, including one real map, have `L-DIR/L-CW local_linearizations ≤ 0.85`;
- the same cases have at least 25% fewer bow splits or at least 5% fewer node tests;
- pooled candidate leaves do not increase by more than 2%;
- no ordinary case has more than 10% local-linearization regression; and
- G1 remains satisfied.

**No-go for this feature:** retain global certified segmentation or lazy componentwise ownership, and remove the claim that the first-order slope materially reduces ray knots. Do not select only the explanatory Figure 2 ray as evidence.

## 14. Final decision matrix

| Outcome | Next action |
|---|---|
| G1 fails | Correct the reference; no CUDA authorization |
| G1 passes, G2 fails | Stop the ray-performance branch of the hierarchy; continue evaluating area sampling separately |
| G1–G2 pass, G3 fails | Revise the certified curve adapter or examine a conservative hybrid; do not weaken the claim |
| G1–G3 pass | Authorize **only** the fixed-segmentation GPU `G-MM` versus `G-FO` slab ablation |
| G1–G4 pass | Schedule a later GPU lazy-directional experiment after the fixed GPU slab shows a real bandwidth-aware win |
| G1–G3 pass but G4 fails | Keep first-order surface rejection; drop/defer directional lazy segmentation as a contribution |

Passing Step 1 does not establish superiority to TFDM, Ogaki, RMIP, PDM, triangles, or DMM. It establishes that a GPU test is rational.

## 15. Execution order after this specification is approved

Each item receives a short implementation plan before its code is changed.

1. **Persistence plan:** freeze JSONL fields, deterministic IDs, hashing, and output directory behavior.
2. **Dataset-loader plan:** implement procedural variants and file crops; generate the dataset-manifest figure only.
3. **Ray-generator plan:** implement each family independently and visualize a sample sheet before running traversal.
4. **Metric-instrumentation plan:** add counters without changing predicates; regression-test old summaries.
5. **Smoke execution:** run the development set and fix only correctness/persistence failures.
6. **Decision execution:** run the frozen corpus once thresholds and seeds are locked.
7. **Held-out and fuzz execution:** no tuning after inspecting held-out results.
8. **Report generation:** create plots and a machine-readable gate table.
9. **Decision note:** record go, marginal, or no-go with the exact manifest ID.

**Progress — 2026-08-23:** execution item 1 is through persistence Phase P2. The `step1-v1` schema and aggregate adapter pass 26 tests. Persisted run `p2-40a9c6e5e680` covers 14 current procedural map/shell cases and 896 rays with zero recorded correctness failures. Its pooled `G-FO/G-MM` node and accepted leaf-interval ratios are 0.9709 and 0.8610, respectively, but these are aggregate-only plumbing/mechanism results—not paper evidence. The detailed P3 observer, counting semantics, per-ray fields, static-node definitions, equivalence tests, and hard gate are frozen in [[Plan — Step 1 persistence and instrumentation#16. P3 detailed observer and record plan — 2026-08-23]] before traversal instrumentation begins.

**P3 completion — 2026-08-23:** all 32 P1–P3 tests pass, as does the unchanged 384-ray/6,144-polynomial legacy gate. Detailed run `p3-f29f4158c87f` persisted 896 rays and all four methods with zero correctness failures and exact global-method aggregate agreement. Global certified segmentation has p95 one and max two segments in this legacy suite; fixed `G-FO` reduces pooled exact leaf-interval tests by 13.9%. Direct directional lazy certification saves only four local linearizations and two node tests while adding 5,300 directional-bound evaluations, so its broad benefit is not supported by this checkpoint. P4 lifecycle must precede the frozen corpus expansion.

**P4/P5 development completion — 2026-08-23:** 39 P1–P4 tests pass. Manifest-backed attempt `step1-512846b663fc/a0001` checkpointed all 14 development cases and finalized 896 ray records with zero failures. Independent readback verifies every hash/count/reference/sequence, and its scientific records equal P3 except for `run_id`. Execution item 1 is complete; execution item 2, the dataset loader and manifest figure, is planned next before code.

**D1 dataset completion — 2026-08-23:** execution item 2 passes. The independent loader freezes 15 decision maps and seven audited source files, including channel/bit-depth/crop/normalization rules and canonical grid hashes. All 50 P1–D1 tests pass. Manifest `dataset-2a294309ffff` reopens with 15 unique grids and matching configuration/stream hashes; the visually audited v2 overview is under `figures/step1_dataset/`. [[Plan — Step 1 controlled ray generator]] now freezes execution item 3 before implementation.

**D2 controlled-ray completion — 2026-08-23:** execution item 3 passes. All 61 P1–D2 tests pass. Suite `rays-935162cc636e` freezes 45 decision cases and 11,520 unique ordinary rays across the six declared families, with contiguous sequence indices and independently verified case/ray hashes and references. The v3 sample sheet was visually audited and contains no query result. Oracle integration must receive its own plan before any of these inputs are traversed.

**O1 oracle completion — 2026-08-23:** [[Plan — Step 1 oracle integration]] passes on all 45 cases. Float run `oracle-dd072597b14a/a0001` persists 11,520 annotations, 25,387 grouped physical roots, zero ordinary constructed-root failures, and 360 explicitly classified one-ULP range ambiguities. Linked 80-digit run `oracle-hp-791061ae1b7d/a0001` checks 100,980 cubics on 90 frozen rays with 255/255 matching root groups and zero mismatches. All 70 P1–O1 tests pass. This establishes independent truth for the decision corpus but still supplies no hierarchy-benefit result.

## 16. Deliverables

Step 1 is complete only when the repository contains:

- the frozen dataset and ray manifests with hashes and seeds;
- persisted per-node and per-ray records;
- a zero-error correctness report or minimized unresolved witnesses;
- all required plots with run IDs;
- a one-page gate table for G0–G4;
- an explicit decision on fixed first-order GPU authorization; and
- a separate decision on whether directional lazy segmentation remains in scope.

## 17. Decisions frozen by this plan

1. Step 1 uses recursive quadtree traversal, not DDA.
2. It evaluates operation counts and bound tightness, not GPU performance.
3. `G-MM` and `G-FO` share identical global certified segments to isolate the surface hierarchy.
4. `L-CW` and `L-DIR` share identical UV ownership rules to isolate directional height certification.
5. Real map crops and held-out seeds are frozen before results.
6. Correctness is a hard zero-failure gate.
7. A directional-lazy failure does not erase a fixed-segmentation first-order win.
8. External baselines begin only after this reference gate justifies the relevant GPU implementation.

---

Related: [[Plan — Ray application method, prototype, and baselines]] · [[Project — Conservative first-order queries on displacement maps]]
