---
title: Plan — A1 GPU mathematical port and differential tests
tags: [plan, ray-tracing, CUDA, differential-testing, certification, triangle-proxy]
status: active-device-differential
created: 2026-08-24
updated: 2026-08-25
parent: "[[Plan — Certified ray architecture comparison]]"
predecessor: "[[Plan — P-D1 certified tube-supercover DDA reference]]"
implementation-status: a1-c1-through-c13-3-complete-a2-authorized
---

# Plan — A1 GPU mathematical port and differential tests

> [!abstract] Purpose
> Port the successful P-D1 mathematics to isolated CUDA primitives and prove differential agreement before building a traversal. A1 does not implement `CERT-DDA-MM`, change historical Mode 1, or measure method speed. It produces versioned CPU/device vectors and a correctness gate for the later A2 architecture implementation.

## 1. Frozen input and represented surface

The authoritative CPU source is P-D1.6 run `p-d1-6-e254f252e010`: 864 cases, 48 practical windows, three triangle-shell families, and six ray families. Its gate has zero event/exhaustive and event/recursive root-group disagreement, zero constructed-hit omission, and passing binary64-versus-Decimal checks.

The represented surface remains

$$
F(u,v,h)=B(u,v)+hN(u,v),\qquad h=d(u,v),
$$

where `B` and `N` are affine over one triangle and each texture cell has two affine-height microtriangles. No Catmull–Clark surface, first-order displacement plane, or planar world-microtriangle substitute is permitted.

Source artifacts are content-hashed in every A1 run:

- `experiments/p_d1_full_reference/p-d1-6-e254f252e010/result.json`;
- `experiments/p_d1_lazy_segmentation/p-d1-2-ec0dd6a036f5/rays.jsonl.gz`;
- the P-D1 Python reference modules; and
- the CUDA probe binary and relevant device source.

## 2. Precision and comparison contract

Production inputs and device arithmetic are binary32. The exporter first quantizes positions, displacement directions, texture coordinates, rays, heights, and cell data to binary32. References are then recomputed from those exact binary32 values in binary64 or Decimal; comparing against the older unquantized Python inputs is forbidden.

Three different comparisons are required:

1. **Algebraic quantities** such as coefficients use a condition-aware outward absolute-error interval based on the sum of absolute algebraic terms. ULP distance is recorded as a diagnostic, not used alone near cancellation.
2. **Certified intervals** must contain the CPU reference interval after the recorded outward binary32 widening. An interval that is merely close but narrower is a failure.
3. **Discrete and geometric results**—candidate cells, root groups, closest root, and owner sets—must have zero omissions. Extras are allowed only at conservative intermediate stages and are counted. Final represented roots use the P-D1 coordinate and residual tolerances.

The probe is compiled with the production CUDA floating-point flags. Directed-rounding intrinsics or explicit nextafter widening are required where an interval endpoint is claimed conservative. Fast-math results may be used for non-certifying center estimates but may not silently define a lower or upper bound.

NaN, infinity, zero denominator, tangent root, collapsed interval, edge/corner tie, and signed-zero behavior are explicit classifications. None may be converted into a miss. Every bounded buffer has a counted conservative fallback; no fixed cap may truncate work.

## 3. Versioned vector format

Every substage writes a human-auditable `vectors.jsonl.gz` and a packed little-endian device input. The common manifest schema is `ray-architecture-a1-manifest-v1`. Each vector contains:

- stable integer `case_index` and string case/ray ID;
- source run and source-record hashes;
- shell, asset/window, and ray-family strata;
- exact binary32 input bit patterns as hexadecimal strings in JSON;
- decoded numeric values for readability;
- binary64/Decimal reference values;
- outward acceptance intervals or exact discrete expected sets; and
- expected status/fallback classification.

Packed files have a fixed header containing an eight-byte magic, schema version, primitive ID, record count, record byte size, and SHA-256 of the canonical JSON vector stream. Every record begins with `case_index`. The CUDA output repeats the header hash and case index so stale or reordered results cannot pass.

Content-addressed outputs live under `experiments/ray_architecture_a1/`. A run is invalid if JSON, binary, source, or executable hashes disagree.

## 4. Ordered primitive stages

### A1.0 — vector exporter and schema validator

Export all 864 P-D1 ray cases after binary32 quantization. Reconstruct the three frozen shells by ID and use the final P-D1.2 v3 ray artifact. Add analytic records for axis-aligned directions, near-cancellation, exact affine shells, signed zero, and denominators close to—but separated from—zero.

The initial packed primitive is `C1-COEFFICIENTS`. Its inputs are the three positions, three displacement directions, three texture coordinates, ray origin, and an exported orthonormal frame. Exporting the frame isolates coefficient arithmetic; production frame construction is tested separately before composition.

**Gate:** deterministic byte-identical rerun; unique/stable IDs; all source hashes match; every float JSON bit pattern matches its packed value; no nonfinite ordinary input; exact record count; schema corruption and reorder tests fail closed.

### A1.1 — rational shell-ray coefficients and evaluation

Split this into two tests:

1. `C1-COEFFICIENTS`: device evaluation of canonical numerator/denominator and texture-space numerator coefficients using the exported frame.
2. `C2-FRAME-EVAL`: production coordinate-frame construction plus rational evaluation of `(u,v,h,t)` at ordinary endpoints and analytic stress heights.

For coefficients, compare against exact-real evaluation of the binary32 inputs enclosed by a frozen condition-aware error interval. For evaluated coordinates, denominator classification must agree first; regular coordinates must lie in the reference interval, while near-singular cases are handed to A1.2 rather than divided unsafely.

**Gate:** zero status mismatch; every finite device coefficient/evaluation lies within its reference enclosure; exact affine-shell zero terms remain zero; no unreported nonfinite result.

### A1.2 — denominator, derivative, and boundary events

Port outward quadratic root isolation for:

- the shared denominator;
- texture-coordinate derivative numerators;
- proxy boundaries `u=0`, `v=0`, and `u+v=1`; and
- any additional monotonicity/ray-distance condition frozen by P-D1.

Device output is a sorted set of outward root brackets plus regular/singular interval classifications. Tangencies and overlapping brackets are merged conservatively. Exact roots are never used as unguarded floating endpoints.

**Gate:** every CPU root bracket is overlapped/contained by a device bracket; zero missing partitions; regular intervals exclude zero denominator; all extra partitions and singular fallbacks are reported; no capacity loss.

### A1.3 — chord certificate

Port the rational second-derivative numerator, Bernstein/interval range, endpoint evaluation, and secant-error bound for `u` and `v`. The GPU stores an outward binary32 rectangle `(epsilon_u,epsilon_v)`; it does not use the rejected first-order directional predicate.

**Gate:** every device tube contains the dense/high-precision P-D1 curve audit and the CPU certified tube; affine cases have a zero or minimal outward radius; zero false certification when the denominator is unresolved; conservative fallback on resource failure.

### A1.4 — closed tube-supercover

Implement a device reference kernel before optimizing an incremental DDA. Given a certified chord tube and grid level, enumerate closed-set candidate cells and merged parameter intervals. Simultaneous x/y crossings include every edge/corner owner; no epsilon jump is allowed.

This stage may use a deliberately simple per-vector output buffer with an overflow flag. Overflow must trigger a host-side exhaustive conservative record for testing; it may not return a partial candidate set.

**Gate:** zero CPU event/recursive candidate omission over all P-D1 tubes and analytic ties; every interval contains the CPU interval; reverse-direction ownership consistency; all extras and overflow fallbacks counted.

### A1.5 — exact cubic leaf and ownership

Port construction and solution of

$$
G(h)=hD(h)-A,U(h)-B,V(h)-C,D(h)=0
$$

for each affine-height leaf triangle. Filter roots by closed height, UV, proxy, world-ray, and owner constraints. Return actual rational `(u,v,h,t)`, not chord coordinates.

**Gate:** zero represented-root, closest-root, or owner-set omission against P-D1.6; every hit satisfies the frozen polynomial, height, and world residual thresholds; tangencies and shared edges/corners pass; overflow/failure is conservative and explicit.

### A1.6 — composed differential replay

Compose A1.1–A1.5 only after every isolated gate passes. Replay all 864 cases and the analytic stress set without OptiX broad phase or performance timing. Compare device partitions, tubes, cells, roots, closest roots, and owners with persisted CPU vectors.

**A1 final gate:** zero candidate/root/closest-root/owner omission; zero unreported resource loss; every claimed interval encloses the reference; every hit passes residual thresholds; deterministic results under repeated and reordered batches.

## 5. Stop rules

- Stop at the producing primitive on the first omission; do not compensate by widening an unrelated downstream stage.
- A condition-aware widening change requires regenerating its vectors and all dependent artifacts.
- Do not add performance shortcuts, shared traversal state, packed hierarchy layouts, or OptiX integration in A1.
- Do not modify or rename Mode 1.
- Do not time A1 primitives as evidence. Profiling may be used only to catch accidental pathological resource use.
- Failure of the A1 final gate blocks A2.

## 6. Immediate authorized work

A1.0, `C1-COEFFICIENTS`, and `C2-FRAME-EVAL` have passed and are recorded below. C3 is split into independent gates so an error in one event family cannot be hidden by another:

1. `C3a-DENOMINATOR-BRACKETS`: shared-denominator singular intervals;
2. `C3b-UV-PROXY-EVENTS`: texture-coordinate derivative and proxy-boundary events; and
3. `C3c-T-ORDER-EVENTS`: the separately derived ray-distance monotonicity/order events.

All C1–C3 substages now pass. The next authorized action is to freeze the detailed A1.3 chord-certificate and lazy-segmentation device subplan. No A1.3 device code may be added until that addendum is written here. DDA, leaf solving, traversal timing, and changes to historical Mode 1 remain unauthorized until their own ordered gates.

## 7. Execution record — 2026-08-24

### 7.1 A1.0 vector gate

Run `ray-a1-0-vectors-d5637078cdef` exports 870 immutable records: all 864 P-D1 practical ray cases plus six analytic axis, signed-zero, near-axis, affine, and balanced-direction cases. Each record stores binary32 input bits, decoded values, binary64 coefficient references, and algebraic term scales. The packed input repeats the canonical JSON SHA-256.

The A1.0 gate passes:

- exact P-D1 record count and six analytic records;
- stable unique IDs and contiguous indices;
- JSON/binary binary32 bit agreement;
- packed round trip for every record;
- deliberate corrupt-header rejection; and
- byte-identical reconstruction of JSON and packed inputs.

This artifact is vector plumbing only; it contains no device result.

### 7.2 A1.1 `C1-COEFFICIENTS` device gate

Run `ray-a1-1-c1-9db5bd49db2b` evaluates the canonical and texture coefficient algebra in a standalone CUDA kernel compiled with production fast-math flags. It consumes the exported frame so frame construction cannot contaminate this first comparison. No NRTDSM, Mode 1, OptiX traversal, hierarchy, or timing kernel is invoked.

Across `870 × 15 = 13,050` coefficients:

| Check | Result |
|---|---:|
| Device status/index/schema agreement | pass |
| Finite ordinary outputs | pass |
| Condition-aware enclosure | pass |
| Exact structural zeros | pass |
| Maximum absolute error | `6.9893e-08` |
| Maximum normalized enclosure use | `0.009161` |
| Maximum ULP distance, diagnostic only | `11` |
| **C1 gate** | **pass** |

The enclosure is `64 * epsilon_binary32 * max(1, absolute_term_sum)`. The largest observed error uses less than one percent of that allowance. This supports only the isolated coefficient port. Frame construction, rational coordinate evaluation, singular partitions, tubes, cells, leaves, and roots remain untested on the GPU.

### 7.3 Current stop point

The `C2-FRAME-EVAL` subplan is frozen below and is now authorized for implementation. It remains an isolated device-math test; event isolation and traversal are not authorized.

## 8. Frozen `C2-FRAME-EVAL` subplan

### 8.1 One vector per ray/height sample

Each packed C2 input record contains:

- stable global `sample_index` and parent A1.0 `case_index`;
- binary32 proxy positions, displacement directions, and texture coordinates;
- binary32 world-ray origin and direction; and
- one binary32 shell height `h`.

The frame is no longer an input. The device must construct the production Frisvad frame from the ray direction, recompute the already validated C1 coefficients, evaluate the denominator, and either return guarded status or evaluate `(a,b,u,v,t)`.

The output record contains `sample_index`, parent `case_index`, `frame0`, `frame1`, `D(h)`, `(a,b,u,v,t)`, and a status enum. Output order and the source-vector SHA-256 are checked before numerical comparison.

### 8.2 Frozen height strata

For every one of the 870 C1 cases, export binary32 heights `{0, 0.25, 0.5, 0.75, 1}`. For every real denominator root in the closed shell-height interval, additionally export:

1. the root rounded to binary32;
2. one sample on each side at a scale chosen from the local derivative and the denominator guard; and
3. adjacent binary32 values around the rounded root when distinct.

Duplicate height bit patterns are merged per parent case. Root-derived heights are test placement only; C2 does not claim to isolate roots. Certified outward root brackets remain A1.2/C3 work.

### 8.3 Reference and status policy

The coordinate reference is an independent binary64 fixed-height inverse of the shell map using the exact binary32 geometry/ray values. It does not reuse the device frame or C1 device output. The reference frame uses binary64 normalization plus the same Frisvad construction and is compared with a componentwise enclosure.

Let

$$
s_D(h)=|d_0|+|h,d_1|+|h^2d_2|,
$$

and freeze the basic device denominator guard

$$
g_D=128\epsilon_{32}\max(1,s_D).
$$

Reference records are classified with separation bands:

- `regular` if `|D_ref| > 4 g_D`;
- `guarded` if `|D_ref| <= 0.5 g_D`; and
- `gray` otherwise.

The device computes the same basic guard from its binary32 coefficients and divides only when `|D_device| > 4 g_D`; otherwise it returns guarded status for later event handling. A regular reference must return regular coordinates. A guarded reference must not divide. A gray reference may return guarded, or may return regular only when all returned coordinates pass their enclosures. Any nonfinite coordinate from a claimed regular result fails.

The frame enclosure is `256 epsilon32` per component. Regular coordinate enclosures use a first-order condition estimate from numerator/denominator term sums, widened by `512 epsilon32` with a scale floor. ULP distances are diagnostic only. The exact formulas and constants are persisted in the C2 manifest before execution.

### 8.4 C2 gate

C2 passes only if:

- vector JSON and packed inputs regenerate byte-identically and reject corrupt/reordered schemas;
- sample indices and parent case indices are preserved;
- every frame component lies inside the frozen enclosure, including axes and signed zero;
- all regular references return finite `(a,b,u,v,t)` inside their condition-aware enclosures;
- all guarded references avoid division and report guarded status;
- every gray reference is either guarded or returns valid enclosed coordinates;
- all reconstructed regular world points satisfy the binary32-input shell/ray residual tolerance; and
- there is no unreported device failure.

Passing C2 authorizes a separately written C3 denominator/event-isolation subplan. It does not authorize chord construction, DDA, leaves, or timing.

## 9. C2 execution record — 2026-08-24

### 9.1 Vector artifact

