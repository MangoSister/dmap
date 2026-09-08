---
title: Plan — Practical first-order DDA ray traversal
tags: [plan, displacement-mapping, ray-tracing, first-order, DDA, GPU, datasets]
status: active
created: 2026-08-23
updated: 2026-08-24
parent: "[[Project — Conservative first-order queries on displacement maps]]"
implementation-status: p-d2-0c-negative-ray-branch-stopped
---

# Plan — Practical first-order DDA ray traversal

> [!abstract] Decision
> The paper-facing ray result is **complete traversal performance at equal correctness**, not residual-width percentages. Static bound tightness remains a mechanism diagnostic. The next ray method couples the first-order representation directly to ordered hierarchical DDA through a plane-compensated residual coordinate. No implementation begins until the dataset, mathematical predicate, data layout, ablations, metrics, and go/no-go thresholds below are reviewed.

> [!warning] Superseded performance branch — 2026-08-24
> P-D2.0c and the completed representation oracle reject the first-order residual payload as a viable ray accelerator on this topology. Do not execute the first-order GPU stages preserved below. The active, separately gated scalar comparison is [[Plan — Certified ray architecture comparison]]; it tests certified shell-ray min/max DDA against Ogaki-style nonlinear traversal and TFDM without reopening the first-order claim.

## 1. What the completed reference run did and did not test

The Step 1 decision corpus contains eight analytic/procedural maps and seven file-backed maps. Of the latter, `R00-test-macro` is also synthetic; only six are practical Poly Haven CC0 materials: Rock 05, Cobblestone Floor 08, Rocky Terrain 02, Brick Wall 001, Leather Red 02, and Coast Sand Rocks 02.

Every Step 1 query used a `33×33` represented grid. Each practical source began at `1024²`, but the fixed center `512²` crop was BOX-resized to `33×33` and percentile-normalized. This was intentional for exhaustive cubic enumeration over 11,520 CPU rays. It validates certificates and isolates node predicates; it does **not** reproduce production texture frequency, cache behavior, bit depth, multi-triangle scenes, or GPU traversal cost.

The current T2 method uses recursive quadtree traversal with globally certified curve segments. It is not DDA and records operation counts, not time. Consequently, the observed coherent ratios—`0.897` nodes and `0.691` exact tests—show pruning potential but do not establish faster ray traversal.

## 2. Correct paper question

The ray application must answer:

> At identical represented geometry and zero candidate omissions, does a first-order residual-space hierarchy reduce **end-to-end GPU intersection time** relative to the same hierarchical DDA with scalar min/max, and where does it lie on the time–memory–update Pareto frontier against TFDM, Ogaki, RMIP, PDM, explicit triangles, and DMM?

The primary outputs are nanoseconds per ray, rays per second, intersection/path-tracing milliseconds, and bytes. Bound ratios, candidates, and DDA steps explain the result but never substitute for it.

## 3. Proposed method: plane-compensated hierarchical DDA

For one hierarchy node $N$ with UV domain $\Omega_N$, store a conservative first-order model

$$
H(u,v)=p_N(u,v)+r_N(u,v),\qquad
p_N(u,v)=c_N+g_{u,N}(u-u_N)+g_{v,N}(v-v_N),
$$

with $r_N(u,v)\in[r_N^-,r_N^+]$ over the complete represented surface inside $\Omega_N$.

A certified shell-ray segment has affine chord

$$
\bar q(s)=(\bar u(s),\bar v(s),\bar h(s)),\qquad s\in[s_a,s_b],
$$

and componentwise tube error

$$
|u-\bar u|\le\epsilon_u,\quad
|v-\bar v|\le\epsilon_v,\quad
|h-\bar h|\le\epsilon_h.
$$

Define the node-local residual ray

$$
\bar\rho_N(s)=\bar h(s)-p_N(\bar u(s),\bar v(s)).
$$

Because the chord and node plane are affine, $\bar\rho_N(s)$ is affine. The true residual ray is enclosed by

$$
\rho_N(s)\in
\bar\rho_N(s)\oplus[-E_N,E_N],\qquad
E_N=\epsilon_h+|g_{u,N}|\epsilon_u+|g_{v,N}|\epsilon_v.
$$

