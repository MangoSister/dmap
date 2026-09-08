---
title: Plan — Mode 1 certified tube-supercover integration
status: correctness-and-cost-recovery-stage-complete-not-yet-competitive
created: 2026-08-29
parent: "[[Plan — Historical Mode 1 correctness repair]]"
---

# Plan — Mode 1 certified tube-supercover integration

## 1. Objective

Replace historical Mode 1's midpoint-only curve flattening and zero-width, half-open chord traversal with a conservative texture-space curve tube and closed-supercover hierarchy traversal. Preserve Mode 1's represented surface: the fixed-diagonal, piecewise-planar displaced texel microtriangles constructed in world space from the triangle proxy shell.

This stage is accepted for correctness before any optimization or performance comparison. It does not change the paper method to Catmull–Clark surfaces and does not reinterpret the standalone A2 timing result.

## 2. Frozen arithmetic and surface contract

For a triangle shell

$$F(a,b,h)=p_A+a(p_B-p_A)+b(p_C-p_A)+h\left(n_A+a(n_B-n_A)+b(n_C-n_A)\right),$$

construct the texture-space shell ray

$$u(h)=U(h)/D(h),\qquad v(h)=V(h)/D(h),$$

where $U,V,D$ are quadratic. Compute their centers in binary64 from the packed binary32 proxy, shell, UV, and ray values using the already-audited dominant-axis annihilators. Convert each coefficient to an outward binary32 interval before certificate evaluation. The certificate therefore encloses the device curve defined from the packed renderer inputs; dense rendered leaf tests remain the final same-surface oracle.

The supported height band is the complete per-triangle displacement min/max band converted to canonical height. Entry/exit height returned by the side-prism solver may be used only as a later proven optimization, not as the correctness domain.

## 3. Certified lazy segments

For interval $I=[h_0,h_1]$, evaluate $D(I)$ with an outward Bernstein range. If it contains zero, certification refuses.

For each texture coordinate $f=P/D$, bound its second derivative using