Run `ray-a1-1-c2-vectors-bd273d03380b` contains 4,361 immutable ray/height records derived from all 870 C1 parents. It includes 4,350 clearly regular references, eight guarded references, and three gray references; 13 records are denominator-root-derived. JSON/binary bit agreement, corruption rejection, packed round trip, and byte-deterministic regeneration all pass.

### 9.2 First device run and producing-stage correction

The first device run allowed division whenever `|D_device| > g_D`. Two gray analytic samples then returned coordinates of magnitude about 9,250 and exceeded the fixed world-residual gate (`4.17e-4` and `2.49e-4` versus `2e-4`), although their broad condition-aware coordinate enclosures passed. This was not repaired by increasing the residual tolerance.

The producing classification was corrected to match the frozen regular band: C2 divides only when `|D_device| > 4 g_D`. Ambiguous gray samples are now explicitly deferred to event handling. C1 was rerun after the shared probe changed and retained its byte-identical comparison result in run `ray-a1-1-c1-ff85a9251f87`.

### 9.3 Final C2 gate

Authoritative run: `ray-a1-1-c2-gpu-4705dd3c19ef`.

| Check | Result |
|---|---:|
| Regular references returned regular | `4350 / 4350` |
| Guarded references avoided division | `8 / 8` |
| Gray references conservatively deferred | `3 / 3` |
| Frame enclosure | pass |
| Denominator enclosure | pass |
| Coordinate enclosures | pass |
| Maximum frame absolute error | `7.5814e-08` |
| Maximum denominator absolute error | `1.2423e-07` |
| Maximum normalized coordinate error | `0.64069` |
| Maximum regular world residual | `3.3778e-05` |
| Frozen world-residual tolerance | `2e-04` |
| **C2 gate** | **pass** |

C2 establishes the actual world-ray-to-fixed-height shell-curve evaluation path, including production frame construction and conservative near-singular deferral. It does not locate denominator roots, construct certified intervals, segment a curve, traverse cells, or intersect a surface.

### 9.4 C2 handoff stop point

At the C2 handoff, the C3a subplan below was frozen. Its vector, CUDA, and differential gates have since passed and are recorded in Section 11.

## 10. Frozen C3 event-isolation subplan

### 10.1 Why C3 is three gates

The final curve partition is not claimed to be the globally minimum number of cuts. It contains two kinds of cuts:

- **mandatory algebraic cuts**, which separate singularities and changes in the conditions needed by traversal; and
- **adaptive chord cuts**, introduced later by the node-local tube certificate.

C3 tests only the mandatory algebraic layer. Denominator singularities are first because every later rational event polynomial and chord certificate depends on regular denominator intervals. UV/proxy events can reuse the same quadratic bracket primitive after their formulas are frozen. Ray-distance ordering is kept separate because its event degree and conditioning must be derived rather than assumed.

### 10.2 C3a input and coefficient enclosure

Each C3a record contains a stable polynomial index, parent C1 case index (or an analytic sentinel), and outward binary32 intervals for

$$
D(h)=d_2h^2+d_1h+d_0,\qquad h\in[0,1].
$$

For practical records, the center and absolute term scales come from the immutable C1 binary32-input/binary64 reference. A non-structural coefficient receives the frozen C1 enclosure

$$
e_i=64\epsilon_{32}\max(1,s_i),
$$

followed by outward binary32 rounding of $[d_i-e_i,d_i+e_i]$. A structural zero with zero term scale remains exactly $[0,0]$. This deliberately encloses both the exact-real coefficient of the binary32 inputs and the validated C1 device result. Composition with device-computed coefficients remains an A1.6 test.

The practical 870 polynomials are augmented with exact and uncertain analytic cases: no root, two roots, linear degeneration, a double/tangent root, roots at both closed endpoints, a near-linear quadratic, clustered roots, roots outside the domain, an identically zero polynomial, and a coefficient interval whose constant term straddles zero.

### 10.3 Correctness-first device bracket representation

C3a initially uses a uniform closed interval partition with `N = 4096` elementary bins. This is a differential-testing oracle for the GPU port, not the final runtime root solver and not a paper performance result. For every bin $X_i=[i/N,(i+1)/N]$, the device evaluates the interval polynomial with directed-rounding interval Horner arithmetic:

$$
(( [d_2]X_i+[d_1])X_i+[d_0]).
$$

A bin is retained exactly when its outward range contains zero. Adjacent retained bins are merged into sorted closed components. The fixed output holds at most two components, which is sufficient for an ordinary scalar quadratic; however, capacity is never trusted as a correctness assumption. More than two components, invalid input, nonfinite interval arithmetic, or any unclassified state returns the explicit conservative full-domain fallback $[0,1]$. An all-active or identically-zero polynomial is also reported as a full-domain singular result rather than a regular interval.

The packed output contains:

- polynomial and parent indices;
- status (`regular-brackets` or `full-domain-fallback`);
- merged component count and retained elementary-bin count; and
- two outward bracket endpoint pairs.

Exact floating roots are never emitted as unguarded partition endpoints. Downstream work consumes bracket complements as regular candidate intervals only after the denominator exclusion gate below.

### 10.4 Independent reference and C3a gate

The CPU reference uses Decimal arithmetic on the exact binary32 coefficient-interval endpoints and exact dyadic bin endpoints. It also records high-precision roots of the center polynomial, including endpoint and repeated roots. It does not reuse device interval arithmetic.

C3a passes only if:

- JSON and packed vectors regenerate byte-identically and reject stale/corrupt/reordered schemas;
- polynomial/parent indices and source digest are preserved;
- every high-precision center root in `[0,1]` is covered by a returned bracket;
- every elementary bin whose independent Decimal interval contains zero is covered by a returned bracket;
- every omitted elementary bin has a Decimal interval that excludes zero, establishing the regular complement at the elementary-bin resolution;
- tangent/double, endpoint, linear-degenerate, near-linear, and all-zero cases are retained conservatively;
- returned brackets are sorted, closed, grid-aligned, nonoverlapping, and finite;
- every capacity or arithmetic uncertainty becomes the counted full-domain fallback; and
- there is zero unreported or partial capacity loss.

False-positive retained bins are allowed and counted. Timing and the number of scanned bins are not performance evidence. Passing C3a authorizes the explicit C3b derivative/boundary formulas, not chord segmentation or DDA.

### 10.5 Formulas reserved for C3b

For a rational coordinate $q(h)=P(h)/D(h)$ with

$$
P(h)=p_2h^2+p_1h+p_0,
$$

the derivative event numerator is the quadratic

$$
P'D-PD'=
(p_2d_1-p_1d_2)h^2+
2(p_2d_0-p_0d_2)h+
(p_1d_0-p_0d_1).
$$

C3b will instantiate it for texture `u` and `v`. Its proxy-boundary polynomials are the relevant canonical numerators for `a=0`, `b=0`, and `a+b=1` (equivalently `A`, `B`, and `A+B-D`), rather than assuming atlas texture coordinates are proxy barycentrics. Their coefficient-enclosure propagation and exact boundary ownership policy must be frozen before C3b code is added.

## 11. C3a execution record — 2026-08-25

### 11.1 Persisted artifacts

- vectors: `ray-a1-2-c3a-vectors-4d99a692576e`;
- device differential: `ray-a1-2-c3a-gpu-fa79335afba4`;
- current-binary C1 regression: `ray-a1-1-c1-983022c95140`; and
- current-binary C2 regression: `ray-a1-1-c2-gpu-3ccbf0388492`.

The vector artifact contains 880 polynomials: all 870 C1 parents plus ten exact/uncertain analytic classes. JSON/binary bit agreement, packed round trip, deliberate stale/corrupt rejection, and byte-deterministic regeneration pass.

### 11.2 Device differential result

The standalone CUDA primitive scans 4,096 closed elementary intervals with directed-rounding interval Horner evaluation. Against the independent 80-digit Decimal reference:

| Check | Result |
|---|---:|
| Decimal-active bins omitted | `0` |
| Center roots omitted | `0` |
| Invalid/sorted/grid-alignment bracket records | `0` |
| Analytic classes passing | `10 / 10` |
| Extra conservative bins | `1` |
| Full-domain results | `2` |
| **C3a gate** | **pass** |

The two full-domain results are the intentional `identically-zero` and `uncertain-zero` analytic records. No practical P-D1 case falls back. Three C1 analytic frame cases retain one endpoint bin each; the 864 P-D1 practical rays contain no denominator singularity in their frozen safe height bands, consistent with the CPU corpus construction.

The C1 and C2 gates pass unchanged against the same rebuilt executable. The complete A1 unit discovery reports 22/22 passing tests, the `nrtdsm`, `tfdm`, and `ray_a1_math_probe` Release targets build, and `git diff --check` reports no patch errors.

### 11.3 Interpretation and next stop

C3a establishes a conservative GPU representation of denominator-uncertain regions and a proven regular complement at the frozen elementary-bin resolution. It does not establish an efficient runtime root solver, a final segmentation policy, DDA correctness, or speed. The 4,096-bin scan is disposable test scaffolding.

Before C3b implementation, freeze:

1. which proxy/domain events are correctness-required and which UV turning events are optional knot candidates;
2. outward coefficient-interval propagation for `u'`, `v'`, `a=0`, `b=0`, and `a+b=1`;
3. how overlapping event brackets and closed proxy ownership form valid regular intervals; and
4. analytic affine-UV, swapped/skewed-atlas, double-turning, proxy-edge, and coincident-event vectors.

## 12. Frozen C3b UV/proxy-event subplan

### 12.1 Event roles are not interchangeable

C3b constructs five quadratic event families from the C1 rational coefficients:

| Event | Polynomial | Role when unresolved |
|---|---|---|
| proxy `a=0` | `A(h)` | correctness-required; retain boundary uncertainty or fallback |
| proxy `b=0` | `B(h)` | correctness-required; retain boundary uncertainty or fallback |
| proxy `a+b=1` | `C(h)=D(h)-A(h)-B(h)` | correctness-required; retain boundary uncertainty or fallback |
| texture `u` turn | `U'D-UD'` | optional knot candidate; chord certification remains authoritative |
| texture `v` turn | `V'D-VD'` | optional knot candidate; chord certification remains authoritative |

The UV turning roots are useful because splitting at a coordinate extremum can reduce tube width and DDA work. They are not required for correctness once A1.3 supplies a certified chord tube. Consequently an unresolved/full-domain UV-turn result disables that optional pre-cut and increments a counter; it does not create a miss or force an unbounded split loop. Proxy-boundary uncertainty is different: the ray may enter or leave the represented triangle, so it is retained conservatively and may force the later nonlinear/exhaustive fallback.

Atlas texture coordinates and proxy barycentrics are kept separate. In particular, proxy validity is never inferred from texture `u`, `v`, or their derivatives. This directly fixes the conceptual weakness in the historical Mode 1 turning-event selection for non-identity/skewed UV maps.

### 12.2 Formula and interval propagation

Write each numerator and denominator in ascending power notation,

$$
P(h)=p_0+p_1h+p_2h^2,\qquad D(h)=d_0+d_1h+d_2h^2.
$$

The rational derivative numerator is

$$
E_P(h)=P'D-PD'
=(p_1d_0-p_0d_1)
+2(p_2d_0-p_0d_2)h
+(p_2d_1-p_1d_2)h^2.
$$

The C3b packed parent record contains outward intervals for the 15 C1 coefficients `A`, `B`, `D`, `U`, and `V`, using exactly the C3a structural-zero and `64 epsilon32 max(1,s_i)` policy. The device constructs all five event coefficient intervals with directed-rounding interval multiply, add, subtract, and exact multiplication by two. It then sends each polynomial through the already validated C3a bracket scan. The device must not compute a center event polynomial and attach an unrelated generic epsilon afterward.

The independent CPU reference uses 80-digit Decimal arithmetic on the exact binary32 input-interval endpoints and the same declared expression tree. It separately constructs the center event coefficients from the C1 binary64 centers and records their high-precision roots. The gate checks both coefficient containment and root-bin coverage, so a formula/order/sign error cannot hide behind a wide bracket.

### 12.3 Output and merging contract

Each parent produces five fixed-order event records. Every record repeats parent index and event kind and stores:

- the three constructed outward coefficient intervals;
- regular-bracket versus full-domain status;
- retained elementary-bin count; and
- up to two sorted closed bracket components.

Unexpected component count, invalid input, nonfinite interval arithmetic, or bounded-output pressure returns `[0,1]` for that event kind. No partial components survive. Coincident or overlapping event brackets remain independently attributable at C3b and are unioned only by the later composed partition, where their event-kind bit mask is preserved.

For a regular interval outside the union of the mandatory `D`, `A`, `B`, and `C` uncertain bins, denominator sign is fixed. Proxy validity is

$$
A/D\ge0,\qquad B/D\ge0,\qquad C/D\ge0.
$$

The later composition classifies this with interval signs, not a tolerance-shifted midpoint. Closed mandatory-event bins are conservatively attached to every adjacent valid interval whose closure they touch; if validity cannot be decided, the bin is retained for fallback. C3b itself verifies the event producer but does not yet emit final valid shell intervals.

### 12.4 Vector strata

Use all 870 C1 parents and add direct analytic event records plus synthetic parent-coefficient records for:

- identity affine UVs;
- swapped, scaled, and skewed atlas UVs whose texture turns differ from `a`/`b` turns;
- linear-degenerate and exact structural-zero terms;
- a repeated/double UV-turn root;
- proxy-edge roots at `h=0`, `h=1`, and the interior;
- simultaneous `a=0`/`b=0`, UV-turn/proxy, and multiple-event roots;
- clustered roots within one elementary bin; and
- full-domain coefficient uncertainty.

Direct analytic event polynomials stress the reused bracket primitive; synthetic parents stress the C3b formula construction. Every analytic class is named and independently gated.

### 12.5 C3b gate and stop

C3b passes only if:

- vector/schema determinism, bit agreement, corruption/reorder rejection, and source hashes pass;
- every device event coefficient interval contains the independently constructed Decimal interval;
- every Decimal-active event bin and every center root is covered;
- proxy and UV event kinds are never exchanged, including skewed-atlas cases;
- all double, endpoint, coincident, clustered, structural-zero, and uncertain cases are retained;
- output components are finite, sorted, grid-aligned, nonoverlapping, and their stored bin counts agree;
- every exceptional state becomes an event-attributable full-domain result; and
- C1, C2, and C3a regressions still pass against the changed executable.

Passing C3b authorizes the C3c ray-distance derivation. It does not authorize A1.3 chord code, DDA, timing, or a claim that these are the best/minimum cuts. The later A2 ablation must compare mandatory-only, mandatory-plus-turning, and lazy certificate-driven cut policies at equal correctness.

## 13. C3b execution record — 2026-08-25

### 13.1 Persisted artifacts and scope

- vectors: `ray-a1-2-c3b-vectors-108670af5305`;
- device differential: `ray-a1-2-c3b-gpu-f687c5bdd6e1`;
- current-binary C1: `ray-a1-1-c1-c5b8c97ad399`;
- current-binary C2: `ray-a1-1-c2-gpu-ad7ffadea7ec`; and
- current-binary C3a: `ray-a1-2-c3a-gpu-f3dbd8c5f44f`.