The node is impossible if the range of this widened affine function over the UV-overlap interval is disjoint from $[r_N^-,r_N^+]$. More strongly, solve the one-dimensional affine inequalities to obtain the subinterval

$$
I_N=\{s:\bar\rho_N(s)\in[r_N^- - E_N,r_N^+ + E_N]\}.
$$

Descendants receive only $I_N$, not the full cell-crossing interval. This **slab clipping**, rather than a scalar claim that $r_N^+-r_N^-$ is always smaller than min/max width, is the proposed first-order traversal mechanism.

## 4. DDA traversal contract

For every certified segment:

1. enter its UV footprint at an adaptive mip level;
2. enumerate hierarchy cells in increasing ray parameter using 2D DDA;
3. conservatively supercover the UV tube, not only its zero-width chord;
4. intersect the tube with the proxy triangle and obtain the cell parameter interval;
5. apply min/max clipping for `DDA-MM` or residual-space slab clipping for `DDA-FO`;
6. reject, advance, or descend with the clipped interval; and
7. at admitted leaves, solve the exact analytic microtriangle equation and retain closest-hit/any-hit ordering.

Watertight corner ties advance all tied DDA axes with closed ownership. No fixed step, stack, or segment cap may become a miss. Resource exhaustion invokes a conservative exact/nonlinear fallback and is measured.

The existing Mode 1 DDA code is reusable only as an engineering scaffold: per-segment setup, adaptive start level, texture-cached min/max access, child-state updates, OptiX integration, and counters. Its zero-width chord, heuristic `hBow`, fixed segment/stack limits, and planar world-microtriangle leaves are not the correctness contract of this method.

## 5. Data-layout gate

First-order traversal can lose despite fewer candidates if every node doubles memory traffic. Test these layouts before choosing one:

1. `FO32`: full float plane/remainder data, correctness/performance reference;
2. `FO16-outward`: outward-rounded half/packed plane and remainder values;
3. `tile-predictor`: one plane per fixed tile or branch plus a two-channel residual min/max pyramid, amortizing the predictor fetch; and
4. `parent-delta`: parent plane retained in registers with conservatively quantized child corrections.

Every packed variant must decode to a superset of `FO32`. Report bytes per source texel, fetches, L2/DRAM traffic, registers, occupancy, and widening. If no layout makes first-order traversal faster than `DDA-MM`, the ray acceleration claim fails even if leaf counts improve.

## 6. Practical displacement corpus

Keep the current `33×33` corpus as **Reference-C**, never as the performance corpus.

Freeze a separate **Performance-P** corpus before timing:

- existing Poly Haven Rock 05 at native `1K/2K/4K` for the cache-capacity sweep;
- existing Cobblestone Floor 08, Rocky Terrain 02, Brick Wall 001, Leather Red 02, and Coast Sand Rocks 02 at `2K` or `4K`;
- at least four additional CC0 maps spanning layered concrete/cracks, compact sand, dirt/footprints, and rocky trail/cliff structure;
- one smooth low-frequency control and two declared high-frequency/adversarial controls.

Candidate public assets include Poly Haven Concrete Layers (`4K–16K`), Sand 01 (`1K–8K`), Dirt (`1K–8K`), and Rocky Trail 02 (`up to 16K`). Preserve 16-bit or floating displacement; do not route final maps through an 8-bit-only loader. Record source URL, license, content hash, native bit depth, physical coverage when supplied, crop/tile policy, and displacement scale.

Performance uses complete `1K/2K/4K` maps without BOX reduction. Correctness uses multiple frozen native `64×64` windows from every practical source, selected before traversal results, so exhaustive or dense-oracle checks remain tractable while retaining native texture frequency.

## 7. Practical scene/ray corpus

One isolated proxy triangle is insufficient for the final evaluation. Use:

- one large tiled plane/terrain;
- curved triangle-proxy shells at low, medium, and high normal divergence;
- a UV-mapped closed object with seams and thousands of coarse triangles;
- a captured/model asset carrying both base geometry and displacement when licensing permits; and
- repeated/instanced objects to expose memory and build amortization.