$$f''=\frac{((P''D-PD'')D-2(P'D-PD')D')}{D^3}.$$

If $|f''|\le M_f$, the secant error is bounded by

$$\epsilon_f\le M_f|h_1-h_0|^2/8.$$

Store one segment as

$$[h_0,h_1],\;(u_0,v_0),(u_1,v_1),\;(\epsilon_u,\epsilon_v).$$

The work list begins with the complete height band. A task is first clipped conservatively to the texture-triangle AABB expanded by its tube radii and then recertified on the clipped interval. It is accepted when

$$\max(\epsilon_u/\Delta u_{leaf},\epsilon_v/\Delta v_{leaf})\le\eta,$$

using the existing `thresholdScale` as $\eta$ for the first implementation. Otherwise split at the exact floating midpoint. A precision-floor interval is accepted with its certified radius. Segment/work capacity refusal invokes the whole-invocation fallback.

The midpoint distance and `sqrt(midDev/threshold)` rule are removed as correctness logic.

## 4. Closed tube-supercover hierarchy

For an accepted chord $c(s)$, $s\in[0,1]$, the certified UV enclosure is

$$c(s)\oplus[-\epsilon_u,\epsilon_u]\times[-\epsilon_v,\epsilon_v].$$

Traversal has two layers:

1. At an adaptive coarse start level, an incremental centerline DDA streams neighboring cells within `ceil(radius/cellWidth)+1`. Each neighbor is retained only if the chord intersects that cell expanded by the certified radii. Exact edge and corner contact is closed; both tied DDA axes advance without an epsilon jump.
2. Each unique retained start cell descends the min/max hierarchy. All four children are tested independently by closed tube-versus-child clipping. Height rejection intersects the child's min/max band with the inherited exact chord parameter interval, using $h(s)=h_0+s(h_1-h_0)$; no global segment-height range or heuristic `hBow` is used.

The traversal carries the conservative $s$ interval at every node. Leaf tests retain the closest world-ray hit globally and reconstruct attributes from the winning microtriangle.

Start-cell duplicates may initially be retested for correctness, but must be counted. Deduplication is an optimization and cannot change the admitted set.

## 5. Whole-invocation fallback

The following conditions discard every partial candidate/hit produced by the certified path and restart the same primitive invocation with an exhaustive scan of all leaf cells overlapping the proxy triangle:

- denominator or arithmetic certificate refusal;
- no distinct midpoint while the certificate is invalid;
- segment/work-list capacity exhaustion;
- hierarchy work-stack exhaustion;
- invalid/backward DDA event;
- a derived DDA iteration bound being exceeded; or
- non-finite packed input/coefficient data.

The exhaustive path samples the same displacement texture corners, constructs the same two world microtriangles, applies the same ray interval, and selects the same closest hit. It is intentionally slow and separately counted. No fixed cap may turn one of these conditions into a miss.

## 6. Implementation stages and gates

### M1-C1 — production certificate primitive

- Add a small renderer header for outward intervals, Bernstein ranges, rational second-derivative bounds, and tube-box clipping.
- Add a CPU mirror test using the production coefficient ordering.
- Differential-test analytic affine, strong-bow, near-singular, tiny-interval, reversed-height, and UV-transform cases against the existing A1 Decimal reference.

**Gate:** every sampled curve point lies inside the reported tube; affine radius is only arithmetic padding; every denominator-containing interval explicitly refuses.

### M1-C2 — closed-supercover reference extension

- Extend `scripts/mode1_dda_audit.py` from zero-radius chords to anisotropic tubes and inherited hierarchy intervals.
- Add horizontal/vertical grid-edge, corner, stationary-point, reverse, non-square texture, and bowed-curve cases.

**Gate:** exact candidate-set equality with exhaustive closed cell clipping and zero curve-cell omissions.

### M1-C3 — GPU integration

- Replace heuristic segment generation with certified lazy segments.
- Replace centerline-only hierarchy descent with streamed start-level tube supercover plus closed four-child descent.
- Add the whole-invocation exhaustive same-surface fallback and diagnostics.

**Gate:** no partial failure path; all M1-C1/C2 tests and the existing A1/A2 tests pass; Release build succeeds.

### M1-C4 — renderer evidence

- Re-run `quad`, `curved_surface`, `sphere`, and frozen `twisted_quad + disp_rock` against the dense same-surface oracle.
- Save beauty, hit mask, depth, normal, and XOR images.
- Report certificate refusals, fallbacks, segments, maximum tube radius in texels, DDA anchors, start-cell duplicates, hierarchy nodes, min/max fetches, and leaves per active invocation at p50/p95/p99/max.

**Gate:** the repaired cross remains absent; no unexplained omission relative to the dense oracle; all fallback rates and capacity high-water marks are explicit.

### M1-C5 — cost recovery

Only after M1-C4 passes, optimize start-level selection, duplicate suppression, node ordering, height interval clipping, and storage. Re-run identical correctness gates after every optimization. Historical pre-repair timing is never mixed with certified timing.

## 7. Stop conditions

Stop and document rather than weaken correctness if:

- the renderer's six-corner OptiX primitive bound is shown not to enclose the represented world microtriangles;
- the per-triangle displacement min/max buffer is not outward/conservative;
- a packed-input curve coefficient cannot be enclosed reliably by the production arithmetic path; or
- the fallback surface differs from the normal certified leaf surface.

These are representation-contract failures, not tolerances to tune away.

## 8. Executed correctness result — 2026-08-29

### 8.1 M1-C1/C2

The production CUDA certificate primitive is in `nrtdsm/gpu_kernels/mode1_certified_tube.h`. It uses outward binary32 interval arithmetic, Bernstein ranges for the quadratic denominator and cubic second-derivative numerator, endpoint inflation, and the secant-error theorem. Historical Mode 1 now constructs dominant-axis annihilator coefficients in binary64 from packed renderer inputs and converts them to outward coefficient intervals.

`scripts/mode1_dda_audit.py` now contains three deliberately separate enumerators:

1. exhaustive closed tube/cell clipping;
2. incremental anisotropic tube-supercover DDA; and
3. coarse-supercover plus independent four-child hierarchy descent.

They have exact candidate-set equality on 256 randomized tube cases and all frozen adversarial cases: stationary grid corner, horizontal/vertical closed grid edges, anisotropic radii, centerline outside the texture with overlapping tube, reverse traversal, non-square grids, and a bowed curve. The bowed curve loses cells under the old zero-width chord and loses none under the certified tube. Eleven combined Mode-1/A1/A2 tests pass.

### 8.2 M1-C3 integration

Historical midpoint subdivision, `sqrt(midDev/threshold)`, zero-width hierarchy descent, side-prism entry/exit height restriction, heuristic `hBow`, fixed DDA step termination, and partial capacity results are no longer correctness predicates in active `TRAVERAL_METHOD == 1`.

The active path now uses:

- the complete per-triangle displacement-height band;
- certified lazy texture-space chord tubes;
- closed start-level tube-supercover DDA;
- inherited tube and height parameter intervals during four-child min/max descent;
- global closest-hit world microtriangle tests; and
- an exhaustive same-surface whole-invocation fallback after any certificate/resource refusal.

The old repaired centerline implementation remains under inactive `TRAVERAL_METHOD == 2` for controlled comparison only.

### 8.3 Renderer correctness evidence

On frozen `twisted_quad + disp_rock` at 1920x1080:

- certified Mode 1 versus Mode 0 mask disagreement: `0.0012056%`;
- earlier full-height repaired Mode 1 versus Mode 0: `1.2694%`;
- common-hit mean world-position difference versus Mode 0: `2.58e-7`;
- p99 common-hit world-position difference: `1.31e-6`;
- certificate/cap/step/stack diagnostic pixels: all zero; and
- the historical cross remains absent.

The standard independent dense-oracle suite passes:

| case | method | mask disagreement | relative mean position | median normal error |
|---|---:|---:|---:|---:|
| quad | certified M1 | `0.00000` | `0.00000` | `0.000 deg` |
| curved surface | certified M1 | `0.00003` | `0.00007` | `0.826 deg` |
| sphere | certified M1 | `0.00003` | `0.00007` | `0.517 deg` |

The sphere frame reports 2,024 certificate-refusal pixels and one resource-capacity pixel among 199,863 active pixels. Every such invocation restarts with the exhaustive same-surface fallback; the output still passes the oracle gate. These are performance events, not partial misses.

### 8.4 Correctness-first cost

On frozen twisted-rock, the current implementation reports:

- `9.28` certified segments per active pixel;
- `198.99` candidate/hierarchy tests per active pixel;
- `9.76` exact leaf tests per active pixel;
- `1,125,108` intersection cycles per active pixel; and
- `1,595.6 ms` one-sample path pass.

The prior non-certified repaired path used `60.40` cells, `27.18` leaves, `3,839,531` isolated intersection cycles, but only `142.5 ms` for the path pass. The apparent contradiction is local-memory/occupancy pressure: OptiX reports roughly 5.9 KB direct stack per intersection thread, 128 registers, and 256 B spills after adding materialized segment/task/node arrays. Isolated per-active-pixel cycle counters do not capture the global occupancy loss.

Therefore the correctness architecture is validated, but the current renderer port is not a performance result. M1-C5 must stream lazy segments, reduce/materialize no per-thread arrays, suppress duplicate coarse roots, and resolve common denominator events without exhaustive fallback before any baseline timing is meaningful.

### 8.5 M1-C5 cost-recovery experiments

All experiments below use the same frozen `twisted_quad + disp_rock` command and compare geometry AOVs, not only beauty images.

#### Accepted: unique coarse-root ownership

Consecutive DDA anchors previously expanded to overlapping neighborhoods and independently descended the same coarse hierarchy root. A conservative segment-local root AABB is at most 8x8 under the frozen start-level rule, so a 64-bit ownership mask now admits each coarse root once. Closed chord-tube clipping remains the actual admission predicate. If the footprint exceeds the fixed mask contract, the whole invocation restarts exhaustively.

This change is bit-identical to the pre-optimization certified render and gives:

| frozen twisted-rock metric | before root ownership | after root ownership |
|---|---:|---:|
| segments / active pixel | `9.28` | `9.28` |
| cell or hierarchy tests / active pixel | `198.99` | `79.19` |
| exact leaf tests / active pixel | `9.76` | `1.98` |
| path-trace time | `1,584.2 ms` | `937.4 ms` |
| throughput | `1.31 Mray/s` | `2.21 Mray/s` |

This is a real 41% end-to-end recovery, but the accepted path is still about 6.6x slower than the old unsafe 142.5 ms historical Mode 1. The complete quad/curved/sphere dense-oracle suite passes after duplicate suppression with the same mask, position, and normal results as before.

#### Rejected: streamed segments

Lazy subdivision was changed experimentally to traverse an accepted certificate immediately instead of materializing 32 compact segment records. The streamed and materialized outputs were bit-identical. Direct local storage dropped only modestly, from about 5.25 KB of PTX local frame to 4.88 KB, and path time regressed from `1,584.2 ms` to `1,632.4 ms` before root ownership. Streaming is therefore not retained.

The experiment also exposed an instrumentation defect: placing traversal inside the segmentation timing scope raised the measured total to about 26.4M cycles/pixel. Earlier counters time certificate generation and leaf tests but omit most hierarchy/DDA work. End-to-end GPU timing is authoritative until the cycle scopes are repaired.

#### Rejected: non-inlined traversal frame

Moving the unique-root hierarchy walk to a non-inlined device helper produced zero mask changes and only roundoff-level position changes (mean `4.3e-8`), but timing was neutral (`940.1 ms` versus `937.4 ms`). Call and parameter overhead canceled the smaller entry frame, so the active implementation remains inlined.

#### Rejected: smaller fixed capacities

- Reducing the segment array from 32 to 16 produced bit-identical output and no new frozen-scene fallbacks, but reproducibly regressed path time to `2.47--2.48 s`; OptiX retained the same approximately 5.5 KB direct stack. This is a compiler/code-generation cliff.
- Reducing the hierarchy stack from 48 to 24 produced bit-identical output and no frozen-scene overflow, but did not reduce the PTX local frame and regressed timing to `981.4 ms`.

Both constants are restored. Further scalar capacity tuning is not justified.

#### Current performance conclusion

The certified curve-tube plus closed-supercover traversal is now candidate-set correct on the tested corpus, and duplicate-root ownership recovers a material fraction of its cost. It is not yet a competitive renderer. The next performance hypothesis must change the hierarchy execution model: a stackless/recompute traversal, a compact breadth/micro-stack layout, or a first-order residual hierarchy that rejects substantially more nodes. Baseline timing must remain blocked until one of those changes closes most of the remaining gap without weakening the oracle gates.

## 9. M1-C5b compact level-state traversal

### 9.1 Frozen method

Replace the 48-entry pending-node stack, whose records contain `(lod, ix, iy, lower, upper)`, with:

- one inherited lower/upper parameter interval per relative quadtree depth;
- one three-bit next-child cursor per depth, packed into a 64-bit word on GPU;
- the current `(lod, ix, iy)` only; and
- integer ascent to recover parent coordinates after a leaf or exhausted node.

On descent, the child obtains its interval by closed tube/cell clipping followed by min/max-height clipping, exactly as in the full-node stack. If a child is rejected, traversal remains at the parent and tries the next sibling. If all four children are exhausted, it ascends. Maximum supported depth is explicit; exceeding it triggers whole-invocation fallback.

This is a storage transformation, not a new culling rule. Node admission, leaf construction, closest-hit selection, and fallback surface stay unchanged.

### 9.2 CPU gate

`compact_level_state_tube_supercover()` in `scripts/mode1_dda_audit.py` is the independent CPU state-machine reference. The first test correctly caught a sibling-resume bug: a rejected child originally caused premature ascent. After correction, the compact traversal has exact leaf-key and inherited-interval equality with the full pending-node stack on 256 randomized anisotropic tubes and all square-grid adversarial cases.

The combined Mode-1/A1/A2 suite now passes 12 tests.

### 9.3 GPU acceptance gates

1. Replace only the per-root node descent; keep 32 materialized segments and unique-root DDA ownership fixed.
2. Require zero differences against the current unique-root geometry AOV on the frozen twisted scene, except documented floating-point position roundoff if node visit order changes.
3. Require the complete dense-oracle suite to pass with no new unexplained fallback class.
4. Accept as a performance result only if OptiX local frame or end-to-end path time drops materially. Otherwise restore the full-node stack and record the rejection.

### 9.4 GPU result

The compact level-state traversal is accepted:

- frozen twisted-rock geometry is bit-identical to the full-node unique-root traversal;
- segments, hierarchy tests, leaf tests, and all fallback diagnostics are identical;
- two frozen path timings are `917.3 ms` and `915.9 ms`, versus `937.4 ms` for the full-node unique-root traversal;
- PTX local frame drops from about `5.25 KB` to `4.99 KB`; and
- the complete quad/curved/sphere dense-oracle suite remains `ALL PASS`, with the same sphere certificate/resource fallback counts and no compact-depth overflow.

The retained certified implementation after M1-C5 is therefore unique-root DDA plus compact level-state hierarchy descent. Combined cost recovery is `1,584.2 ms` to approximately `916 ms` on the frozen scene, a reduction of about 42%. This remains roughly 6.4x slower than the old unsafe historical path and is not a competitive baseline result.

The performance conclusion for this stage is now firm: representation-independent traversal engineering recovered duplicate work and some storage cost, but cannot close the remaining gap by itself. The next paper-relevant experiment should test whether a first-order residual hierarchy materially lowers the remaining `79.19` node tests per active pixel at equal certified correctness. That experiment must compare first-order residual bounds against the same min/max mip hierarchy while holding the compact traversal, segment certificates, leaf surface, and fallback rules fixed.

## 10. Visual gallery and twisted-oracle correction

A fresh four-case visual gallery was rendered from the retained Release implementation at one primary sample per pixel. Every row uses one geometry and one practical 1K displacement map, with matched camera, lighting, texture, and displacement settings across internal M0, certified M1, and the dense pre-tessellated hardware-triangle oracle. The gallery and exact manifest are under `.tmp/mode1_certified/visual_gallery/`.

Three cases pass the existing dense-oracle gates:

| case | M1 mask disagreement | M1 relative mean position | M1 median normal error |
|---|---:|---:|---:|
| `quad + disp_brick` | `0.00000` | `0.00000` | `0.000 deg` |
| `curved_surface + disp_cobble` | `0.00004` | `0.00012` | `1.213 deg` |
| `sphere + disp_terrain` | `0.00005` | `0.00009` | `1.184 deg` |

The `twisted_quad + disp_rock` row does **not** pass the dense oracle: M1 has `0.01166` mask disagreement, `0.01917` relative mean position error, and `29.188 deg` median normal error. Internal M0 fails by nearly identical amounts (`0.01167`, `0.01917`, and `29.188 deg`). This makes an M1-only DDA omission unlikely, but does not diagnose the disagreement. It may be a represented-surface mismatch introduced by dense subdivision of the twisted proxy, or a shared M0/M1 surface/intersection error.

Consequently, the frozen twisted-rock frame remains valid for M1-versus-M0 regression and internal performance comparisons, but it is **not** dense-oracle correctness evidence. The earlier M1-C4 request to classify this disagreement remains open. The gallery marks the row as failed and provides a separate `comparison_oracle_gated_cases.png` containing only the three passing rows.

For readable visual inspection, a separate presentation gallery was rendered at `64 spp` using `data/env.exr` for HDR environment lighting. It is under `.tmp/mode1_certified/rendered_images_64spp_env/` and contains PNG files only: twelve full-resolution renders, four large per-case comparison plates, and one certified-M1 overview. Diagnostic AOVs, error images, statistics, and logs remain outside this presentation folder.

## 11. Mode-0 reference and proxy-domain seam repair

The dense tessellated implementation is no longer treated as a same-surface oracle. Its recursive world-mesh subdivision renormalizes midpoint normals before displacement and later recomputes smooth area-weighted shading normals. Mode 0 instead intersects planar displacement microtriangles in texture/canonical space under the original per-proxy shell map. These constructions differ when proxy normals vary, so Mode 0 is the current operational surface reference. A future independent oracle must reproduce the Mode-0 surface contract exactly rather than subdividing the coarse world mesh.

The visible `31 x 31` grid on `curved_surface + disp_cobble` was traced to missing proxy-domain clipping in the Mode-1 world-microtriangle leaf test. A texture microtriangle that crossed an original proxy edge was mapped through the current proxy and accepted in full. Adjacent proxies therefore produced overlapping extrapolated surface copies, and closest-hit/normal selection switched at their shared boundaries. Mode 0 already rejects roots whose coarse-triangle barycentrics lie outside its proxy.

Mode 1 now reconstructs the coarse proxy barycentrics at every linear leaf hit and accepts only finite coordinates in the closed interval `[-1e-5, 1+1e-5]`. Closed tolerance permits both primitives to own an exact shared edge while rejecting material extrapolation. The CPU audit records the same ownership contract, and the combined Mode-1/A1/A2 suite passes 13 tests.

On the frozen curved/cobble AOV, relative to Mode 0:

| metric | before proxy clipping | after proxy clipping |
|---|---:|---:|
| hit-mask disagreement | `0.0000613` | `0.0000613` |
| mean relative position | `6.95e-6` | `5.82e-6` |
| p99 relative position | `3.09e-5` | `2.57e-5` |
| median normal error | `0.0396 deg` | `0.0343 deg` |
| p99 normal error | `2.496 deg` | `0.799 deg` |
| pixels above `2.5 deg` | `0.999%` | `0.222%` |

The coarse grid is absent in the 64-spp HDR rerender. Quad/brick and twisted/rock retain near-zero p99 differences against Mode 0; sphere/terrain improves to zero mask disagreement and `0.208 deg` p99 normal error, although rare large position outliers remain a separate exact-surface question. Analytic-normal replacement and nonlinear leaf refinement are therefore deferred: they are not required for the seam repair, and should be evaluated later only as explicit same-surface accuracy/performance ablations.

The current presentation folder is `.tmp/mode1_certified/rendered_images_64spp_env_m0_reference/`. It contains thirteen PNG files only: eight full-resolution Mode-0/M1 renders, four large two-column comparisons, and one certified-M1 overview. The incorrect dense-reference column is omitted. Frozen twisted-rock timing is `908.4 ms` for the path pass with unchanged work counts (`9.28` segments, `79.19` cell tests, and `1.98` leaf tests per active pixel), so the domain predicate introduces no measured regression relative to the prior approximately `916 ms` path.