The C3b corpus contains 4,374 event records: five events for each of the 870 C1 parents, twenty events from four synthetic parent-coefficient cases, and four direct analytic event polynomials. The device receives source coefficient intervals and constructs `A`, `B`, `D-A-B`, `U'D-UD'`, or `V'D-VD'` itself with directed interval arithmetic before invoking the common bracket scan.

### 13.2 Differential result

| Check | Result |
|---|---:|
| Device event-coefficient enclosure | pass |
| Decimal-active event bins omitted | `0` |
| Center roots omitted | `0` |
| Extra retained bins | `0` |
| Analytic records passing | `24 / 24` |
| Full-domain proxy events | `0` |
| Full-domain `u`-turn events | `3` |
| Full-domain `v`-turn events | `3` |
| **C3b gate** | **pass** |

The six full-domain turning events are expected: the C1 analytic affine shell has identically zero `u` and `v` derivatives, the synthetic identity case has constant `v`, the structural-zero case has constant `u` and `v`, and the direct uncertain-turn record straddles zero. No P-D1 practical event uses a full-domain fallback. This supports the frozen role distinction: unresolved proxy boundaries would be correctness-critical, whereas an identically-zero or uncertain texture derivative merely disables an optional turning pre-cut.

The complete A1 unit discovery now passes 26/26 tests. The `nrtdsm`, `tfdm`, and `ray_a1_math_probe` Release targets build, all C1–C3a current-binary regressions pass unchanged, and `git diff --check` reports no patch errors.

### 13.3 Interpretation

C3b proves the GPU formula construction and conservative bracketing for proxy boundaries and texture-coordinate turns, including skewed atlas mappings. It does not show that adding turning knots is faster. A2 must measure whether optional `u`/`v` turns reduce certificate splits, tube area, DDA cells, or elapsed time enough to repay their event cost.

## 14. Frozen C3c ray-distance event subplan

### 14.1 Rational ray time and event degree

Let `A(h)`, `B(h)`, and `D(h)` be the quadratic canonical coefficients and let `r` be the unnormalized world-ray direction. Define three affine dot polynomials

$$
\begin{aligned}
L_0(h)&=(p_0-o+h n_0)\cdot r,\\
L_1(h)&=((p_1-p_0)+h(n_1-n_0))\cdot r,\\
L_2(h)&=((p_2-p_0)+h(n_2-n_0))\cdot r.
\end{aligned}
$$

Then

$$
t(h)=\frac{T(h)}{\lVert r\rVert^2D(h)},\qquad
T=L_0D+L_1A+L_2B,
$$

where `T` is cubic. Therefore the closed ray-range events

$$
T-t_{min}\lVert r\rVert^2D=0,\qquad
T-t_{max}\lVert r\rVert^2D=0
$$

are cubic, while the ray-time turning numerator

$$
T'D-TD'
$$

is generally quartic. C3c must not send these through the quadratic-only C3a capacity or silently drop the highest terms.

### 14.2 Correctness versus ordering role

The `t_min` and `t_max` events are mandatory if the composed shell-domain partition uses them to discard intervals; unresolved brackets retain the interval or enter the conservative fallback. Exact leaf roots always recheck the closed ray range.

The `t'(h)=0` events are optional traversal-order candidates. Closest-hit correctness does not depend on monotone `t(h)` if every admitted leaf root is evaluated and the smallest valid `t` is retained. A nonmonotone interval merely prevents first-hit/ordered early exit. Thus a full-domain `t`-turn result disables ordered early exit and increments a counter; it cannot produce a miss or an infinite split sequence. Any-hit rays may terminate on any exact valid occluder and do not require `t` monotonicity.

### 14.3 Ordered C3c substages

1. `C3c.0-T-COEFFICIENTS`: construct interval dot polynomials `L0/L1/L2`, `|r|^2`, and cubic `T` from binary32 geometry/ray inputs plus validated `A/B/D` coefficient intervals.
2. `C3c.1-T-RANGE`: construct and bracket the two cubic `t_min/t_max` event polynomials.
3. `C3c.2-T-TURN`: construct and bracket the optional quartic `T'D-TD'` polynomial.

Each substage gets its own vector and differential result. A failure stops at the producing substage; a wide quartic bracket may not be repaired by widening a downstream chord tube.

### 14.4 Interval construction and generic scan

The packed input contains exact binary32 proxy positions, displacement directions, ray origin/direction, `t_min/t_max`, and outward `A/B/D` intervals. The device treats geometry scalars as point intervals and constructs subtraction, dot products, polynomial convolution, and event coefficients with directed rounding. The CPU reference uses 80-digit Decimal arithmetic on those exact inputs and independently constructs center coefficients in binary64.

Extend the correctness-only scan from quadratic to degree four using interval Horner evaluation on the same 4,096 closed bins. Cubic outputs hold at most three components and quartic outputs at most four. Invalid arithmetic, unexpected component count, or any output pressure returns the full `[0,1]` domain for that event. This scan remains disposable A1 scaffolding, not the final runtime solver.

### 14.5 Vector strata and gate

Use all 864 persisted P-D1 rays plus the six C1 analytic parents after restoring their declared/frozen `t_min/t_max`. Add analytic cases for:

- constant, affine, quadratic, and cubic `T` degenerations;
- roots at `h=0` and `h=1`;
- repeated and clustered cubic range roots;
- quartics with zero through four closed-domain components;
- identically-zero and interval-uncertain `t` turns;
- non-unit ray directions, signed-zero axes, and cancellation in `|r|^2`; and
- a deliberately nonmonotone `t(h)` with multiple valid leaf-time candidates.

C3c passes only if every device `L`, `|r|^2`, `T`, range-event, and turn-event coefficient interval encloses the independent reference; every Decimal-active bin and center root is covered; event degree/status/kind is preserved; analytic degeneracies and endpoint roots pass; every overflow/uncertainty becomes full-domain; and C1–C3b regressions pass on the changed executable.

Passing C3c authorizes planning A1.3 chord certificates. It still does not freeze the final cuts. A2 must separately ablate:

1. mandatory denominator/proxy/range events only;
2. mandatory plus `u/v` turns;
3. mandatory plus `t` turns for ordered early exit; and
4. lazy certificate-driven cuts with profitable optional events.

## 15. C3c.0 execution record — 2026-08-25

### 15.1 Persisted artifacts

- vectors: `ray-a1-2-c3c0-vectors-51f83a037217`;
- device differential: `ray-a1-2-c3c0-gpu-04948405e4df`;
- current-binary C1: `ray-a1-1-c1-e7b0c257c25b`;
- current-binary C2: `ray-a1-1-c2-gpu-10d032e27c4a`;
- current-binary C3a: `ray-a1-2-c3a-gpu-d505a0e00512`; and
- current-binary C3b: `ray-a1-2-c3b-gpu-7a44500a1369`.

The C3c.0 corpus contains all 870 C1 parents plus four analytic non-unit, signed-zero-axis, dot-cancellation, and constant-shell cases. The GPU constructs `L0/L1/L2`, `|r|^2`, and cubic `T` from exact binary32 geometry/ray values and outward `A/B/D` intervals.

### 15.2 Differential result

| Check | Result |
|---|---:|
| Records | `874` |
| Output coefficient intervals | `9,614` |
| Status/index mismatch | `0` |
| Reference enclosure failures | `0` |
| Maximum device/reference width ratio | `40.0` |
| **C3c.0 gate** | **pass** |

The maximum width ratio occurs in the cancellation-prone `L2` slope of repeated oblique `S1-moderate` cases. Its reference interval is about `2.33e-10` wide and the stepwise directed device interval is about `9.31e-9` wide, so the ratio is large while the absolute width remains small. This is recorded as a potential coefficient-tightening target. It is not repaired by narrowing the reference or relaxing containment. C3c.1 must report whether this widening materially enlarges cubic event brackets or triggers full-domain fallbacks.

All current-binary C1–C3b regressions pass. The A1 unit discovery now passes 29/29 tests, all three Release targets build, and `git diff --check` reports no patch errors.

### 15.3 Next stop

C3c.0 authorizes C3c.1 only. Construct the persisted binary32 `t_min/t_max` lookup from the 864 P-D1 ray records, assign explicit ranges to the ten analytic parents, form the two cubic range polynomials from frozen C3c.0 intervals, and validate a degree-three full-domain-on-overflow bracket primitive. Do not add quartic `t` turns in the same change.

## 16. C3c.1 and C3c.2 execution record — 2026-08-25

### 16.1 C3c.1 cubic ray-range events

Authoritative artifacts:

- vectors: `ray-a1-2-c3c1-vectors-21315adbcb32`;
- device differential: `ray-a1-2-c3c1-gpu-dbd55e91a232`; and
- current-binary regression: `ray-a1-2-c3c1-gpu-801168e03a47`.

The corpus contains 1,752 cubic events: `t_min` and `t_max` for all 874 C3c.0 parents plus four analytic triple-root, endpoint-root, clustered-root, and uncertain-zero cases. The 864 practical records consume their persisted P-D1 binary32 ray ranges; analytic parents use explicit `[0,3]` ranges.

The first vector producer left the uncertain analytic endpoints as Python doubles while the packed device input quantized them to binary32. Its attempted differential failed the coefficient-enclosure check before any root omission. The producer was corrected at the source by quantizing those endpoints before constructing the Decimal reference; the earlier `ray-a1-2-c3c1-vectors-d588ee15a894` artifact is preserved but non-authoritative.

| C3c.1 check | Result |
|---|---:|
| Records | `1,752` |
| Decimal-active bins omitted | `0` |
| Center roots omitted | `0` |
| Extra retained bins | `0` |
| Full-domain results | `1` |
| **C3c.1 gate** | **pass** |

The one full-domain result is the intentional uncertain-zero analytic cubic. C3c.0's widest cancellation-driven coefficient interval causes no extra retained range bin in this corpus.

### 16.2 C3c.2 optional quartic ray-time turns

Authoritative artifacts:

- vectors: `ray-a1-2-c3c2-vectors-10e7c3cd2c29`; and
- device differential: `ray-a1-2-c3c2-gpu-290ecd261f18`.

The corpus contains 879 quartics: one constructed `T'D-TD'` event for each C3c.0 parent plus direct four-root, repeated-root, endpoint-root, identically-zero, and uncertain-turn stress cases. The device uses the algebraically simplified coefficients

$$
\begin{aligned}
e_4&=t_3d_2,\\
e_3&=2t_3d_1,\\
e_2&=3t_3d_0+t_2d_1-t_1d_2,\\
e_1&=2(t_2d_0-t_0d_2),\\
e_0&=t_1d_0-t_0d_1,
\end{aligned}
$$

with directed interval operations, avoiding avoidable dependency from separately expanding `T'D` and `TD'`.

| C3c.2 check | Result |
|---|---:|
| Records | `879` |
| Decimal-active bins omitted | `0` |
| Center roots omitted | `0` |
| Extra retained bins | `0` |
| Full-domain results | `2` |
| Parent curves with a turn in `[0,1]` | `0 / 874` |
| **C3c.2 gate** | **pass** |

The two full-domain records are the intentional identically-zero and uncertain analytic quartics. None of the 874 C3c.0 parent curves has a ray-time turning event in the tested closed height domain. Therefore the optional `t`-turn mechanism is correct but currently has no knot-reduction or early-ordering opportunity on this corpus. It should remain disabled by default until a later A2 workload demonstrates nonmonotone ray time and a net performance benefit.

### 16.3 Final C3 regression and handoff

Against the final C7-capable executable, current-binary C1 through C3c.1 all pass:

- C1: `ray-a1-1-c1-3ef9045766c5`;
- C2: `ray-a1-1-c2-gpu-e2bcabbf24b4`;
- C3a: `ray-a1-2-c3a-gpu-7770ca36a4d6`;
- C3b: `ray-a1-2-c3b-gpu-31b1f3597732`;
- C3c.0: `ray-a1-2-c3c0-gpu-f62122d52946`; and
- C3c.1: `ray-a1-2-c3c1-gpu-801168e03a47`.

The A1 unit discovery passes 35/35 tests. `nrtdsm`, `tfdm`, and `ray_a1_math_probe` build in Release, and `git diff --check` reports no patch errors.

C3 is complete as an isolated correctness port. It provides conservative denominator, proxy, UV-turn, ray-range, and optional ray-time-turn brackets; it does not yet segment a curve, construct a chord tube, traverse a hierarchy, solve a leaf, render, or establish speed. A1.3a below is now frozen and authorized for implementation. `t` turns remain an off-by-default optional event family.

## 17. Frozen A1.3 chord-certificate and lazy-split subplan

### 17.1 Scope and staging

A1.3 is the first isolated device component that constructs part of the proposed method rather than only porting event algebra. It is deliberately split into two gates:

1. **A1.3a / `C8-CHORD-CERTIFICATE`:** construct a conservative componentwise chord tube for one already-regular height interval; and
2. **A1.3b / `C9-LAZY-SPLIT`:** replay node-local interval clipping, certificate reconstruction, and midpoint splitting using only a passing C8 primitive.

C8 does not inspect hierarchy nodes or emit knots. C9 does not enumerate grid cells. Tube-supercover traversal remains A1.4 and is unauthorized until both gates pass. The rejected first-order directional segmentation predicate is not part of either default path: P-D1.2 showed no material knot or work reduction relative to the componentwise certificate.

### 17.2 C8 mathematical contract

For one coordinate

$$
q(h)=\frac{N(h)}{D(h)},\qquad
N=n_0+n_1h+n_2h^2,\quad D=d_0+d_1h+d_2h^2,
$$

define

$$
W=N'D-ND'=w_0+w_1h+w_2h^2
$$

with

$$
\begin{aligned}
w_0&=n_1d_0-n_0d_1,\\
w_1&=2(n_2d_0-n_0d_2),\\
w_2&=n_2d_1-n_1d_2.
\end{aligned}
$$

Then

$$
q''(h)=\frac{S(h)}{D(h)^3},\qquad S=W'D-2WD'=s_0+s_1h+s_2h^2+s_3h^3,
$$

where the device uses the simplified coefficients

$$
\begin{aligned}
s_0&=w_1d_0-2w_0d_1,\\
s_1&=2w_2d_0-w_1d_1-4w_0d_2,\\
s_2&=-3w_1d_2,\\
s_3&=-2w_2d_2.
\end{aligned}
$$

This avoids the dependency inflation produced by separately expanding `(N''D-ND'')D-2(N'D-ND')D'` as a nominal quartic.

Over a closed interval $I=[h_0,h_1]$, directed interval arithmetic and affine-composed Bernstein coefficients must enclose `D(I)` and `S(I)`. If `D(I)` contains zero, C8 returns `DENOMINATOR_UNRESOLVED`; it must not emit a finite certificate. Otherwise let

$$
m_D=\min_{h\in I}|D(h)|,\qquad M_S=\max_{h\in I}|S(h)|.
$$

The analytic secant-deviation radius is

$$
\epsilon_{\mathrm{analytic}}
=\frac{(h_1-h_0)^2}{8}\frac{M_S}{m_D^3}.
$$