Trace primary closest-hit, shadow any-hit, and secondary/path-tracing rays. Freeze front-facing, oblique, silhouette/grazing, near-miss, and incoherent ray batches. Sweep displacement amplitude relative to base-edge length and local curvature radius rather than normalizing every material to the same arbitrary height range.

## 8. Required ablations and baselines

Internal isolation:

1. `DDA-MM`: certified segments + tube-supercover hierarchical DDA + scalar min/max;
2. `DDA-FO-test`: identical traversal, boolean first-order slab test;
3. `DDA-FO-clip`: identical traversal plus residual-space interval clipping;
4. `recursive-MM/FO`: reference traversal to verify DDA candidate equivalence;
5. segmentation time excluded and included; and
6. all layout variants from Section 5.

External paper baselines at matched represented geometry/quality:

- upstream Ogaki-style nonlinear traversal;
- TFDM;
- RMIP;
- PDM with a step-size/quality sweep;
- dense displaced triangles/BLAS;
- DMM where supported; and
- exhaustive/dense geometry as correctness oracle only.

TFDM already uses a displacement min/max mipmap and affine arithmetic to generate conservative world-space boxes. RMIP already performs ray-dependent inversion and anisotropic oblong bounding. Therefore neither hierarchical traversal nor conservative boxes alone are novel. The candidate distinction is stored first-order height–UV correlation consumed directly as an ordered residual-space clipping operation, plus reuse of the same first-order hierarchy by the area-sampling application.

## 9. Metrics and final calls

Primary:

- full custom-primitive intersection time and ns/ray;
- end-to-end frame/path time for fixed ray counts;
- throughput for closest-hit and any-hit;
- resident/build-scratch bytes and preprocessing/update time; and
- correctness/quality versus the independent oracle.

Mechanism diagnostics:

- certified segments, DDA cells, node fetches, rejected cells, descended cells, exact leaves, and fallback rates per ray with p50/p95/p99/max;
- time split among shell inversion/segmentation, DDA, hierarchy fetch/predicate, and exact leaves;
- register count, local-memory spills, occupancy, branch efficiency, L2 hit rate, and DRAM bytes; and
- slab-clipped interval reduction relative to min/max, separated from static residual width.

### Internal first-order acceleration gate

At zero candidate omissions and identical leaf equations, `DDA-FO-clip` must be at least `10%` faster in pooled intersection time than `DDA-MM` on the coherent practical cohort, win on a majority of practical assets, and avoid an unexplained `>15%` regression on any non-adversarial asset. Leaf/node percentages alone do not pass.

### Paper-facing ray gate

The ray application survives as a headline contribution only if it either:

1. provides a competitive or better time–memory Pareto point against the strongest non-tessellated baselines; or
2. remains within `15%` of the best applicable non-tessellated traversal while the single shared ray+sampling hierarchy provides a material combined memory/build/update advantage over two specialized structures.

No universal "faster than every baseline" requirement or claim is used. Static DMM/pre-tessellation may remain faster in its favorable regime.

## 10. Ordered execution; stop after every gate

1. **P-D0 dataset manifest — complete:** audit/download practical sources, bit depths, full-resolution decode, native correctness windows, and visual manifest.
2. **P-D1 mathematical DDA reference — complete:** implement a CPU tube-supercover DDA and prove equivalent candidate/root sets to recursive and exhaustive traversal. No timing claim.
3. **P-D2 residual clipping ablation — complete, negative:** compare scalar min/max, componentwise first order, directional first order, and hybrid clipping over the frozen practical corpus.
4. **P-D2.1 full-resolution replay — stopped:** not authorized because neither P-D2.0c candidate passed.
5. **P-D3 layout microbenchmark — stopped for this formulation:** retain the design only for a new hypothesis that first passes a CPU usefulness gate.
6. **P-D4/P-D5 GPU and external ray baselines — stopped for this formulation.**
7. **Shared-system continuation:** complete the area/metric oracle and sampler gates, then decide whether the ray correctness method plus area application form a viable EG paper.