C8 evaluates each rational endpoint as an outward interval, selects a representable binary32 chord endpoint inside that interval, and adds the maximum endpoint-center enclosure radius to `epsilon_analytic`. Thus the stored radius certifies deviation from the actual binary32 chord used downstream, not from an ideal exact-endpoint chord. The final radius is rounded outward. The construction is applied independently to `U/D` and `V/D`, producing `(epsilon_u, epsilon_v)` and four endpoint coordinates.

All coefficient algebra, Bernstein conversion, rational endpoint division, absolute maxima, denominator separation, cube, and final division use directed interval operations. Nonfinite arithmetic, invalid interval order, or an unrepresentable bound returns an explicit unresolved status. No center estimate may define a claimed lower or upper bound.

### 17.3 C8 vector corpus and output

Each packed C8 record contains a stable interval index, parent C1 case index, binary32 `U`, `V`, and `D` coefficient centers plus their already-validated outward coefficient intervals, and binary32 `(h0,h1)`. The device recomputes `W`, `S`, Bernstein ranges, endpoints, and radii; host-provided second-derivative or range values are forbidden.

The practical corpus uses all C1 parents and the five frozen dyadic intervals

`[0,1]`, `[0,0.5]`, `[0.5,1]`, `[0.25,0.5]`, and `[0.5,0.75]`.

Add analytic records for an affine coordinate, strong rational bowing, a tiny noncollapsed interval, a denominator separated but close to zero, a denominator root inside the interval, an endpoint close to a singularity, and signed-zero coefficients.

Each output repeats the interval and parent indices and records status, outward `D` and `S_u/S_v` ranges, endpoint intervals and chosen binary32 chord endpoints, endpoint-inflation terms, and final radii. The content hash and record ordering follow the existing A1 manifest contract.

### 17.4 C8 independent gate

C8 passes only if:

1. status and denominator-separation classification agree with an independent Decimal reference;
2. every device denominator and second-numerator range encloses high-precision evaluations over the closed interval;
3. endpoint intervals contain the 80-digit Decimal rational endpoints;
4. for at least 257 high-precision samples per ordinary interval, the exact curve deviation from the stored binary32 chord is at most the stored componentwise radius;
5. the device radius also encloses an independent exact-arithmetic CPU certificate for the binary32 center curve; the older P-D1 binary64 certificate's blanket numerical pad is recorded only as a diagnostic because its pad is amplified by $1/m_D^3$ and can become arbitrarily loose even when an exactly affine small-denominator coordinate has zero true curvature;
6. affine structural cases have only zero or endpoint-rounding inflation;
7. denominator-root and deliberately unresolved records never report a finite certificate; and
8. all C1-C3 current-binary regressions and vector schema tests still pass.

No timing result from C8 is paper evidence. A failure stops at the range, endpoint, or radius operation that first under-encloses; downstream widening is not an acceptable repair.

### 17.5 C9 node-local lazy split policy

Only after C8 passes, freeze and implement C9 as a separate device replay. Its default input is a regular event interval, one hierarchy-node UV rectangle, and one C8 certificate. It performs this policy:

1. intersect the certified chord tube with the closed node rectangle;
2. if that clips the height interval, reconstruct a C8 certificate over the clipped interval;
3. compute the componentwise normalized tube ratio

$$
\rho=\max\left(\frac{\epsilon_u}{\Delta u_{\mathrm{node}}},
                 \frac{\epsilon_v}{\Delta v_{\mathrm{node}}}\right);
$$

4. accept when `rho <= eta`; otherwise split at the representable midpoint and recurse; and
5. use `eta=0.5` for the central differential replay, retaining `0.25` and `1.0` only for later A2 sensitivity tests.

There is no fixed segment cap. If the midpoint is not distinct, C9 returns one conservative unsplit tube and increments a precision-floor counter. If the bounded device work queue overflows, it returns an explicit conservative fallback for the entire unresolved parent interval; it never returns a partial knot list. Mandatory denominator/proxy/ray-range partitions remain inputs from C3. Optional UV and `t` turns stay disabled by default and are later ablations, not correctness requirements.

### 17.6 C9 gate and handoff

C9 must reproduce the CPU componentwise accept/split classification and accepted interval cover on persisted P-D1 rays, with zero uncovered height interval, zero lost mandatory boundary, and explicit agreement on precision-floor and overflow statuses. Its metrics are accepted chords per interval, split depth, certificate reconstructions, fallback count, and p50/p95/p99/max work. They are correctness and workload diagnostics only.

Passing C9 authorizes writing the A1.4 closed tube-supercover plan. It does not authorize an optimized incremental DDA, OptiX integration, rendering, GPU speed claims, or modification of historical Mode 1.

## 18. A1.3a `C8-CHORD-CERTIFICATE` execution record — 2026-08-25

### 18.1 Authoritative artifacts

- vectors: `ray-a1-3-c8-vectors-4c520e2b5dd7`;
- device differential: `ray-a1-3-c8-gpu-10e6d0e5e672`;
- probe primitive: `C8-CHORD-CERTIFICATE`, primitive ID `8`; and
- vector corpus: 4,357 intervals: five fixed dyadic intervals for each of 870 C1 parents plus seven analytic cases.

The device consumes outward binary32 `U`, `V`, and `D` coefficient intervals. It constructs the simplified cubic rational second-derivative numerator, affine-composed Bernstein ranges, outward rational endpoint intervals, a stored binary32 chord, endpoint-center inflation, and the final componentwise radius. It reports no finite tube if the denominator range contains zero.

### 18.2 Differential result

| C8 check | Result |
|---|---:|
| Records | `4,357` |
| Finite certified tubes | `4,350` |
| Denominator refusals | `7` |
| Arithmetic-unresolved fallbacks | `0` |
| Range or endpoint under-enclosures | `0` |
| 257-sample Decimal tube escapes | `0` |
| Exact-center CPU certificate omissions | `0` |
| Maximum sampled-error / stored-radius ratio | `0.895081` |
| Maximum device / exact-reference analytic-radius ratio | `1.000019` |
| **C8 gate** | **pass** |

Six refusals are expected C1 analytic axis/near-axis parent intervals whose exported denominator uncertainty spans zero; the seventh is the constructed interior-denominator-root case. The affine and signed-zero structural cases retain zero analytic curvature radius, and the endpoint inflation accounts only for the actual stored chord endpoints.

### 18.3 Corrected oracle policy

The first attempted artifact pair—`ray-a1-3-c8-vectors-2237a8e7e4c3` and `ray-a1-3-c8-gpu-053904603c00`—is preserved but non-authoritative. Its geometric sample checks all passed. Its gate incorrectly required the device radius to exceed the legacy P-D1 binary64 certificate's blanket numerical pad. Because that pad is inserted into the second-numerator range and later divided by $m_D^3$, it produces a radius around `1.42e4` for an exactly constant numerator and denominator near `1e-6`, despite the rational coordinate being exactly affine and having zero curvature.

The corrected authoritative gate compares against an 80-digit exact-arithmetic certificate for the binary32 center curve and records the legacy padded number only as a diagnostic. This is a gate correction, not a relaxation of geometry: every interval range and endpoint remains outward-enclosed and every stored tube still contains the independent Decimal curve samples.

### 18.4 Current-binary regression

Against the C8-capable executable, all earlier gates pass:

- C1: `ray-a1-1-c1-ec7844cfda5a`;
- C2: `ray-a1-1-c2-gpu-25bc463d554c`;
- C3a: `ray-a1-2-c3a-gpu-4c770eab6f5c`;
- C3b: `ray-a1-2-c3b-gpu-ac262f25d31c`;
- C3c.0: `ray-a1-2-c3c0-gpu-722db341c843`;
- C3c.1: `ray-a1-2-c3c1-gpu-606b2f7ab213`; and
- C3c.2: `ray-a1-2-c3c2-gpu-40885b99d0f8`.

The A1 unit discovery passes 39/39 tests. `nrtdsm`, `tfdm`, and `ray_a1_math_probe` build in Release. `git diff --check` reports no patch errors; only pre-existing line-ending warnings remain.

### 18.5 Interpretation and next stop

C8 proves that the GPU can conservatively replace one regular rational shell-ray arc by a componentwise tube around a stored binary32 chord. This is the first passing device primitive that directly belongs to the proposed traversal. It does not yet show that the lazy policy chooses a complete interval cover, how many knots are emitted, which texels are visited, or any speed benefit.

The detailed C9 contract is frozen below. C9 is now authorized for isolated implementation. A1.4 DDA remains unauthorized.

## 19. Frozen A1.3b `C9-LAZY-SPLIT` device contract

### 19.1 Isolated question

C9 answers only this question: given one denominator-regular rational curve interval and one closed hierarchy-node UV rectangle, can the GPU lazily clip, recertify, and subdivide that interval into a conservative set of componentwise chord tubes satisfying the node-relative tolerance?

It does not descend to child nodes, evaluate a first-order residual slab, enumerate texels, solve displacement roots, or order hits. Removing those operations is intentional: P-D1 rejected a distinct first-order/directional segmentation advantage, so C9 validates the reusable curve-side mechanism without attributing unrelated hierarchy behavior to it.

### 19.2 Packed input and default corpus

Each input record stores:

- stable `task_index` and parent C1 case index;
- binary32 parent interval `(h0,h1)`;
- the same binary32 `U`, `V`, and `D` centers and outward intervals accepted by C8;
- one strictly positive closed node rectangle `(u_min,v_min,u_max,v_max)`; and
- binary32 `eta`.

For every C1 parent, select the full `[0,1]` C8 interval when denominator-regular, otherwise the first regular frozen dyadic interval. Construct four deterministic node tasks:

1. the root `[0,1]^2`;
2. the quadtree level-two cell of width `1/4` containing the exact center-curve midpoint;
3. the `16 x 16` leaf cell containing that midpoint; and
4. the farthest `16 x 16` corner cell, exercising rejection or partial overlap.

This gives 3,480 practical node tasks with root, coarse, fine, and negative/far-node strata while keeping the output corpus compact. Add analytic tasks for strong bowing, a constant chord component, exact edge/corner overlap, no overlap, adjacent-binary32 precision floor, forced queue overflow, and invalid node extent. Practical tasks use `eta=0.5`; alternative `eta` values exist only in analytic stress records and later A2 sweeps.

### 19.3 Closed clipping and recertification

For every queued interval C9 invokes the already passing C8 math locally. It intersects the stored chord with the node rectangle expanded componentwise by `(epsilon_u,epsilon_v)`. Slab divisions and the mapping from chord parameter back to height use directed binary32 bounds. Parallel chord components are handled as exact closed-set membership tests; edge and corner contact are retained.

If the overlap is empty, the queued interval is rejected. If clipping changes either height endpoint, C9 reconstructs a C8 certificate on the outward clipped height interval. It then computes

$$
\rho^+=\max\left(
\frac{\epsilon_u^+}{(u_{\max}-u_{\min})^-},
\frac{\epsilon_v^+}{(v_{\max}-v_{\min})^-}
\right).
$$

The segment is accepted only when `rho+ <= eta`. Otherwise it is split at the representable binary32 midpoint and the two closed children are queued. Shared midpoint ownership is deliberate; later closed tube-supercover logic must retain both owners.

### 19.4 Output and bounded fallback

The correctness probe uses a bounded LIFO queue and accepted-segment array of 32 entries per record. This is not a production segment cap. Each accepted entry stores `(h0,h1)`, the two binary32 chord endpoints, and `(epsilon_u,epsilon_v)`—eight floats total. Output also stores:

- status: regular, no-overlap, conservative-overflow, or certificate-unresolved;
- accepted count, split count, C8 reconstruction count, precision-floor count, and maximum depth; and
- the original parent interval for any fallback.

If the queue or accepted array would overflow, the output discards every partial segment and returns one explicit fallback for the entire original parent interval. If a midpoint is not distinct, C9 accepts the current conservative tube, increments `precision_floor`, and never drops the interval. A denominator or arithmetic refusal from C8 returns `certificate-unresolved` for the full parent. Invalid node extents fail closed the same way.

### 19.5 Independent gate

Binary32 outward arithmetic can legitimately add splits relative to a binary64 CPU policy, so exact knot identity is a diagnostic rather than a correctness requirement. C9 passes only if:

1. indices, statuses, counts, segment layout, sorted order, and closed parent bounds are valid;
2. every regular accepted segment independently passes the C8 range, endpoint, and 257-sample Decimal tube audit;
3. every non-precision-floor accepted segment satisfies the outward `rho+ <= eta` predicate;
4. at least 1,025 Decimal samples of the exact center curve that lie in the node rectangle are covered by an accepted segment, or by the explicit whole-parent fallback;
5. no-overlap results omit no audited exact curve point in the node;
6. exact edge/corner contacts are retained;
7. the analytic precision-floor record returns a conservative unsplit tube;
8. the analytic overflow record returns the untouched whole-parent fallback with zero partial output;
9. unresolved/invalid inputs never emit a finite accepted segment; and
10. all C1-C8 current-binary regressions pass.

Report accepted chords, split depth, C8 reconstructions, precision floors, and fallback counts with p50/p95/p99/max diagnostics. These are mechanism counts, not timing or paper-performance evidence.

Passing C9 completes A1.3 and authorizes planning A1.4 closed tube-supercover enumeration. It still does not authorize an optimized DDA or a performance conclusion.

## 20. A1.3b `C9-LAZY-SPLIT` execution record — 2026-08-25

### 20.1 Authoritative artifacts

- vectors: `ray-a1-3-c9-vectors-206576f83ce5`;
- device differential: `ray-a1-3-c9-gpu-11c86b8fd3ac`; and
- corpus: 3,488 tasks—four node strata for each of 870 C1 parents plus eight analytic stress tasks.

The device recursively invokes C8, clips the chord tube to a closed node rectangle, recertifies changed height intervals, and splits only when the outward componentwise radius exceeds `eta=0.5` times the node extent. Its 32-entry correctness buffer is fail-closed: pressure discards partial output and returns the original parent fallback.

### 20.2 Differential and mechanism result

| C9 check | Result |
|---|---:|
| Records | `3,488` |
| Regular outputs | `2,614` |
| Certified no-overlap outputs | `871` |
| Forced whole-parent overflow fallbacks | `1` |
| Explicit unresolved inputs | `2` |
| Node-curve coverage omissions | `0` |
| Accepted-tube Decimal escapes | `0` |
| False no-overlap decisions | `0` |
| Partial segments leaked on overflow/unresolved | `0` |
| Maximum sampled-error / stored-radius ratio | `0.893945` |
| **C9 gate** | **pass** |

Mechanism distributions:

| Metric per task | p50 | p95 | p99 | max |
|---|---:|---:|---:|---:|
| Accepted chords | `1` | `1` | `1` | `4` |
| Split events | `0` | `0` | `1` | `54` |
| C8 reconstructions | `1` | `2` | `5` | `98` |
| Maximum split depth | `0` | `0` | `1` | `26` |

The extreme split/reconstruction/depth values belong to the forced-overflow analytic record, not the practical corpus. The adjacent-binary32 analytic task produces the intended precision-floor acceptance. Exact edge/corner contact is retained. The two unresolved records are the invalid zero-width node and the deliberate denominator-crossing input. There is no practical whole-parent fallback in this corpus.

These counts support a narrow implementation conclusion: node-local lazy segmentation is usually cheap for the tested C1 curves and node rectangles, with 99% of tasks using at most one accepted chord and at most one split. They are not runtime evidence because the probe excludes hierarchy descent, cell enumeration, leaves, OptiX, and timing.

### 20.3 Current-binary regression and handoff

Against the C9-capable executable, all earlier device gates pass:

- C1: `ray-a1-1-c1-d5a843d6ac18`;
- C2: `ray-a1-1-c2-gpu-1f4a1d105bb3`;
- C3a: `ray-a1-2-c3a-gpu-4b8e5fcf26a8`;
- C3b: `ray-a1-2-c3b-gpu-eb0449dd4e6b`;
- C3c.0: `ray-a1-2-c3c0-gpu-c1cadabbb028`;
- C3c.1: `ray-a1-2-c3c1-gpu-a9d19fa8f02b`;
- C3c.2: `ray-a1-2-c3c2-gpu-ebec03072b7f`; and
- C8: `ray-a1-3-c8-gpu-12952a01b943`.

The A1 unit discovery passes 43/43 tests. `nrtdsm`, `tfdm`, and `ray_a1_math_probe` build in Release. `git diff --check` reports no patch errors; only pre-existing line-ending warnings remain.

A1.3 is complete as isolated device mathematics. The next action is to freeze the A1.4 closed tube-supercover enumeration contract and corpus. Do not implement the optimized incremental DDA yet: the first A1.4 gate must be a simple closed-set enumerator whose candidate cells and parameter intervals can be audited independently.

## 21. Frozen A1.4 `C10-CLOSED-SUPERCOVER` contract

### 21.1 Why the first GPU enumerator is deliberately simple

C10 is a correctness bridge, not the final fast DDA. One GPU thread receives one certified tube and exhaustively tests the proxy cells at one grid level using a closed directed slab clip. This control flow is independent of both the exact-event P-D1.4 oracle and the recursive quadtree oracle, which makes it a useful third differential implementation. Only after this cell/interval contract passes may a later optimized incremental DDA replace the exhaustive loop at identical outputs.

No timing or cell-test count from C10 is paper performance evidence.

### 21.2 Input and exact reference

Primitive `C10-CLOSED-SUPERCOVER` stores:

- stable tube index, source C9 task/segment indices, and grid level;
- binary32 chord endpoints `(u0,v0)` and `(u1,v1)`;
- outward binary32 radii `(epsilon_u,epsilon_v)`; and
- a tube-kind/reversal identifier for analytic checks.

Test levels `0..6`, corresponding to resolutions `1..64`. The practical corpus consumes every regular accepted C9 tube, deduplicates identical endpoint/radius bit patterns, and tests it at all seven levels. Add binary32 analytic tubes for horizontal, vertical, diagonal, stationary, exact grid-edge, exact grid-corner, proxy-edge, partially out-of-domain, reversed, and radius narrower than/equal to/wider than one cell cases. Include an adjacent-event case and one deliberately over-capacity large-radius tube.

For every stored binary32 input, the independent host oracle converts endpoints, radii, and dyadic grid boundaries to exact rational numbers. It exhaustively solves the two closed affine slab inequalities per cell, intersects with `[0,1]`, retains degenerate point intervals, applies the closed proxy rule `ix+iy <= resolution`, and outward-converts exact interval endpoints to binary32. The authoritative comparison is against this exact binary32-input oracle, not the older binary64 persisted endpoints.

### 21.3 Device enumeration and output

The C10 correctness kernel iterates cells in deterministic Morton order. For each proxy cell it clips the stored chord against

$$
[i/n-\epsilon_u,(i+1)/n+\epsilon_u]
\times
[j/n-\epsilon_v,(j+1)/n+\epsilon_v]
$$

using sign-aware directed binary32 subtraction and division. A zero chord component uses exact closed slab membership. A cell is emitted whenever the outward entry bound is no greater than the outward exit bound; equality is retained.

Each candidate stores integer `(ix,iy)` and outward normalized chord-parameter interval `(s_enter,s_exit)`. The correctness record has capacity 512. This is not a production candidate cap: if another candidate would exceed capacity, C10 erases all partial candidates and returns `CONSERVATIVE_OVERFLOW` for the entire grid/tube. Invalid/nonfinite inputs return `UNRESOLVED` with no partial output. The ordinary status with zero candidates is distinct from either fallback.

### 21.4 Differential gate

C10 passes only if:

1. indices, level, status, count, Morton ordering, and unused output storage are valid;
2. every exact-oracle candidate is emitted unless the record returns the explicit whole-tube overflow fallback;
3. every emitted interval contains the exact rational cell interval after outward binary32 conversion;
4. all extra cells from directed conservatism are counted, with exact equality required for the explicit analytic edge/corner/parallel/stationary cases;
5. point-only owners at grid edges, grid corners, and the proxy edge are retained;
6. reversing a tube preserves its candidate set and maps each exact interval to `[1-s_exit,1-s_enter]`;
7. the over-capacity analytic record has zero partial candidates and an intact whole-tube fallback;
8. invalid inputs emit no candidate; and
9. every C1-C9 current-binary regression passes.

Report candidates and tube-added cells over the zero-radius centerline with p50/p95/p99/max, point-only counts, extra-cell counts, and fallback counts. These are correctness/mechanism diagnostics only.

Passing C10 authorizes A1.5 exact cubic leaf planning. It also freezes the cell-ownership target for the later optimized GPU DDA, but it does not by itself authorize that optimization or a speed claim.

## 22. A1.4 `C10-CLOSED-SUPERCOVER` execution record — 2026-08-25

### 22.1 Authoritative artifacts

- vectors: `ray-a1-4-c10-vectors-a0409a86e549`;
- device differential: `ray-a1-4-c10-gpu-8c1161f09624`;
- practical source: 2,050 unique accepted C9 tubes tested at every level `0..6`; and
- total corpus: 14,367 records, including 17 analytic/reversal/overflow records.

The device primitive independently loops over proxy cells in Morton order and performs a directed closed slab clip. The host oracle uses exact rational arithmetic on the stored binary32 endpoints, radii, and dyadic cell boundaries. Neither side uses the P-D1 event enumerator or recursive hierarchy code.

### 22.2 Differential result

| C10 check | Result |
|---|---:|
| Records | `14,367` |
| Regular candidate records | `14,365` |
| Whole-tube overflow fallbacks | `1` |
| Invalid/unresolved records | `1` |
| Missing exact cells | `0` |
| Extra conservative cells | `0` |
| Missing point-only owners | `0` |
| Inward interval endpoints | `0` |
| Analytic ownership-set disagreements | `0` |
| Reversal disagreements | `0` |
| Partial candidates leaked on fallback | `0` |
| **C10 gate** | **pass** |

Mechanism distributions across regular records:

| Metric | p50 | p95 | p99 | max |
|---|---:|---:|---:|---:|
| Candidate cells | `3` | `24` | `59` | `197` |
| Tube-added cells over centerline | `0` | `8` | `34.36` | `158` |

The nonzero tube tail is important: a centerline-only DDA would omit cells in this corpus. The counts still are not runtime evidence because C10 deliberately tests every proxy cell rather than executing the proposed incremental traversal.

### 22.3 Oracle implementation notes

The first exact-vector build exceeded the command-time limit because it reconstructed identical rational tube values inside every cell test. The optimized exporter precomputes tube rationals and restricts the exhaustive loop to the exact swept bounding rectangle; it does not change the closed cell predicate or corpus.

The first device comparison reported two reverse-interval failures while all exact-cell and outward-interval checks passed. The checker had applied ordinary `1-x` subtraction to an already outward-rounded device endpoint, manufacturing a gap. The authoritative checker verifies reversal using the exact rational forward and reverse intervals, while independently requiring each device interval to enclose its exact oracle. No device result or geometric tolerance was weakened.

### 22.4 Current-binary regression and handoff

Against the C10-capable executable, all earlier gates pass:

- C1: `ray-a1-1-c1-d01ee22ead4b`;
- C2: `ray-a1-1-c2-gpu-72ac3fdf5e7e`;
- C3a: `ray-a1-2-c3a-gpu-8de5ad685f77`;
- C3b: `ray-a1-2-c3b-gpu-9d5661241425`;
- C3c.0: `ray-a1-2-c3c0-gpu-b66dce38765c`;
- C3c.1: `ray-a1-2-c3c1-gpu-9fc20f98f5aa`;
- C3c.2: `ray-a1-2-c3c2-gpu-ddee35e8a016`;
- C8: `ray-a1-3-c8-gpu-0f47887eaf63`; and
- C9: `ray-a1-3-c9-gpu-39a813a5430c`.

The A1 unit discovery passes 48/48 tests. `nrtdsm`, `tfdm`, and `ray_a1_math_probe` build in Release. `git diff --check` reports no patch errors; only pre-existing line-ending warnings remain.

C10 freezes the exact cell-ownership and parameter-interval target for the eventual optimized DDA. The staged A1.5 contract is frozen below. Only its C11 polynomial-construction gate is authorized for implementation; root isolation and ownership remain blocked on C11.

## 23. Frozen staged A1.5 represented-leaf contract

### 23.1 Frozen represented equation and source corpus

For one fixed-diagonal displacement microtriangle with affine height

$$
h(u,v)=c+g_u u+g_v v,
$$

the represented intersection equation for the rational shell ray is

$$
G(h)=hD(h)-cD(h)-g_uU(h)-g_vV(h)=0.
$$

This is the actual triangle-proxy shell with piecewise-affine displacement. A planar world-space microtriangle intersection is not an allowed substitute.

The practical vector producer consumes the final corrected P-D1 chain:

- root intervals: `p-d1-6-e254f252e010`;
- level-six event candidates: `p-d1-4-dd1251769c51`;
- rays/hierarchies: `p-d1-2-ec0dd6a036f5`; and
- A1 coefficient parents: `ray-a1-0-vectors-d5637078cdef`.

The 864 root cases admit 6,431 level-six cells and therefore 12,862 fixed-diagonal microtriangle tasks before analytic additions. The exporter reconstructs the practical height windows, maps each tube ID to its corrected `(h0,h1)`, maps each ray ID to its A1 parent, outward-maps every normalized candidate interval to shell height, and quantizes UV-height vertices to binary32 before constructing the reference. All source and window hashes are persisted.

### 23.2 A1.5a / `C11-LEAF-POLYNOMIAL`

C11 isolates only plane and polynomial construction. Each record stores:

- stable leaf-task, parent C1, cell, and local-triangle indices;
- three binary32 `(u,v,height)` vertices;
- binary32 candidate height bounds;
- binary32 `U`, `V`, and `D` centers plus their outward coefficient intervals; and
- source tube/window hashes.

From the vertices, the device constructs `c`, `g_u`, and `g_v` using a directed interval determinant/inverse. It then constructs the four ascending coefficients

$$
\begin{aligned}
G_0&=-g_uU_0-g_vV_0-cD_0,\\
G_1&=D_0-g_uU_1-g_vV_1-cD_1,\\
G_2&=D_1-g_uU_2-g_vV_2-cD_2,\\
G_3&=D_2.
\end{aligned}
$$

The output contains outward plane and `G` coefficient intervals plus binary32 center values computed by a separately declared center path. A zero/uncertain UV determinant, invalid height interval, or nonfinite arithmetic returns `PLANE_UNRESOLVED` and no polynomial.

C11 analytic cases include constant height, pure `u`/`v` slopes, both diagonal conventions, signed zero, tiny but nonzero UV area, zero UV area, cancellation in every `G` coefficient, and an identically zero polynomial.

**C11 gate:** every device plane and polynomial interval encloses an independent 80-digit expression over the exact packed binary32 inputs; every center value satisfies the condition-aware reference enclosure; fixed-grid structural values remain exact where algebraically required; zero-area input fails closed; all 12,862 practical tasks are present; and C1-C10 regressions pass. C11 does not solve a root or report a hit.

### 23.3 A1.5b / `C12-LEAF-ROOT-BRACKETS`

C12 is planned but not authorized until C11 passes. It will consume C11 polynomials and isolate every closed real-root component over the candidate height interval. The first correctness implementation uses outward interval Bernstein/subdivision with explicit `IDENTICALLY_ZERO`, `FULL_INTERVAL_UNRESOLVED`, and bounded-work whole-leaf fallback states. It may use a deliberately expensive certification path, but it may not silently filter complex roots or stop merely because `|G|` is small.

The C12 corpus must include all ordinary C11 polynomials plus constant, linear, quadratic, cubic, one/two/three-root, repeated, clustered, endpoint, no-root, scale-extreme, and identically-zero analytic polynomials. Every 80-digit Decimal root must lie in a device bracket; bracket merging cannot lose multiplicity-adjacent components; and resource pressure returns the complete leaf interval rather than a partial root list. C12 mechanism counts are not runtime evidence.

### 23.4 A1.5c / leaf refinement, filtering, and ownership

C13 is intentionally not frozen yet. After C12 passes, write a separate contract for safeguarded representative refinement and evaluation of `(a,b,u,v,h,t)`, closed microtriangle/proxy/ray ownership, shared-edge owner grouping, closest-hit identity, scale-aware polynomial/height/world residuals, and coplanar/denominator/numerical fallbacks. C13 must carry enough original proxy/ray geometry to audit the world equation; accepting a polynomial root alone is insufficient.

Only a passing C11 authorizes the detailed C12 implementation plan. Only a passing C12 authorizes freezing C13. The optimized DDA and performance timing remain outside A1.5.

## 24. A1.5a `C11-LEAF-POLYNOMIAL` execution record — 2026-08-25

### 24.1 Authoritative artifacts and corpus

- vectors: `ray-a1-5-c11-vectors-f38cfe49e64d`;
- device differential: `ray-a1-5-c11-gpu-12efa871d412`;
- practical source: all 6,431 level-six P-D1.4 candidate cells, with both fixed-diagonal microtriangles per cell; and
- total corpus: 12,873 records = 12,862 practical leaf equations + 11 analytic records.

The host exporter independently reconstructs the 48 verified practical displacement windows, joins each candidate tube to the corrected P-D1.6 height interval and A1 C1 coefficient parent, outward-maps the candidate interval to height, and then quantizes every input to binary32. The reference evaluates exact rational expressions over those packed bits and persists 80-digit decimal diagnostics. It does not call the CUDA plane/polynomial implementation.

### 24.2 Differential result

| C11 check | Result |
|---|---:|
| Records | `12,873` |
| Regular outputs | `12,871` |
| Explicit plane-unresolved outputs | `2` |
| Exact interval enclosure failures | `0` |
| Condition-aware center failures | `0` |
| Centers outside device intervals | `0` |
| Practical determinant disagreements | `0` |
| Analytic-case failures | `0` |
| Partial/nonzero data on unresolved output | `0` |
| Maximum normalized center error | `0.000811625` |
| **C11 gate** | **pass** |