Before each item, write its exact input manifest, metrics, and pass/fail rule. Do not start P-D1 until P-D0 is reviewed.

### Execution record

- `P-D0` passed on 2026-08-23 as run `p-d0-71627ba7b07f`: 12 native images, 10 material identities, 48 frozen windows, six >8-bit inputs, and all source/decode/reproducibility tests passing. See [[Plan — P-D0 practical displacement dataset]].
- `P-D1.0/P-D1.1` passed the triangle-shell inverse and certified chord-bound gates.
- `P-D1.2` passed correctness on 15,552 traces but rejected the proposed first-order segmentation advantage: node clipping already produces sufficiently short local chords, and directional bounds changed only two isolated one-split cases at the central policy. Segmentation remains shared certified infrastructure, not a first-order contribution.
- `P-D1.3` passed the independent closed UV-tube coverage gate on 6,048 practical records and nine analytic ownership cases, with zero candidate disagreement and `0` ULP endpoint disagreement. At `64×64`, nonzero tube radius adds p50 `0`, p95 `3`, and p99 about `8.37` cells over the centerline, confirming that zero-width traversal is insufficient in the tail even though most rays are nearly linear.
- `P-D1.3a` replaced the initial float interval artifact with exact-rational interval propagation after P-D1.4 exposed shared cancellation. Candidate sets were unchanged, but the old float endpoints had inward errors up to about `4.07e-14`; later stages consume corrected run `p-d1-3-dedfc83b59c8`.
- `P-D1.4` passed event-supercover equivalence on 6,048 practical records, 22 special cases, and 256 held-out fuzz cases: zero candidate disagreement, maximum `1` ULP endpoint difference, and all edge/corner/reversal gates passing. This establishes DDA coverage mathematics, not an optimized incremental GPU layout.
- `P-D1.5` passed the represented-leaf/root gate on one complete practical window (18 shell/ray cases). Event and recursive candidates match exhaustive and 80-digit Decimal root records, all constructed hits are retained, and maximum world residual is `7.92e-16`. The represented leaf is the shell-space cubic, not a planar world microtriangle.
- Initial P-D1.6 run `p-d1-6-1a2502ae0003` failed only on proxy-edge roots and exposed a residual-terminated shell-boundary partition bug upstream. The bug was corrected at its P-D1.2 producer, and every dependent content-addressed artifact was regenerated rather than patching the leaf tolerance.
- Final P-D1.6 run `p-d1-6-e254f252e010` passes all 864 practical cases: event/recursive/exhaustive/Decimal roots agree, all constructed targets are retained, and no practical fallback occurs. P-D1 is complete CPU correctness evidence.
- P-D2.0b passes all 108 anchor traversals in `p-d2-0b-ec2b12fcdf82` with exact roots/owners, no fallback, contained intervals, and deterministic records. Relative to `MM-clip`, `FO-CW-clip` uses `0.907×` node predicates and `0.787×` leaf cubics; `HYB-clip` uses `0.884×` nodes and `0.745×` leaves. This was one-window mechanism evidence only; the following P-D2.0c record supersedes its optimistic work-count indication.
- P-D2.0c completes the full frozen corpus in `p-d2-0c-47849fef3202`: 48 exact-audited hierarchies, 864 cases, 5,184 traversals, and zero root, owner, target, interval, hierarchy, fallback, or determinism failures. On the primary coherent cohort, `FO-CW-clip` uses `0.9966×` nodes and `0.9674×` leaf cubics; `HYB-clip` uses `0.9906×` nodes and `0.9622×` leaves. Both miss the `0.85` leaf gate and have negative `delta_max` at leaf/node cost ratios 4 and 8. The current first-order ray-acceleration branch therefore stops before P-D2.1/P-D3/P-D4. The direct directional predicate is also dropped because its corrected pooled work is identical to componentwise clipping.

## 11. Predictive break-even model and CPU/GPU decision

The authoritative P-D2.0c coherent CPU ratios are

$$
r_N=N_{FO}/N_{MM}=0.9966,\qquad r_L=L_{FO}/L_{MM}=0.9674
$$