The two unresolved cases are intentional: a zero-area UV triangle and an invalid reversed height interval. Constant, pure-`u`, pure-`v`, both diagonal conventions, signed zero, tiny nonzero area, coefficient cancellation, and an identically-zero polynomial all pass. Every practical fixed-grid determinant is exactly `1/4096` in its center and interval output.

The widest observed directed coefficient intervals are `3.90769e-4`, `4.05788e-4`, `4.05312e-4`, and `1.52737e-5` for `G0..G3`, respectively. These widths are inputs to the next root-bracketing correctness gate; they are not measured geometric error or timing evidence.

### 24.3 Current-binary regression and handoff

Against the C11-capable executable, every earlier device gate passes:

- C1: `ray-a1-1-c1-04e254c7d0c0`;
- C2: `ray-a1-1-c2-gpu-c1fec339f3bb`;
- C3a: `ray-a1-2-c3a-gpu-9674d33c8934`;
- C3b: `ray-a1-2-c3b-gpu-ecf792fa862d`;
- C3c.0: `ray-a1-2-c3c0-gpu-0acd23531ab9`;
- C3c.1: `ray-a1-2-c3c1-gpu-3ea2363269dd`;
- C3c.2: `ray-a1-2-c3c2-gpu-a493cdfb6f28`;
- C8: `ray-a1-3-c8-gpu-7bb51d2b2539`;
- C9: `ray-a1-3-c9-gpu-4a9a49316a8f`; and
- C10: `ray-a1-4-c10-gpu-4e2f2cde3872`.

The A1 unit discovery passes 53/53 tests. `nrtdsm`, `tfdm`, and `ray_a1_math_probe` build in Release. `git diff --check` reports no patch errors; only pre-existing line-ending warnings remain. Historical Mode 1 is unchanged.

Passing C11 authorizes the C12 root-bracketing contract below. It does not validate any root, accepted hit, final ownership rule, incremental DDA, rendering, or speed.

## 25. Frozen A1.5b `C12-LEAF-ROOT-BRACKETS` contract

### 25.1 Inputs and exact target

C12 consumes each passing C11 record's binary32 candidate height interval, four outward `G` coefficient intervals, and four binary32 center coefficients. Its exact correctness target remains the center polynomial constructed as an exact real expression over the original packed C11 inputs; C11's wider coefficient intervals are conservative device operands, not permission to change the represented surface.

The practical corpus contains all 12,862 practical C11 equations. Add analytic constant, linear, quadratic, and cubic polynomials with no roots; one, two, and three distinct roots; repeated and triple roots; adjacent/clustered roots; roots at both interval endpoints; roots just outside the interval; large/small coefficient scales; a degenerate height interval; an identically-zero polynomial; invalid intervals; and a forced bounded-work fallback. Persist the C11 vector/output hashes and every analytic factorization.

### 25.2 Deliberately simple certified bracket implementation

This first C12 kernel is correctness scaffolding, not the final leaf solver. It maps the candidate height interval to normalized `x in [0,1]` and tests 4,096 closed dyadic bins. For each bin, directed interval arithmetic converts the power-basis coefficient intervals to a cubic Bernstein enclosure over that bin. By the Bernstein convex-hull property, a bin is rejected only when all four Bernstein coefficient intervals are strictly positive or all are strictly negative. Equality retains the bin.

Adjacent active bins are merged into closed normalized brackets. Endpoint-only bins and degenerate brackets are retained. At most 16 components are stored. If input validation fails, a coefficient interval is nonfinite/reversed, the declared work budget is exhausted, an interval operation becomes nonfinite, or another component would exceed capacity, the kernel erases all partial brackets and returns `FULL_INTERVAL_UNRESOLVED` with the single bracket `[0,1]`. A zero-width height interval is evaluated as one closed point. `IDENTICALLY_ZERO` is returned only when all four input coefficient intervals are exactly `[0,0]`, also with `[0,1]`.

The normal practical work budget is the complete 4,096-bin scan. A test-only smaller budget is stored in analytic records solely to exercise the bounded-work fallback. Counts from this scan are not timing evidence and the scan is not the production cubic solver.

### 25.3 Independent oracle and gate

For every ordinary record, an independent high-precision host oracle isolates all real roots of the exact center polynomial over the exact packed binary32 height interval, including repeated and endpoint roots. It also directly evaluates the exact polynomial at all critical points and bracket boundaries. Analytic roots additionally come from their persisted factorizations rather than the device algorithm.

C12 passes only if:

1. task/parent/cell/triangle identities and statuses are preserved;
2. every ordinary exact root maps into at least one emitted closed normalized bracket;
3. every regular rejected range is independently shown root-free by exact square-free Sturm counts (the device itself rejects a bin only with a directed Bernstein sign certificate);
4. endpoint, repeated, triple, clustered, and degenerate-interval analytics are retained;
5. `IDENTICALLY_ZERO` is used only for exact zero coefficient intervals;
6. invalid or bounded-work records return the complete `[0,1]` fallback with no partial brackets;
7. bracket ordering, merging, capacity, unused storage, and finite-range invariants hold;
8. every one of the 12,862 practical tasks is present; and
9. C1-C11 current-binary regressions pass.

Report regular/zero/fallback counts, active-bin and component p50/p95/p99/max, normalized bracket-width p50/p95/p99/max, exact roots per task, and analytic outcomes. These are correctness and mechanism diagnostics only.

Passing C12 authorizes freezing C13 refinement/filtering/world-equation/ownership. It still does not authorize the optimized DDA or performance timing.

## 26. A1.5b `C12-LEAF-ROOT-BRACKETS` execution record — 2026-08-25

### 26.1 Authoritative artifacts

- vectors: `ray-a1-5-c12-vectors-19042ea430b6`;
- device differential: `ray-a1-5-c12-gpu-4d30776c4197`;
- source C11 vectors: `ray-a1-5-c11-vectors-f38cfe49e64d`;
- source C11 device bytes: SHA-256 `62398ece01d221c26c880c9d9aaecf21fbd15cbf4cc645923695d7144e9e0fcc`; and
- total corpus: 12,887 records = all 12,862 practical C11 equations + 9 passing C11 analytics + 16 C12 root/fallback analytics.

The host oracle transforms the exact rational height polynomial to normalized `[0,1]`, removes repeated factors with an exact polynomial GCD, and isolates every distinct real root with a rational Sturm sequence. Endpoint roots are extracted before open-interval counts. No NumPy or vendor polynomial root solver is part of the correctness oracle.

### 26.2 Differential result

| C12 check | Result |
|---|---:|
| Records | `12,887` |
| Regular outputs | `12,880` |
| Exact identically-zero outputs | `5` |
| Full-interval fail-closed outputs | `2` |
| Exact-root omissions | `0` |
| Root-containing rejected ranges | `0` |
| Bracket layout failures | `0` |
| Analytic-case failures | `0` |
| **C12 gate** | **pass** |

All 12,862 practical tasks return regular status. Their mechanism distribution is:

| Practical metric | p50 | p95 | p99 | max |
|---|---:|---:|---:|---:|
| Active bins of 4,096 | `0` | `45` | `3446.88` | `4096` |
| Components | `0` | `1` | `1` | `1` |

Of the practical tasks, 10,029 reject every bin, 2,638 contain one exact root, and none contain two or three exact roots over the admitted interval. The other 195 tasks retain conservative bins without an exact root. Importantly, 128 practical tasks retain all 4,096 bins. Across the full corpus, normalized retained-bracket width is `0.0012207 / 0.233582 / 1 / 1` at p50/p95/p99/max.

The analytic suite retains constant/linear/quadratic/cubic cases, one/two/three distinct roots, repeated and triple roots, clustered roots, both endpoints, outside roots, large/small scales, degenerate intervals, exact zero, invalid height, and forced work exhaustion. The two full-interval outputs are exactly the invalid-height and forced-budget cases and contain no partial bracket data.

### 26.3 Interpretation and current-binary regression

C12 proves that the represented cubic roots can be retained conservatively on the GPU. It does **not** yet establish a practical leaf solver. The p99/full-domain tail shows that the generic coefficient enclosures inherited from C3a/C11 are too wide for efficient refinement in a small but important subset. That tail is now a formal optimization blocker rather than a correctness failure.

Against the C12-capable executable, all preceding gates pass:

- C1: `ray-a1-1-c1-ec7001106104`;
- C2: `ray-a1-1-c2-gpu-f0bd3280c700`;
- C3a: `ray-a1-2-c3a-gpu-a46fb7d0ad09`;
- C3b: `ray-a1-2-c3b-gpu-3ba1b54742d9`;
- C3c.0: `ray-a1-2-c3c0-gpu-80cf264f97ff`;
- C3c.1: `ray-a1-2-c3c1-gpu-3ef3e3028b5e`;
- C3c.2: `ray-a1-2-c3c2-gpu-4f4d5e21a08a`;
- C8: `ray-a1-3-c8-gpu-bb39e35d10d8`;
- C9: `ray-a1-3-c9-gpu-37c132960f7f`;
- C10: `ray-a1-4-c10-gpu-ea6543cef86a`; and
- C11: `ray-a1-5-c11-gpu-4750b2bb2cb4`.

The A1 unit discovery passes 58/58 tests. All three Release targets build, and `git diff --check` has no patch errors beyond pre-existing line-ending warnings.

## 27. Frozen A1.5c tight leaf-equation oracle before hit refinement

### 27.1 Why refinement is temporarily paused

The current C12 intervals are correct but inherit C3a's generic coefficient pad, whose `max(1, scale)` floor can be much larger than the actual error of small `U`, `V`, and `D` coefficients. A final root/hit stage built directly on this policy would spend bounded work on 128 full-domain practical leaves and a p99 of about 3,447 active bins. This is not an acceptable production starting point.

The next stage therefore isolates precision policy from root-solving policy. It does not change the represented surface, candidate cells, exact root oracle, or historical Mode 1.

### 27.2 C13.0 CPU precision-oracle experiment

For every practical C12 task, replay the same 4,096-bin Bernstein classification under three equation-enclosure policies:

1. **generic-current:** the passing C11/C12 intervals, reproduced byte-for-byte;
2. **exact-center oracle:** exact rational `G0..G3` singletons from the packed C1 and microtriangle inputs; this is an unattainable lower bound on conservatism, not a proposed implementation; and
3. **operation-local directed:** reconstruct `D`, `U`, and `V`, the leaf plane, and `G` directly from the raw packed C1 geometry/frame/texture inputs with the same declared operation graph as the CUDA implementation, rounding every primitive outward instead of applying a generic coefficient pad afterward.

Report active bins, bracket width, full-domain leaves, false-candidate leaves, and exact-root omissions per task and by shell/ray family/map window. Also attribute interval width to the plane versus `U/V/D` terms and persist the worst 128 cases.

**Go/no-go:** operation-local directed arithmetic must have zero exact-root omissions; eliminate the unexplained full-domain practical tail or return those cases through an explicit bounded fallback; materially close the gap to the exact-center oracle at p95/p99; and not make median/no-root rejection worse. Do not freeze thresholds by looking only at aggregate means. If it remains far from the oracle, test a separately reported binary64/FMA coefficient construction or root-local interval evaluation before writing CUDA.

### 27.3 Ordered handoff

Only a passing C13.0 oracle authorizes `C13.1-TIGHT-LEAF-EQUATION` on the GPU. That device gate must differential-test every directed intermediate, then rerun the unchanged C12 bracket classifier on the tighter intervals. Only after both gates pass may A1.5d freeze root representative refinement, rational `(a,b,u,v,t)` evaluation, microtriangle/proxy/ray filtering, world-equation residuals, shared-edge owner grouping, and closest-hit ordering.

The optimized DDA, integration into Mode 1, rendering, and performance timing remain blocked.

## 28. A1.5c C13.0 precision-oracle execution record — 2026-08-25

### 28.1 Authoritative artifacts and corrected truth contract

- six-policy oracle: `ray-a1-5-c13o-1fdc01e2f6e6`;
- selected hybrid report: `ray-a1-5-c13h-b22534d8ab57`; and
- corpus: all 12,862 practical C12 leaf tasks.

The authoritative geometric truth is now explicit in the artifact configuration: exact-real leaf-equation algebra over the raw packed C1 geometry/frame/texture values and packed microtriangle vertices. The oracle implementation SHA-256 is also part of the run configuration and therefore the run ID.

This corrects an integration ambiguity exposed by the experiment. C11/C12 used a self-consistent polynomial whose coefficient centers were host-rounded from the C1 binary64 reference and then packed to binary32. Those stages remain valid arithmetic and root-retention gates for their declared inputs, and their broad generic intervals also cover every raw-equation root in this corpus. They are not, however, the final geometric truth for comparing a direct binary64 coefficient construction. C13.0 recomputes the exact raw equation independently for every task.

An initial binary64 replay compared against the older C11 center polynomial and reported 75 apparent omissions. The integrity gate correctly failed. The cause was the truth-contract mismatch, not missing roots in the binary64 enclosure. The authoritative rerun hashes the corrected raw truth contract and has zero omissions under every reported policy.

### 28.2 Precision-policy result

| Policy | p95 active bins | p99 | max | full leaves | false-candidate leaves | total active bins | root omissions |
|---|---:|---:|---:|---:|---:|---:|---:|
| Generic current C11/C12 | `45` | `3446.88` | `4096` | `128` | `202` | `639,854` | `0` |
| Operation-local binary32 | `1` | `59.58` | `4096` | `123` | `190` | `508,146` | `0` |
| Binary64 coefficients + binary32 plane | `1` | `1` | `4096` | `7` | `28` | `45,769` | `0` |
| Binary32 coefficients + binary64 plane | `1` | `37.68` | `4096` | `123` | `190` | `507,533` | `0` |
| Full operation-local binary64 | `1` | `1` | `2` | `0` | `0` | `2,632` | `0` |
| Exact-center lower bound | `1` | `1` | `2` | `0` | `0` | `2,632` | `0` |

The ordinary operation-local binary32 policy substantially improves p95/p99 but fails the predeclared method gate because the exact grid-corner family still creates 123 full-domain leaves. The mixed policies localize the source: shell-ray `U/V/D` coefficient precision is the dominant issue. Computing those coefficients in binary64 once per ray/proxy eliminates nearly the entire tail; keeping only the leaf plane in binary32 leaves seven full-domain grid-corner tasks.

### 28.3 Selected hybrid policy

The persisted hybrid policy is:

1. construct shell-ray coefficients with directed binary64 arithmetic;
2. construct the ordinary leaf plane in directed binary32 and assemble/classify the equation using binary64;
3. if that base equation retains more than eight of 4,096 bins, recompute the plane and equation fully in binary64; and
4. otherwise keep the base result.

| Hybrid metric | Result |
|---|---:|
| Practical tasks | `12,862` |
| Full-precision leaf fallbacks | `11` (`0.0855%`) |
| Active bins p50/p95/p99/max | `0 / 1 / 1 / 2` |
| Total active bins | `2,661` |
| Exact-center lower-bound total | `2,632` |
| False-candidate leaves | `23` |
| Full-domain leaves | `0` |
| Raw exact-root omissions | `0` |
| **Hybrid CPU gate** | **pass** |

This is an algorithmic precision decision only. It does not show that FP64 is cost-effective on the target GPU. In particular, consumer-GPU FP64 throughput can make even amortized coefficient work expensive. A compensated/float-float binary32 construction remains a required comparison, not an optional polish item.

## 29. Frozen C13.1 GPU precision gate and cost decision

### 29.1 Correctness primitive

`C13.1-TIGHT-LEAF-EQUATION` will consume the raw 33 packed C1 inputs, the packed microtriangle vertices and height interval, and stable task/parent/cell/triangle identities. It must not consume host-rounded C11 coefficient centers as geometric truth. Implementation is split into two ordered gates so classifier behavior cannot mask an enclosure error.

`C13.1a-TIGHT-EQUATION-INTERVALS` first constructs directed binary64 `D/U/V` intervals from the exact declared C1 operation graph, constructs both the directed binary32 and directed binary64 leaf planes, and assembles both mixed and full-binary64 cubics. It outputs only these intermediate intervals and statuses. Every endpoint must match or conservatively enclose the independent exact/operation-level CPU reference; invalid determinants return zero partial data. No bin scan is allowed in C13.1a.

Only after C13.1a passes, `C13.1b-TIGHT-EQUATION-BRACKETS` runs the unchanged closed 4,096-bin Bernstein classifier on the mixed equation. If more than eight bins survive, it switches to the full-binary64 equation and reclassifies. Output includes base/final counts and brackets, fallback flag, and zeroed partial storage on any unresolved condition.

The independent host differential evaluates exact Fractions over the raw packed inputs, verifies every intermediate enclosure and raw exact root, checks the exact 11-task fallback set unless a more conservative device result is explained, and compares final active-bin/bracket metrics to `ray-a1-5-c13h-b22534d8ab57`. C1-C12 regressions must pass after each device gate. Only C13.1a is authorized for implementation now. This remains a correctness probe, not final traversal.

### 29.2 Mandatory precision-cost comparison after correctness

Only after C13.1 passes, measure isolated GPU costs for:

- current binary32/generic;
- operation-local directed binary32;
- selected hybrid FP64 coefficient + rare full-leaf fallback;
- full FP64; and
- compensated binary32 or float-float `D/U/V` construction with the same raw-root gate.

Time coefficient construction per ray/proxy separately from leaf-plane/equation work per admitted leaf. Report instructions, registers, occupancy, fallback incidence, and ns/item; do not infer full traversal speed from the 4,096-bin correctness scan. The GPU implementation proceeds to hit refinement only if a zero-omission policy remains cost-plausible. If native FP64 loses badly, the next implementation target is compensated binary32, not silent relaxation of the interval or ownership rules.

Historical Mode 1, final hit refinement, optimized DDA, rendering, and paper performance timing remain blocked.

## 30. C13.1a tight-equation interval execution record — 2026-08-25

### 30.1 Authoritative artifacts

- vectors: `ray-a1-5-c13a-vectors-17395a5c66c9`;
- device differential: `ray-a1-5-c13a-gpu-035ff319f8b9`;
- practical records: all 12,862 C12 leaf tasks; and
- analytic records: constant, both sloped diagonal conventions, signed zero, tiny nonzero area, and zero area.

Each ordinary record carries the raw 33 packed C1 inputs, nine packed microtriangle coordinates/heights, and the candidate height interval. The output contains 23 intervals / 46 double endpoints: binary64 `U/V/D`, promoted binary32 plane, mixed cubic, binary64 plane, and full-binary64 cubic. There is no bin classification in this primitive.

### 30.2 Differential result

| C13.1a check | Result |
|---|---:|
| Records | `12,868` |
| Regular | `12,867` |
| Explicit unresolved zero-area analytic | `1` |
| Nonzero endpoint disagreements | `0` |
| Raw exact mixed/full cubic enclosure failures | `0` |
| Layout/status/identity failures | `0` |
| Analytic failures | `0` |
| Signed-zero-only bit differences | `40,770` |
| Partial data on unresolved output | `0` |
| **C13.1a gate** | **pass** |

All 40,770 bit differences are a directed device lower endpoint of `-0.0` versus the host reference's `+0.0`. They are numerically identical and valid outward lower bounds; there are no nonzero endpoint differences. This case is retained as an explicit diagnostic rather than normalized away.

Maximum observed widths include `3.33e-16` for the largest `U/V/D` coefficient interval, `1.43e-6` for binary32 plane `c`, `1.36e-6` for mixed `G0`, and `8.73e-11` for full-binary64 `G0` (the latter maximum includes the tiny-area analytic stress case). These are construction diagnostics, not a timing or full-traversal result.

### 30.3 Current-binary regression

Against the C13.1a executable, all preceding gates pass:

- C1: `ray-a1-1-c1-feef916cc1d6`;
- C2: `ray-a1-1-c2-gpu-387c79494a33`;
- C3a: `ray-a1-2-c3a-gpu-8a054def280d`;
- C3b: `ray-a1-2-c3b-gpu-797ec6901b2d`;
- C3c.0: `ray-a1-2-c3c0-gpu-279b26481f5b`;
- C3c.1: `ray-a1-2-c3c1-gpu-4b5e099f6036`;
- C3c.2: `ray-a1-2-c3c2-gpu-c1b5ab838083`;
- C8: `ray-a1-3-c8-gpu-ecc5aa6f6755`;
- C9: `ray-a1-3-c9-gpu-61d5311ede72`;
- C10: `ray-a1-4-c10-gpu-04300b1dc530`;
- C11: `ray-a1-5-c11-gpu-942cefb901d7`; and
- C12: `ray-a1-5-c12-gpu-b285c36de360`.

The A1 unit discovery passes 65/65 tests. `nrtdsm`, `tfdm`, and `ray_a1_math_probe` build in Release. `git diff --check` reports no patch errors beyond pre-existing line-ending warnings. Mode 1 remains unchanged.

Passing C13.1a authorizes only C13.1b's adaptive bracket classifier under the contract in Section 29. It does not authorize hit refinement or performance claims.

## 31. C13.1b adaptive bracket execution record — 2026-08-25

### 31.1 Authoritative artifacts and contract

- vectors: `ray-a1-5-c13b-vectors-8fb8ded7423b`;
- device differential: `ray-a1-5-c13b-gpu-303fe2a691e8`;
- practical records: all 12,862 C13.0/C12 leaf tasks; and
- analytic records: the same six C13.1a plane cases, including the explicit zero-area unresolved case.

`C13.1b-TIGHT-EQUATION-BRACKETS` consumes the already-verified C13.1a mixed and full-binary64 cubic intervals. It does not reconstruct coefficients. It runs the closed 4,096-bin double-interval Bernstein classifier on the mixed equation and reclassifies with the full-binary64 equation exactly when the mixed scan admits more than eight bins. Both base and final brackets are emitted as merged closed dyadic intervals with capacity 16. A source-unresolved or arithmetic-unresolved record returns no partial bracket state.

### 31.2 Differential result

| C13.1b practical result | Value |
|---|---:|
| Records | `12,862` |
| Mixed-to-full fallbacks | `11` (`0.0855%`) |
| Final active bins, p50 / p95 / p99 / max | `0 / 1 / 1 / 2` |
| Total mixed active bins | `45,768` |
| Total final active bins | `2,660` |
| Zero-active final tasks | `10,208` |
| False-candidate final tasks | `23` |
| Full-domain final tasks | `0` |
| Raw exact-root omissions | `0` |
| Status/identity/layout/ownership failures | `0` |
| Analytic failures | `0` |
| Partial output on the zero-area unresolved case | `0` |
| **C13.1b gate** | **pass** |

The device fallback set is exactly the 11-task CPU hybrid set. One `grid-corner-hit` task (`12068`) is one bin tighter than the deliberately padded long-double CPU classifier: the CPU admits bins `[4047,4049)`, while the directed device scan admits `[4047,4048)`. The complete square-free/Sturm raw-equation reference proves the removed bin contains no root. All other base and final brackets equal the CPU policy artifact. Consequently, the device total is 2,660 rather than the CPU report's 2,661 active bins; this is a verified tightening, not an omission.

This closes bracket retention only. The 4,096-bin scan is correctness scaffolding and must not be timed or presented as the production root solver.

### 31.3 Current-binary regression

Against the C13.1b executable, every earlier device gate passes:

- C1: `ray-a1-1-c1-c1aa379b0983`;
- C2: `ray-a1-1-c2-gpu-16cda82d7628`;
- C3a: `ray-a1-2-c3a-gpu-45285f5f84c5`;
- C3b: `ray-a1-2-c3b-gpu-0b0f87104260`;
- C3c.0: `ray-a1-2-c3c0-gpu-c2c7436d1429`;
- C3c.1: `ray-a1-2-c3c1-gpu-eac1828ec8fb`;
- C3c.2: `ray-a1-2-c3c2-gpu-d12e0afedb38`;
- C8: `ray-a1-3-c8-gpu-fac510d97f46`;
- C9: `ray-a1-3-c9-gpu-728771465930`;
- C10: `ray-a1-4-c10-gpu-5a053866fded`;
- C11: `ray-a1-5-c11-gpu-0390687f81fa`;
- C12: `ray-a1-5-c12-gpu-610640b64215`; and
- C13.1a: `ray-a1-5-c13a-gpu-035e755e6a0a`.

The A1 unit discovery passes 69/69 tests. The targeted Release builds for `nrtdsm`, `tfdm`, and `ray_a1_math_probe` pass. Historical Mode 1 remains unchanged.

## 32. Frozen C13.2 precision-cost decision plan

### 32.1 Question and non-claims

C13.2 asks which zero-omission arithmetic policy is plausible for the later production leaf solver. It does **not** time the 4,096-bin classifier, predict full traversal speed from arithmetic alone, or authorize a paper performance claim.

The benchmark separates two amortization domains:

1. **ray/proxy setup:** construct shell-ray `U/V/D` data from the 33 packed proxy/ray inputs once per OptiX candidate; and
2. **admitted-leaf work:** construct the leaf plane and cubic from nine packed `(u,v,h)` vertices once per admitted microtriangle.

Combining these into one kernel would hide whether binary64 hurts the once-per-proxy or once-per-leaf path.

### 32.2 Ordered policies

The frozen comparison is:

| Policy | Ray/proxy setup | Leaf plane/equation | Correctness requirement |
|---|---|---|---|
| `F32-GENERIC` | existing binary32 interval path | existing C11 binary32 path | diagnostic baseline only |
| `F32-DIRECTED` | operation-local directed binary32 | directed binary32 | zero raw-root omissions or rejected |
| `F64-HYBRID` | directed binary64 `U/V/D` | binary32 plane/mixed equation; recorded rare full-binary64 fallback | C13.1b result |
| `F64-FULL` | directed binary64 `U/V/D` | directed binary64 plane/equation | zero raw-root omissions |
| `FF32` | compensated/float-float binary32 with certified error radius | matching certified plane/equation | must pass a new C13.2a exact-root gate before timing is decision evidence |

`FF32` is not allowed to mean an unbounded double-single estimate. Its `TwoSum`/FMA `TwoProd` construction must produce a proved enclosing interval (or a center plus outward error radius), and the complete 12,862-task raw-root gate must pass before it enters the timing table.

### 32.3 Benchmark protocol

- Use the frozen C13.1 corpus and stable identities. Deduplicate the ray/proxy phase by `(parent_case_index, ray/proxy inputs)`; retain all 12,862 records for the leaf phase.
- Use separate no-inline kernels per policy, identical input/output traffic, and a checksum/output dependency that prevents dead-code removal.
- Report operation-only kernels; file I/O, allocation, host/device copies, C13.1b's 4,096-bin scan, and process startup are excluded.
- Warm up at least 20 launches. Run at least 30 randomized policy-order trials with enough repeated items to exceed 2 ms per timed sample. Use CUDA events and report median, IQR, p05/p95, coefficient ns/item, leaf ns/item, and throughput.
- Record GPU model, compute capability, driver, CUDA compiler, clocks/power state when available, executable/configuration hashes, register count, static local memory, achieved occupancy, and at least L2/DRAM traffic. If Nsight Compute is unavailable, mark hardware counters missing rather than infer them.
- Validate every timed kernel's checksum against an untimed reference launch. Timing variability must have robust CV below 5%; otherwise increase batch work and repeat.
- Report native FP64 ratios against both `F32-GENERIC` and `F32-DIRECTED`. Report `FF32` only after its independent correctness artifact is linked.

### 32.4 Decision rule

The output is a Pareto decision, not an arbitrary single speed winner:

1. discard every policy with a raw exact-root omission, unresolved-tail regression, or uncertified arithmetic;
2. among the remaining policies, prefer the lowest measured two-part cost unless another policy materially reduces final candidate bins;
3. carry both coefficient and leaf costs into A2's later measured candidate/leaf counts—do not extrapolate a full-ray speedup from isolated ns/item; and
4. if native binary64 is clearly dominated, make certified `FF32` the next optimization target. Never loosen intervals, drop the exact-root gate, or silently revert to the C11 center polynomial.

Passing C13.2 authorizes freezing C13.3 representative refinement and world-equation/ownership filtering. It still does not authorize optimized DDA, rendering, Mode 1 edits, or external-baseline performance claims.

### 32.5 Frozen C13.2a compensated-FP32 correctness primitive

Before timing, `C13.2a-FF32-EQUATION` must consume the same raw 44 binary32 values and identities as C13.1a. Every scalar uses the ball invariant

$$
x \in (x_h+x_l)+[-r,r],
$$

where `x_h`, `x_l`, and nonnegative `r` are binary32. Inputs start as `(x,0,0)`. Centers use error-free `TwoSum`, `QuickTwoSum`, and FMA `TwoProd`; multiplication retains the high×low cross terms, and division uses a high quotient followed by one compensated residual correction. Every radius update uses upward-rounded binary32 operations.

The first implementation deliberately uses conservative center-truncation allowances rather than tuned constants:

- add/subtract: `128 * 2^-48 * (|a_h|+|a_l|+|b_h|+|b_l|)`;
- multiply: `256 * 2^-48 * (|a_h|+|a_l|) * (|b_h|+|b_l|)`; and
- divide: `2048 * 2^-48 * max(1, |q_h|+|q_l|)` after propagating numerator and denominator radii through a strictly positive denominator-magnitude lower bound.

These terms are part of the declared method, not empirical epsilon added by the validator. Overflow, non-finite components, negative radius, or a denominator ball containing zero returns unresolved with zero partial output. The device emits `(high,low,radius)` for four cubic coefficients only; it performs no bin scan.

The host independently reconstructs exact rational endpoints from the three packed floats, verifies enclosure of every raw exact cubic coefficient, then runs the unchanged closed Bernstein classifier over outward-binary64 projections of those rational endpoints. Authorization requires all 12,862 practical records, the six analytics, zero coefficient-enclosure failures, zero raw-root omissions, no partial unresolved data, and no new full-domain tail. If any exact coefficient escapes, increase or repair the analytically declared radius rule and regenerate a new artifact; do not patch individual cases.

## 33. C13.2 precision correctness and cost execution record — 2026-08-25

### 33.1 Authoritative artifacts

- FF32 vectors: `ray-a1-5-c13ff-vectors-423ae39fc429`;
- FF32 device differential: `ray-a1-5-c13ff-gpu-15fdbcb0947e`; and
- isolated cost benchmark: `ray-a1-5-c13cost-789bf1d99d23`.