for `FO-CW-clip`, and `0.9906/0.9622` for the hybrid. The earlier `0.897/0.691` ratios came from the smaller Step 1 reference and are not representative of the frozen practical-corpus decision.

Let $\alpha$ be the fraction of variable min/max traversal cost spent on node/cell work rather than exact leaves, and let $\delta$ be the first-order predicate's extra cost per visited node relative to min/max. Ignoring common setup for the moment,

$$
R_{FO/MM}\approx \alpha r_N(1+\delta)+(1-\alpha)r_L.
$$

P-D2.0c evaluates this model directly over leaf/node cost ratios `2,4,8,16`. At the two required middle ratios, the maximum allowable first-order overhead is already negative: `-0.1900/-0.2831` for `FO-CW-clip` and `-0.1778/-0.2642` for the hybrid. In other words, neither candidate reaches a projected `10%` win even if its additional hierarchy fetch and plane arithmetic were free. Payload packing or GPU cache behavior cannot repair that missing operation-count margin under the current formulation.

The existing heuristic Mode 1 GPU results establish only a regime clue: on the RTX 5090 it is commonly `1.2–1.96×` faster end-to-end than the Ogaki-like Mode 0 on curved/sphere cases, while losing on flat and twisted large-prism cases and retaining nonzero disagreement. This makes parity with Ogaki/TFDM plausible in favorable regimes, not established. RMIP remains a substantially harder target; DMM/pre-tessellation is not expected to lose raw static tracing in its favorable regime.

### Decision on execution platform

The CPU decision stage is complete. Do not move this first-order formulation to GPU: the authoritative full-corpus operation gate fails before payload, cache, occupancy, divergence, and OptiX overhead are charged. Preserve the GPU replay design below as a conditional plan only for a future representation predicate that first clears a newly frozen CPU gate. By user direction, the next active ray stage is A0 of [[Plan — Certified ray architecture comparison]], which uses scalar min/max only; area sampling remains deferred until that first-tier ray decision.

### Cheap predictive GPU gate before full integration

Build a trace-replay microbenchmark before modifying the full intersection program:

1. persist representative DDA node/interval traces for full-resolution Rock `1K/4K`, Leather, one smooth map, and one adversarial map;
2. replay the identical access streams on the GPU using `MM-float2`, `FO32`, `FO16-outward`, and `tile-predictor` payloads;
3. measure ns/node, effective bytes, cache hit rate, registers, and occupancy;
4. combine measured per-node/per-leaf costs with the persisted MM/FO work counts and clipped-interval counts; and
5. reject any layout whose predicted full traversal does not clear the `10%` internal gate under both coherent and incoherent access.

This replay is a predictor, not a paper result: it omits OptiX control flow and cross-ray divergence. It cheaply answers whether the representation's memory cost has already erased its pruning advantage. Only a passing layout proceeds to the complete GPU `DDA-MM` versus `DDA-FO-clip` kernel.

### Baseline implementation order

1. internal `DDA-MM` versus `DDA-FO-clip`;
2. the existing Ogaki-like Mode 0 and TFDM code already in the workspace;
3. dense triangle BLAS and PDM quality sweep;
4. RMIP only after the internal gate passes, because a faithful implementation is substantial; and
5. DMM where the available API/hardware path is reproducible.

Do not implement RMIP or the ray-side first-order GPU payload for the stopped formulation. Execute only the scalar architecture plan's A0–A4 sequence. If its first-tier gate fails, return to the planned area/metric oracle and sampler rather than adding more ray baselines or weakening the gate.

---

Primary-source anchors: [TFDM paper](https://perso.telecom-paristech.fr/boubek/papers/TFDM/TFDM_lowres.pdf) · [RMIP project/paper](https://perso.telecom-paristech.fr/boubek/papers/RMIP/) · [PDM paper](https://diglib.eg.org/bitstream/handle/10.1111/cgf70235/cgf70235.pdf) · [Poly Haven](https://polyhaven.com/)

Related: [[Plan — Step 1 gate report]] · [[Plan — Ray application method, prototype, and baselines]] · [[Guide — Eurographics paper draft]]