The compensated implementation uses only binary32 center/error-free-transform/radius arithmetic in its measured kernels. Its host validator reconstructs each emitted ball exactly from the three packed floats and compares it to the raw exact rational equation.

### 33.2 Correctness and tightness

| FF32 practical result | Value |
|---|---:|
| Records | `12,862` |
| Raw coefficient enclosure failures | `0` |
| Raw exact-root omissions | `0` |
| Active bins, p50 / p95 / p99 / max | `0 / 1 / 1 / 176` |
| Full-domain tasks | `0` |
| Tasks above the eight-bin fallback threshold | `50` (`0.3887%`) |
| Adaptive FF32→full-F64 bins, p50 / p95 / p99 / max | `0 / 1 / 1 / 8` |
| Adaptive total active bins | `2,703` |
| Adaptive false-candidate tasks | `6` |
| Analytic/status/layout failures | `0` |
| **C13.2a gate** | **pass** |

The 50-task fallback is larger than C13.1b's native mixed/full 11-task set but remains below one percent. The adaptive result is only 43 bins above native F64 hybrid over the entire practical corpus while avoiding binary64 coefficient construction on ordinary paths.

### 33.3 Isolated RTX 5090 arithmetic costs

The benchmark uses 861 deduplicated ray/proxy inputs and all 12,862 practical leaves, 20 warmups, 30 randomized-order trials, at least 10 ms per timed sample, CUDA events, deterministic output checksums, and zero reported static local memory. Every robust timing CV is below 5%. These are arithmetic-kernel costs, not traversal time.

| Kernel | ns/item | Registers |
|---|---:|---:|
| setup binary32 generic | `0.5733` | `40` |
| setup binary32 directed | `0.9701` | `48` |
| setup binary64 directed | `33.7787` | `96` |
| setup compensated FF32 | `2.4221` | `72` |
| leaf binary32 directed | `0.2216` | `40` |
| leaf binary64 mixed | `0.6359` | `56` |
| leaf binary64 full | `1.4290` | `60` |
| leaf compensated FF32 | `0.1353` | `48` |

Native binary64 setup is `34.82×` directed binary32 and `13.95×` compensated FF32. FF32 setup is `2.50×` directed binary32; its leaf kernel is `0.61×` the directed-binary32 interval leaf on this device. Charging a full-binary64 leaf to every FF32 `>8` fallback gives `0.1409 ns/leaf`; an intentionally pessimistic bound that also charges a fresh binary64 setup per fallback leaf is `0.2722 ns`.

**Decision:** carry compensated FF32 balls as the production arithmetic candidate and retain an explicit full-binary64 fallback. Native binary64 remains the oracle/fallback, not the ordinary per-proxy setup. This decision is hardware-specific and must be remeasured on every paper GPU.

## 34. Frozen C13.3 exact-hit and ownership plan

C13.3 is split so root finding cannot hide an ownership or world-evaluation failure.

### 34.1 C13.3a safeguarded representative roots

`C13.3a-ROOT-REPRESENTATIVE` consumes one admitted closed normalized bracket, the physical height interval, and either an FF32 center polynomial or a full-binary64 fallback center. Practical C13.0 proves at most one raw exact root per leaf in the current corpus. Separate analytic records cover constant, linear, quadratic double-root, cubic simple/repeated/triple-root, endpoint roots, clustered roots, and false interval candidates.

The device partitions the cubic at all real derivative roots inside the closed height bracket. Every monotone subinterval with an endpoint zero or opposite endpoint signs is refined by safeguarded bisection with an in-bracket Newton proposal; a derivative-root value within the declared scale-aware residual handles even-multiplicity roots. Degree reduction uses coefficient-scale tests rather than exact `g3==0`. Output is zero to three sorted representatives plus iteration/fallback status. No root is accepted outside the input bracket.

The host compares against the complete square-free/Sturm raw reference. For practical leaves, a representative must exist iff the raw exact reference has a root, must lie inside its isolated rational interval enlarged only by the final numerical tolerance, and must meet a scale-aware polynomial residual. False C13.1 candidates must return no representative. Analytic repeated and endpoint roots must pass without relying on a sign change.

### 34.2 C13.3b rational/world hit filtering

Only after C13.3a passes, `C13.3b-HIT-FILTER` consumes the raw packed proxy/ray data, the original ray direction and `[t_min,t_max]`, packed microtriangle vertices, and a representative height. It independently reconstructs binary64 barycentric numerators `A(h),B(h)`, texture numerators `U(h),V(h)`, and denominator `D(h)`, then evaluates

$$
a=A/D,\qquad b=B/D,\qquad u=U/D,\qquad v=V/D,
$$

and computes `t` by projecting the shell world point onto the original ray. A scale-aware denominator guard, proxy barycentric check, closed texture-microtriangle check, ray-range check, height residual, polynomial residual, and world residual are mandatory. A failed numerical guard is unresolved, never a silent miss.

Every closed leaf owner is emitted. The host groups coincident records by the existing P-D1.6 relative `(t,h,u,v)` tolerance, unions owner triples, chooses the lowest-world-residual representative, sorts by `(t,h,u,v)`, and derives closest hit from that canonical order. The complete 864-case comparison must reproduce P-D1.6 group counts, exact owner sets, closest-hit identity, constructed-target retention, and residual gates. No traversal-discovery order participates in ownership.

Passing both C13.3 gates completes A1 mathematical correctness and authorizes the separately planned A2 optimized traversal. It does not itself establish a speedup.

### 34.3 Frozen C13.3b implementation sequence

Implement C13.3b in four separately inspectable steps:

1. **Host join and binary contract.** Join every practical C13.3a task to its C11 leaf, C1 parent, P-D1.6 tube, and original ray record. Store the C1 packed binary32 proxy positions, shell directions, texture coordinates, ray origin/direction, and ray frame; the C11 packed binary32 microtriangle `(u,v,h)` vertices; binary64 representative height; ray `[t_min,t_max]`; and a stable tube index. Keep all 12,862 practical leaf tasks, including those with no representative, so rejected and absent roots cannot disappear from accounting.
2. **Independent device evaluation.** In binary64, reconstruct `A(h), B(h), U(h), V(h), D(h)` from the packed proxy/ray data rather than reading C1 or C11 coefficient outputs. Reconstruct `t(h)` from the world projection formula. Fail closed on a nonfinite or scale-small denominator. Apply the closed proxy, ray-range, and microtriangle tests with the declared `2e-9` relative coordinate tolerance. Re-evaluate the affine microtriangle height and the world ray/shell equality; retain only candidates satisfying the declared height/world residual gates. Emit the reject mask and all diagnostics for every task.
3. **Canonical host ownership.** Group accepted records per tube using the unchanged P-D1.6 relative `(t,h,u,v)` relation, union all `(cell_x,cell_y,local_triangle)` owners, select the lowest-world-residual representative, sort canonically by `(t,h,u,v)`, and take the first group as closest. Traversal or task order is never an ownership rule.
4. **Differential gate.** Across all 864 tubes, require equality with the declared-surface oracle for group counts, complete owner sets, closest owner identity, and the coordinate/height/world residual limits. Report the unchanged P-D1.6 comparison separately when its unquantized surface differs. Report input representatives, accepted/rejected counts by mask, groups, owners-per-group, shared-edge/shared-corner groups, closest-hit disagreements, and p50/p95/p99/max residuals. Add analytic denominator-zero, proxy-edge, grid-edge, grid-corner, ray-endpoint, outside-triangle, outside-time, and nonfinite records. Only a passing gate authorizes A2.

The reference comparison remains P-D1.6, but the device arithmetic target is the explicitly packed production input. If the original binary64 P-D1.6 coordinates and the packed-input geometry differ beyond the frozen tolerance, stop and record the representation mismatch; do not widen the tolerance. In that case, produce an additional packed-input host oracle and decide explicitly which surface contract the paper claims before A2 begins.

### 34.4 C13.3b first execution checkpoint and frozen remediation

The first C13.3b execution reached exactly the mismatch condition above:

- vectors: `ray-a1-5-c13h-vectors-e297bf8d9416`;
- diagnostic GPU run: `ray-a1-5-c13h-gpu-9aa99cdebe3f` (**gate failed; not an authoritative correctness artifact**);
- all eight analytic filter cases passed and no practical record was unresolved;
- only 144 owner records survived, while P-D1.6 has 1,800 event owners in 936 groups;
- 2,343 representatives failed the `2e-9` world residual; and
- the largest coordinate difference from the original binary64 P-D1.6 surface was `9.2363e-8`.

The cause is now explicit. C1's binary32 projected frame defines a very accurate shell-space candidate curve, but the rounded frame is not exactly perpendicular to the packed ray. Therefore a root of the projected leaf cubic need not be an exact world-ray hit. The projected curve remains candidate-generation arithmetic; it cannot be the final geometric definition.

Before revising the kernel, freeze the following correction:

1. The paper/runtime surface contract is the **packed binary32 proxy, shell directions, texture coordinates, displacement vertices, ray origin, and ray direction**, evaluated in binary64 for this correctness gate. The older unquantized P-D1.6 surface remains a design-reference diagnostic. Its tolerance is not widened.
2. Treat the C13.3a representative as an initial guess. At the leaf solve the actual four-equation system in unknowns `(a,b,h,t)`:

   $$
   S(a,b,h)-[O+tR]=0,
   \qquad
   h-c-g_u u(a,b)-g_v v(a,b)=0.
   $$

   Use a pivoted binary64 Newton solve with the analytic `4x4` Jacobian, bounded iterations, a scale-aware pivot guard, and an explicit whole-candidate unresolved state on failure. The final proxy, microtriangle, height-window, time, height, polynomial, and world tests are applied to the refined tuple, not the projected tuple.
3. Add the packed leaf height interval to the binary contract. Independently build a host packed-input oracle by constructing an orthonormal binary64 ray frame from the packed direction, deriving its rational curve and represented leaf cubic, isolating every root over every packed leaf interval, filtering in world space, and canonically grouping owners. This oracle must not use the device representative or Newton iteration.
4. The authoritative C13.3b gate compares device results to this packed-input oracle for group count, owner sets, closest hit, and coordinates. A second unchanged comparison to P-D1.6 reports topology/owner agreement and the quantization-induced coordinate delta as a diagnostic. The deliberately exact grid-corner/proxy-edge rays change ownership under input quantization, so P-D1.6 topology is **not** a gate for the different packed surface. Coordinate or boundary-owner equality to the unquantized surface is neither expected nor claimed; all such differences remain reported rather than hidden by a wider tolerance.

This correction does not change segmentation, the conservative tube, hierarchy candidates, or DDA. It makes their admitted leaf candidate produce an exact hit on the declared production geometry.

### 34.5 Exact ray-annihilator coefficient correction

The packed oracle shows four admitted packed leaf roots for which the old rounded-frame cubic emits no representative. Therefore Newton refinement alone is insufficient: an exact leaf root must exist before refinement has a seed.

For A2, replace the normalized Frisvad projection frame with two **unnormalized dominant-axis ray-annihilator planes**. If `|R_z|` is dominant, for example, use

$$
E_0=(R_z,0,-R_x),\qquad E_1=(0,R_z,-R_y).
$$

The other dominant-axis cases are cyclic permutations. In real arithmetic `E_i dot R=0`; in IEEE arithmetic each dot uses the same product pair with opposite signs, avoiding normalization, square root, and the stored-frame orthogonality drift. Because the dominant direction component is used, the construction also avoids dividing by a small ray component.

Freeze the following bridge test before A2 timing:

1. C13.3b independently constructs `A,B,U,V,D` with the dominant-axis annihilators and constructs the exact represented leaf cubic over the packed leaf height interval.
2. It isolates the complete root set in that interval. The older C13.3a FF32 representative is retained as a measured initial-guess/ablation input, but it is not allowed to suppress a root of the exact packed equation.
3. Each exact-plane root is refined/evaluated in the four-equation packed world system and filtered canonically. Multiple-root or bounded-work pressure fails closed for the whole leaf.
4. Report how often the exact annihilator agrees with the old representative and how often it recovers a root. This count is a design-correction diagnostic, not a claimed speedup.

If this bridge passes the independent packed oracle, A2 must use the same annihilator construction for both curve segmentation and leaf equations. The old rounded-frame path remains only as a historical ablation.

## 35. C13.3 execution record — 2026-08-25

### 35.1 Safeguarded representative roots

- vectors: `ray-a1-5-c13r-vectors-5c77e1999375`;
- device differential: `ray-a1-5-c13r-gpu-6c99b773a0c6`;
- records: 12,862 practical plus 12 analytic; and
- result: **pass**.

The device emitted 2,652 total representatives, retained all repeated, triple, clustered, endpoint, degree-reduced, and `2^-80` scale analytic roots, and rejected the analytic false interval candidate. There were zero root-count, exact-isolation, bracket, or scaled-residual failures. Eighteen shell-endpoint roots used the explicit independently validated binary64 boundary recovery. Practical safeguarded refinement iterations were p50/p95/p99/max `0/33/43/52`.

### 35.2 Exact packed hit and ownership filtering

- vectors: `ray-a1-5-c13h-vectors-d5582d7da860`;
- device differential: `ray-a1-5-c13h-gpu-68a78de884f8`;
- records: 12,862 practical plus eight analytic; 864 tubes; and
- result: **pass** against the independently constructed packed-input oracle.

The packed oracle contains 807 canonical groups/owners over the currently admitted leaf corpus. Device group count, owner sets, closest order, and coordinates all match; the maximum device/oracle `(t,h,u,v)` difference is `8.42e-14`. Maximum height and world residuals are `7.10e-14` and `3.69e-16`. All denominator-zero, proxy-edge, grid-edge, grid-corner, ray-endpoint, outside-triangle, outside-time, and invalid-direction analytics pass, with no unresolved practical leaf.

The exact dominant-axis annihilator found 2,624 leaf roots. It recovered nine roots absent from the old rounded-frame representative and rejected seven old representatives that are not roots of the declared packed equation. Only 1,261 old representatives agree with the corrected root within `2e-9`; therefore the old rounded-frame C13.3a result is retained as an ablation/diagnostic, not the production coefficient definition.

The unchanged unquantized P-D1.6 diagnostic differs on 397 deliberately boundary-sensitive tubes and reaches a `0.0966` coordinate delta in a reordered/missing paired-group diagnostic. This is expected for different rays/surfaces and is not hidden by widening tolerance. The paper/runtime contract is the packed production surface.

### 35.3 A1 handoff

A1 mathematical correctness is complete and A2 is authorized with two mandatory constraints:

1. both candidate segmentation and exact leaf equations use the dominant-axis ray-annihilator construction; and
2. A2 end-to-end coverage is compared to a complete packed-input surface oracle, not merely the inherited unquantized P-D1 cell list.

No traversal speedup, rendering result, or external-baseline conclusion follows from C13.3 alone.

---

Related: [[Plan — Certified ray architecture comparison]] · [[Plan — P-D1 certified tube-supercover DDA reference]] · [[Guide — Eurographics paper draft]]
