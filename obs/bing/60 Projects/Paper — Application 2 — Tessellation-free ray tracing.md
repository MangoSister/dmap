---
title: "Application 2 — Tessellation-free ray tracing with event-segmented shell rays"
tags: [paper, application-2, ray-tracing, displacement-maps, first-order, dda]
status: maintained-draft
evidence-cutoff: P31
updated: 2026-09-07
---

# Application 2 — Tessellation-free ray tracing with event-segmented shell rays

> **Status and intended use.** This is the maintained Markdown chapter for the
> ray-tracing application of the eventual three-application first-order
> representation paper. It synthesizes the preserved implementation and
> measurements through P31. It does not replace or overwrite the standalone
> [LaTeX ray-tracing draft](../../../../paper_dmap_query/ray-tracing-only-draft.tex).
> Numbers are attached to their frozen reports, and statements labeled as
> interpretation or open work are not experimental facts.

## Executive summary

This application renders scalar displacement maps over coarse triangle proxies
without materializing a displacement-resolution tessellation. A world-space ray
is straight, but after inversion through the triangle shell its texture
coordinates are rational functions of displacement height and trace a curved
path. Our renderer partitions that path at analytic events, adaptively replaces
each valid subarc by a measured-error linear chord, visits the chords globally
front-to-back, and traverses each chord through the displacement grid with a
hierarchical DDA. Linear represented-leaf tests complete primary-closest,
secondary-closest, and visibility queries. The strict candidate has no call to
the nonlinear Mode 0 height solver.

The shared first-order representation appears inside this application as a
**tagged plane-plus-residual hierarchy**. Along a linear chord, subtracting a
node's local displacement plane leaves an affine compensated height, so a thin
residual slab can reject cells more tightly than scalar min/max. The tagged
first-order record is packed to the same eight bytes as the scalar record and
is selected only when preprocessing predicts that its extra arithmetic will
pay off. It is used by primary, secondary-closest, and visibility rays on
admitted maps; rough maps use scalar bounds.

The causal attribution matters. On the controlled P22 cohort, the all-scalar
linear+DDA renderer is **1.75×** faster than Mode 0, while changing scalar bounds
to all-ray tagged first-order bounds adds **1.094×** throughput. Thus the core
speedup is the application-specific event segmentation, front-to-back DDA, and
linear leaf work; the first-order representation is a smaller, conditional
accelerator. On the broad 247-pair P23 suite, automatic dispatch gives a
**0.6202×** geometric-mean total-frame ratio (**1.61× speedup**) and is faster in
239/247 cases. All 247 cases pass the primary mask, position, normal, and beauty
PSNR gates; 225/247 pass the stricter full contract because the remaining 22
have dark-region proxy exceptions. These are approximate, quality-controlled
results, not an exact or certified equivalence to Mode 0.

Primary sources: [method/claim audit](../../../../docs/p18_method_claim_audit.md),
[all-ray first-order ablation](../../../../docs/p22_all_ray_first_order.md),
[complete suite](../../../../docs/p23_complete_suite.md), and
[fairness analysis](../../../../docs/p20_fairness_and_speed_analysis.md).

![Application-level method overview](../../../../figures/ray_tracing_paper/rt_method_overview.png)

## 1. Role in the unified paper

The unified paper studies a first-order displacement representation and several
queries that consume it. The current user-facing numbering is **Application 1:
Area/Sampling**, **Application 2: Ray Tracing**, and **Application 3: pending**.
Older project notes that put ray tracing first are historical and do not control
the new paper structure. Ray tracing is an extrinsic
visibility query whose output is a complete path-traced image, not an isolated
microbenchmark. This chapter should make two levels of contribution explicit:

1. The ray-specific algorithm converts a nonlinear shell-space curve into an
   ordered sequence of linear traversal problems and exploits DDA locality.
2. The shared first-order representation tightens hierarchy rejection inside
   that fixed traversal for slope-dominant maps.

The chapter must not attribute the complete Mode 0 speedup to first order. The
P22 2×2 experiment separates primary and outgoing scalar/first-order choices
and is the causal evidence. P23's 39 automatically first-order-selected pairs
are useful breadth evidence, but selection depends on map content and therefore
is not a causal ablation.

The historical project notes include a different, certified rational-tube and
cubic-leaf research line whose first-order clipping result was negative. That
line is scientifically useful but is not the measured GPU method documented
here. In particular, the current chord error is measured/error-targeted, the
leaf surface is a linear approximation, and the current renderer must not
inherit the word “certified” from the earlier reference work. See the
[project history](<Project — Conservative first-order queries on displacement maps.md>)
and the [earlier EG handoff](<Note — EG draft handoff after P-D1.md>)
for the distinction.

## 2. Motivation

Displacement maps encode fine relief compactly, but conventional ray tracing
must either expand them into many triangles or repeatedly solve a nonlinear
intersection problem. Expansion increases geometry memory, build/update cost,
and acceleration-structure pressure. Nonlinear on-the-fly intersection avoids
that storage but performs expensive interval and root work inside a deeply
repeated traversal loop.

The opportunity is that the nonlinear difficulty is structured. For one proxy
triangle, inverse shell coordinates are low-degree rational functions of the
height parameter. Poles, turns, and proxy-boundary crossings can be computed
once per candidate ray/proxy interval. Between those events, a short linear
chord usually gives a coherent front-to-back path through the height grid. DDA
then reuses cell adjacency instead of restarting a nonlinear node or leaf query
at every location.

Scalar min/max nodes lose correlation between texture position and displacement
height. A smooth tilted height field can have a wide scalar range even though
its residual from a local plane is narrow. Retaining that predictable slope is
the application-level reason to test the shared first-order representation.

## 3. Represented surface and assumptions

### 3.1 Triangle shell

Let a coarse proxy triangle use barycentric coordinates $(a,b)$, with
$c=1-a-b$. Base position and displacement direction are affine:

$$
P(a,b)=P_0+a(P_1-P_0)+b(P_2-P_0),
$$

$$
N(a,b)=N_0+a(N_1-N_0)+b(N_2-N_0).
$$

The three-dimensional shell map is

$$
F(a,b,h)=P(a,b)+hN(a,b).
$$

The triangle's texture map $T(a,b)$ is affine. For scalar displacement texture
$H_\tau(u,v)$ and inverse texture map $T^{-1}$, the target surface is

$$
S(u,v)=F\!\left(T^{-1}(u,v),H_\tau(u,v)\right).
$$

This requires a single-valued displacement graph over a valid proxy shell. The
shell may be curved because $hN(a,b)$ is bilinear in barycentrics and height.
Shared edges require consistent parameterization, displacement directions,
sampling, orientation, and ownership. Arbitrary target-to-shell conversion is
outside the current renderer's claim.

### 3.2 Nonlinear Mode 0 and linear represented leaves

Mode 0 evaluates the repository's nonlinear height-map intersection path over
the shell representation. The candidate deliberately uses a linearized
represented surface at the leaf:

- primary-closest and secondary-closest use eight planar pieces per texel in
  the accepted fast configuration;
- visibility uses the cheaper two-triangle representation, followed on curved
  proxies by a local first-order/Jacobian correction only after a planar miss;
- shading normals come from the analytic Jacobian of the represented hit;
- the older 32-piece primary representation is preserved as a compile-time
  quality variant, not used by the headline runs.

Consequently, the candidate approximates Mode 0 even if its hierarchy tests are
conservative for its own reconstruction. The comparison is an explicit
quality–time tradeoff. The final method description is recorded in the
[P18 claim audit](../../../../docs/p18_method_claim_audit.md) and the curved-proxy
visibility repair in [P26](../../../../docs/p26_skull_visibility_fix.md).

## 4. Shell-space ray formulation

For world/object-space ray

$$
R(t)=O+tV,
$$

solve $R(t)=F(a,b,h)$ and use shell height $h$ as the curve parameter. Eliminating
$t$ gives

$$
a(h)=\frac{A(h)}{D(h)},\qquad
b(h)=\frac{B(h)}{D(h)},
$$

where $A$, $B$, and $D$ have degree at most two for an affine-normal triangle
shell. Texture coordinates are affine functions of $(a,b)$, hence rational in
$h$. World-ray time can also be evaluated as a rational function of $h$.

The valid curve is restricted by the ray range, the shell's admitted height
band, nonsingular denominator regions, and proxy ownership
$a\ge0$, $b\ge0$, $a+b\le1$. A fold-aware Jacobian test decides when boundary
roots safely delimit the domain; uncertain or folded shells use the conservative
six-corner shell AABB interval. This preserves coverage without invoking the
nonlinear leaf solver. The development history is in
[P17](../../../../docs/p17_accuracy_fix.md).

## 5. Event-first ray segmentation

### 5.1 Analytic events

The candidate first partitions each admitted height interval at structural
events rather than asking a uniform sampler to discover them. The event set
contains:

- interval endpoints and ray-range endpoints;
- real roots of $D(h)=0$ (rational poles or singular boundaries);
- stationary points of $a(h)$ and $b(h)$, obtained from
  $A'(h)D(h)-A(h)D'(h)=0$ and
  $B'(h)D(h)-B(h)D'(h)=0$;
- proxy-boundary crossings $A(h)=0$, $B(h)=0$, and
  $A(h)+B(h)-D(h)=0$.

After sorting and deduplicating events, midpoint classification discards
intervals not owned by the proxy. Within a retained interval, coordinate
monotonicity and proxy membership are more stable, making the adaptive chord
test and DDA ordering useful. The event code remains in
[the P14 traversal header](../../../../nrtdsm/gpu_kernels/p14_fast_segment_dda.h);
later phases extend it rather than deleting it.

### 5.2 Adaptive chord error

For an event interval $I=[h_0,h_1]$, let $u(h)=(u(h),v(h))$ be the exact
shell-space texture trace and $\ell(h)$ the endpoint chord evaluated with the
same normalized height parameter. At the midpoint $h_m$ the implementation
measures the synchronous UV deviation

$$
\delta_I=\left\|u(h_m)-\ell(h_m)\right\|_2.
$$

For texture dimensions $W\times H$, the user threshold $\tau$ is converted to
a UV-scale tolerance

$$
\varepsilon=\frac{\tau}{\sqrt{WH}},
$$

and the interval receives approximately

$$
m_I=\max\!\left(1,
\left\lceil\sqrt{\delta_I/\varepsilon}\right\rceil\right)
$$

subsegments, subject to numeric and capacity rules. Primary,
secondary-closest, and visibility rays have independently configurable
thresholds. The accepted defaults used in the broad development line were
0.06, 0.05, and 0.10 texel; the P26 local-light artist scenes use the tighter
visibility value 0.01.

This is a practical curvature-inspired estimate, not a supremum-norm
certificate. “Adaptive,” “error-targeted,” or “measured-error chord” are safe;
“certified chord” is not.

### 5.3 Bounded segment storage

The fast implementation materializes at most 32 chord records. A former strict
overflow policy turned overflow into a miss and caused the twisted rocky-trail
shadow/secondary artifacts. The accepted bounded-memory policy either allocates
the remaining records to the final owned interval or rebuilds with four times
the tolerance, for at most three bounded retries. It propagates the effective
relaxed envelope to traversal. No route calls Mode 0. Exact-threshold paged
streaming is preserved as a negative ablation because rescanning made it much
slower. See [P18 linear-only fix](../../../../docs/p18_linear_only_accuracy_fix.md)
and the [claim audit](../../../../docs/p18_method_claim_audit.md).

![Events, adaptive chords, and DDA](../../../../figures/ray_tracing_paper/rt_events_chords_dda.png)

## 6. Global front-to-back traversal

Every final chord is assigned the minimum projected world-ray time over its
endpoints/valid range. Chords are ordered globally by this entry key before any
leaf traversal. The implementation detects already increasing and decreasing
sequences and otherwise uses a stable insertion sort, which is appropriate for
the small bounded record count.

For closest-hit rays, the current best world time clips every later chord.
Entire chords whose entry is beyond the best hit are skipped. Within a chord,
ordered hierarchical DDA clips against coarse nodes and then advances through
leaf cells using adjacency. Visibility rays terminate on the first accepted
blocker. This is where front-to-back ordering and DDA reinforce each other:
DDA makes an individual chord cheap and coherent, while ordering turns an early
hit into pruning across all later chords.

Mode 0 also uses front-to-back child ordering in its nonlinear hierarchy. The
claim is therefore not that the baseline is unordered; it is that the candidate
exposes an inexpensive global chord order and replaces repeated nonlinear
node/leaf work by affine interval tests and incremental grid stepping.

## 7. Scalar and tagged first-order hierarchy bounds

### 7.1 Scalar bound

A scalar node over texture domain $D_n$ stores

$$
H(u)\in[h_n^{\min},h_n^{\max}],\qquad u\in D_n.
$$

After clipping a chord to $D_n$, the scalar test compares its height interval
with the padded scalar height range. It is inexpensive but includes both the
predictable slope and nonlinear residual of the node.

### 7.2 First-order plane plus residual

For center $u_n$, fit

$$
p_n(u)=h_{0,n}+g_n^\mathsf{T}(u-u_n)
$$

and store a conservative residual interval

$$
H(u)-p_n(u)\in[r_n^{\min},r_n^{\max}].
$$

Along chord $(u(s),h(s))$, both $u(s)$ and $h(s)$ are affine. Therefore

$$
q_n(s)=h(s)-p_n(u(s))
$$

is affine and its exact interval on the clipped segment is obtained from the
two endpoints. If that interval does not overlap the padded residual interval,
the node is rejected. The predictable height slope has been removed before the
overlap test.

### 7.3 Tagged, size-matched storage

The first-order record is one packed `half4`/`uint2` (eight bytes), size-matched
to the scalar `float2` record. A per-node tag retains scalar form where scalar
is tighter or plane packing is not worthwhile. “Tagged first-order hierarchy”
is therefore the precise name: not every node is a plane node.

Automatic map admission requires all three measured preprocessing criteria:

| Statistic | Admission rule |
|---|---:|
| First-order-tagged node fraction | at least 0.95 |
| Nodes at least 2× tighter | at least 0.85 |
| Weighted residual/scalar width | at most 0.25 |

The residual range is conservative for the reconstructed bilinear height map,
including expansion for half-precision packing. This statement applies to the
hierarchy record, not to the full ray/surface approximation.

### 7.4 Primary, secondary, and visibility use

P19 first applied the representation only to outgoing secondary/visibility
rays and measured 1.052×. P22 supersedes that interpretation. Primary and
outgoing families now have independently specialized scalar and first-order
OptiX entries, yielding a 2×2 experiment:

| | Outgoing scalar | Outgoing first order |
|---|---|---|
| Primary scalar | SS | SF |
| Primary first order | FS | FF |

In automatic rendering, an admitted map selects first order for all three ray
families. A rejected rough map selects scalar. Primary traversal also has a
lean entry for low-resolution/low-proxy-density cases where hierarchy overhead
is unlikely to amortize. The key controls are
`-p16-primary-hierarchy auto|lean|scalar|first_order` and
`-p15-hierarchy auto|scalar|first_order`.

Separate program entries avoid a hot per-ray representation branch and keep the
traversal schedule constant in the causal ablation. [P19](../../../../docs/p19_first_order_ray_tracing.md)
is retained as the superseded outgoing-only study; [P22](../../../../docs/p22_all_ray_first_order.md)
is authoritative for all-ray first order.

![How the first-order residual slab tightens DDA rejection](../../../../figures/ray_tracing_paper/rt_first_order_mechanism.png)

## 8. Affine specialization

If all three proxy displacement directions are equal, $F(a,b,h)$ is affine in
$(a,b,h)$. One inverse matrix maps the world ray to an exact straight line in
canonical shell space. The specialization emits one exact chord and skips the
general rational coefficient construction, denominator roots, event sorting,
midpoint error estimation, tube padding, and world-time turning work.

A separate zero-height-width degeneracy is also affine even when vertex normals
vary: fixing $h=h_0$ maps the proxy to one displaced affine triangle, so a
canonical linear test is exact for that case. The general event implementation
remains compiled for non-affine proxies.

This is an important implementation optimization for planar boards but is not
the core representation or the headline algorithm. P28's planar motherboard
shows that full-frame speed can remain modest when path-stage work dominates.
See [P28 artist board scenes](../../../../docs/p28_artist_board_warm_scenes.md).

## 9. Linear-only accuracy safeguards

The accepted path combines several repairs accumulated in P17–P29:

- scale-relative boundary-polynomial classification and accumulation of both
  valid side roots;
- fold-aware shell-domain selection, with a conservative AABB interval for
  uncertain/folded shells;
- synchronous chord error in the same height parameter used by traversal;
- tangential/proxy-domain padding and an error envelope around the chord;
- closed/supercover cell ownership near grid boundaries;
- oriented microtriangle normals on mirrored UV regions;
- refined eight-piece primary and secondary-closest leaves;
- a bounded representation-mismatch self-hit rule for outgoing rays;
- on curved proxies, planar-first visibility plus one local canonical-space
  first-order/Jacobian correction after a planar miss;
- explicit capacity relaxation/retry rather than overflow-to-miss.

The P26 correction deserves precise language. It intersects the linear chord
with canonical $(u,v,h)$ leaf microtriangles and applies one local linear
Jacobian correction to the mapped patch. It is not a polynomial root solve, an
iterative nonlinear tracer, or a call to Mode 0. The cheap planar visibility
test remains first, so the correction is conditional.

### No-nonlinear-fallback boundary

For evaluated height-map scenes, the strict builds use
`NRTDSM_P18_LINEAR_ONLY=ON` and
`NRTDSM_P17_EXACT_LEAF_REFINEMENT=OFF`. Primary-closest,
secondary-closest, and visibility queries cannot dispatch to the Mode 0
nonlinear leaf solver. Degenerate and capacity paths stay within bounded linear
logic. Separate shell-map representations or methods outside this evaluated
height-map scope are not silently covered by that claim.

The result is nevertheless **approximate**: leaf refinement is finite, chord
error is heuristic/measured, the fast visibility envelope is not a rigorous
tube certificate, and bounded capacity may relax the target. The correct paper
claim is “linear-only with no nonlinear fallback under an explicit image/AOV
quality contract,” not “exact” or “certified.”

## 10. Algorithms

### Algorithm 1: event-segmented chord construction

```text
BUILD-CHORDS(ray R, proxy triangle T, ray family f, threshold tau_f)
    I <- conservative shell-height intervals intersected by R
    if shell map of T is affine:
        return one exact canonical-space chord, clipped to proxy/ray range

    C <- empty bounded array (capacity 32)
    for each shell interval J in I:
        form rational a(h)=A(h)/D(h), b(h)=B(h)/D(h)
        E <- endpoints(J)
        E <- E union roots(D)
        E <- E union roots(A'D - AD') union roots(B'D - BD')
        E <- E union roots(A) union roots(B) union roots(A+B-D)
        sort and deduplicate E

        for each adjacent event interval K=[h0,h1] in E:
            discard K unless its midpoint is valid and proxy-owned
            delta <- synchronous midpoint UV deviation from endpoint chord
            epsilon <- tau_f / sqrt(texture_width * texture_height)
            m <- max(1, ceil(sqrt(delta / epsilon)))
            append m uniformly parameterized chords over K to C

    if C overflowed:
        cap the final owned interval to remaining slots, or
        retry at 4x tolerance, at most three times
        propagate the effective relaxed error envelope

    compute each chord's minimum projected world-ray time
    return C in global front-to-back order
```

### Algorithm 2: hierarchy plus DDA traversal

```text
TRACE-LINEAR-DDA(ray R, proxy T, ray family f, hierarchy choice k)
    chords <- BUILD-CHORDS(R, T, f, tau_f)
    best_t <- ray.t_max

    for chord c in global front-to-back order:
        if c.entry_t >= best_t: break
        clip c to [ray.t_min, best_t] and proxy domain

        for node n visited by ordered hierarchical DDA over c:
            c_n <- clip c to node texture box
            if k(n) is FIRST_ORDER:
                q <- interval of h(c_n)-p_n(u(c_n)) from endpoints
                reject n if q misses padded [r_min,n, r_max,n]
            else:
                reject n if chord-height(c_n) misses padded [h_min,n,h_max,n]

            if n is internal and admitted: descend in chord order
            if n is a leaf:
                test its linear represented microtriangles
                if f is VISIBILITY and planar test misses on a curved proxy:
                    try local canonical-space/Jacobian linear correction
                if an owned hit is accepted:
                    if f is VISIBILITY: return BLOCKED
                    best_t <- min(best_t, hit.t)

    return best hit, or UNBLOCKED/MISS
```

### Algorithm 3: preprocessing and dispatch

```text
BUILD-AND-SELECT-BOUNDS(height texture H)
    build scalar min/max hierarchy
    fit a local height plane and conservative packed residual at each node
    tag each node FIRST_ORDER only where the packed plane/residual is useful;
    otherwise store scalar form in the same eight-byte record

    measure map-level tagged fraction, 2x-tighter fraction,
            and weighted residual/scalar width
    if all first-order admission gates pass:
        select first-order SBT entries for primary, secondary, visibility
    else:
        select scalar entries

    for trivial low-density primary workloads:
        the independent primary policy may select the lean entry
```

## 11. Implementation details

### 11.1 GPU organization

The renderer is C++/CUDA/OptiX. Hardware acceleration admits conservative
shell AABBs for coarse proxy triangles; custom intersection programs perform
the shell-space query. The main implementation lives in
[the NRTDSM renderer](../../../../nrtdsm/nrtdsm_main.cpp) and
[the P14 traversal header](../../../../nrtdsm/gpu_kernels/p14_fast_segment_dda.h).

The candidate is compiled into specialized primary scalar, primary first-order,
outgoing scalar, and outgoing first-order entries. Shader-binding-table
selection occurs once per geometry/map policy, rather than branching on the
bound type in the traversal loop. Secondary-closest and visibility share the
outgoing representation choice but have different termination and leaf logic.

The adaptive hierarchy has two useful scales: a coarse map level and a local
block level before leaf DDA. P16 used approximately 64 coarse nodes per map axis,
then a 4×4 primary block or an 8×8 secondary block with the existing 4×4 probe.
Low-density 1K primary workloads can bypass hierarchy overhead through the lean
entry. The development and dispatch rationale is in
[P16](../../../../docs/p16_adaptive_hdda.md).

### 11.2 Preprocessing

Preprocessing builds the scalar pyramid, plane/residual candidates, conservative
packed residual expansion, tags, and map-level dispatch statistics. These costs
are excluded from the steady-state frame timer and should be reported
separately in a final paper memory/build table. The current performance evidence
is a steady-state renderer comparison, not a claim about total load-to-first-
frame latency.

### 11.3 Ray-family details

| Family | Query | Accepted leaf | Early termination |
|---|---|---|---|
| Primary | closest | eight-piece linear leaf | best-hit clipping |
| Secondary | closest | eight-piece linear leaf | best-hit clipping |
| Visibility | any hit | planar two-piece first; local linear correction on curved-proxy miss | first blocker |

Primary and secondary use the same eight-piece leaf in the accepted fast build,
so their representation-mismatch self-hit term compiles out. It remains for the
coarser visibility representation. The self-hit rejection distance is bounded
by the local difference between the fine primary and outgoing represented
surfaces; it is not an arbitrary global epsilon.

### 11.4 Current strict build contract

The P26/P28/P29 artist runs use the isolated
`build-nrtdsm-p26-fast-supercover` configuration. Important switches are:

```text
NRTDSM_P14_FAST_SEGMENT_DDA=ON
NRTDSM_P15_FIRST_ORDER_HIERARCHY=ON
NRTDSM_P16_PRIMARY_HDDA=ON
NRTDSM_P16_ORIENT_MICRO_NORMALS=ON
NRTDSM_P17_CONSERVATIVE_RAY_DOMAIN=ON
NRTDSM_P17_FOLD_AWARE_RAY_DOMAIN=ON
NRTDSM_P17_SYNCHRONOUS_CHORD_ERROR=ON
NRTDSM_P17_TANGENTIAL_PARAMETER_PADDING=ON
NRTDSM_P17_EXACT_LEAF_REFINEMENT=OFF
NRTDSM_P18_LINEAR_ONLY=ON
NRTDSM_P18_REFINED_LINEAR_LEAF=ON
NRTDSM_P18_REPRESENTATION_SELF_HIT_GUARD=ON
NRTDSM_P26_FAST_VISIBILITY_SUPERCOVER=ON
```

Frozen suite results remain attached to the exact executable recorded by each
report. A later source tree or build should not inherit an older numerical result
without rerunning the suite.

## 12. Baselines

### 12.1 Mode 0 nonlinear ray tracing

Mode 0 is the main same-surface reference. It is launched from the same NRTDSM
executable as the candidate, with the same proxy, displacement input, camera,
environment, materials, integrator, resolution, and sample schedule. Only the
intersection program/SBT selection changes. It traces the repository's
nonlinear shell/height surface and provides deterministic primary geometry,
position, and normal AOVs as well as a stochastic beauty reference.

Mode 0 is not a naïve unordered baseline: it already uses ordered hierarchy
logic. The valid claim is that the candidate replaces nonlinear repeated work
with event-segmented linear work and grid-coherent DDA, not that it adds the
first ordering of any kind.

### 12.2 TFDM

The in-repository TFDM fork is a separate full renderer. P21 added only a
headless benchmark adapter: resolution, warm-up/measured counts, output paths,
GPU-event timing fields, deterministic AOV readback, and a no-presentation
batch loop. The standard TFDM intersection kernel remains the two-triangle
target-level leaf. An optional bilinear path was rejected during bring-up after
reversed-normal failures; it is not used to manufacture a stronger baseline.

P21 is fair as an **equal-input full-render comparison**: each case uses the
same mesh, displacement image and scale, camera, environment, resolution, path
length, and one-sample-per-measured-frame schedule. It is not equal-surface or
equal-quality. TFDM's two-triangle local surface differs from Mode 0's nonlinear
surface under shell warping, and it passes the primary geometry gate in only
12/30 cases. Therefore “2.27× faster than this in-repository standard TFDM
implementation on equal inputs” is supported; universal or equal-quality TFDM
dominance is not. See [P21](../../../../docs/p21_tfdm_baseline.md).

### 12.3 Baselines not yet measured

The present repository evidence does not include a publication-grade matched
comparison against RMIP, relief/parallax mapping variants, dense explicitly
tessellated triangles, or a hardware displacement-micromesh path. These belong
in open work, not in the completed result table.

## 13. Fair comparison protocol

The broad P20/P21/P23 protocol is designed around complete rendering rather
than a primary-ray-only kernel:

- 1920×1080 output, maximum path length two, one sample per measured frame;
- primary-closest, secondary-closest, and visibility queries all use the tested
  method;
- 10 discarded warm-up frames and 30 GPU-event-timed frames per method/case;
- correctness and timing are separate processes;
- method and case order are randomized under the recorded seed;
- paired methods share mesh, displacement, scale, camera, environment,
  materials, integrator, resolution, and sampling schedule;
- loading, texture/hierarchy construction, acceleration-structure construction,
  OptiX compilation, readback, PNG encoding, and file I/O are outside the
  steady-state timer;
- counter-enabled instrumentation binaries are separate from production timing
  binaries.

Artist audits use the same paired principle with their recorded resolutions,
path length three, finite lights, material maps, 10 warm-ups, 30 measured
frames, and a separate 128- or 256-spp hero pair.

The two same-executable NRTDSM arms share code revision and renderer. Different
specialized entries are a deployable implementation comparison and avoid a hot
algorithm branch. TFDM necessarily uses a separate fork; P21 therefore reports
both total-frame and the tighter combined G-buffer+path timing boundary.

### 13.1 Accuracy contract

Correctness is not inferred from a beauty montage alone. Reports compare:

- primary hit-mask mismatch;
- common-hit world-position p99;
- common-hit shading-normal vector p99;
- whole-image beauty PSNR;
- signed mean luminance over Mode 0-dark regions; and
- a positive dark-leak proxy.

Deterministic geometry AOVs isolate primary intersection errors. Beauty and
dark-region measures expose changed secondary/visibility decisions, but also
contain Monte Carlo variation. Since a changed hit can alter later bounces, the
candidate and Mode 0 do not replay literally identical secondary-ray streams.
The comparison is application-level and output-quality-controlled.

The protocol and its statistical interpretation are detailed in the
[P20 fairness analysis](../../../../docs/p20_fairness_and_speed_analysis.md).

## 14. Results

All method/baseline ratios below are candidate divided by baseline; lower is
faster. “Speedup” is the reciprocal ratio. A geometric mean aggregates cases
unless otherwise stated.

### 14.1 P18: strict linear-only accuracy milestone

P18 removed nonlinear fallback from the evaluated height-map path and repaired
capacity, ordering, leaf depth, and outgoing self-hit behavior. The strict
24-case snapshot passed accuracy and speed in 24/24 cases with a 0.5394×
geometric-mean frame ratio. A later 96-case expansion after the visibility
repairs was faster in 92/96 cases, with 0.7562× total, 1.1513× G-buffer, and
0.6124× path ratios; all primary geometry/position/normal and whole-image PSNR
gates passed, while 12 cases failed the dark proxy. These phase snapshots use
their preserved binaries and should not be combined numerically with P23.

Authoritative narrative: [P18 linear-only result](../../../../docs/p18_linear_only_accuracy_fix.md)
and [progress log](../../../../docs/progress.md).

### 14.2 P20: 30-pair diversity suite against Mode 0

P20 crosses five new geometries with six procedural 1K maps. It uses one frozen
environment assignment per geometry/map pair and the residual-gated automatic
scalar/first-order candidate.

| Metric | Candidate / Mode 0 | Speedup |
|---|---:|---:|
| Total frame, geometric mean | **0.5196×** | **1.92×** |
| Total frame, median case | 0.5096× | 1.96× |
| Total frame, best case | 0.4039× | 2.48× |
| Total frame, worst case | 0.7021× | 1.42× |
| G-buffer, geometric mean | 0.5771× | 1.73× |
| Path stage, geometric mean | 0.4847× | 2.06× |

All 30 cases are faster. Arithmetic mean frame times are 11.9916 ms for Mode 0
and 6.1312 ms for the candidate, saving 5.8603 ms. The path stage accounts for
4.6776 ms (79.8%) of that arithmetic-mean saving; G-buffer accounts for 1.1848
ms (20.2%). Twenty scalar-dispatched cases alone give 0.5414×, showing that the
dominant core benefit exists without first-order bounds.

Twenty-eight of 30 cases pass the full accuracy contract. All 30 pass primary
geometry, position, normal, beauty PSNR, and positive dark-leak gates. The two
exceptions are the woven-cylinder signed dark-bias proxy; the bias persists in
the scalar path and is not attributable to tagged first order. The frozen
artifacts are the [P20 report](../../../../experiments/p20_diversity/full_expansion_10x30/final_report.md),
[machine-readable summary](../../../../experiments/p20_diversity/full_expansion_10x30/summary.json),
and [suite documentation](../../../../docs/p20_diversity_suite.md).

### 14.3 P21: three-way Mode 0 / ours / TFDM

P21 reruns the same 30 P20 inputs with Mode 0, ours, and the no-presentation
TFDM timing artifact.

| Comparison | Total | G-buffer | Path | G-buffer + path |
|---|---:|---:|---:|---:|
| Ours / Mode 0 | **0.5210×** | 0.5796× | 0.4845× | 0.5068× |
| TFDM / Mode 0 | 1.1805× | 1.2683× | 1.0442× | 1.0979× |
| Ours / TFDM | **0.4413×** | **0.4570×** | **0.4640×** | **0.4616×** |

Ours is 1.92× faster than Mode 0 and 2.27× faster than TFDM in geometric-mean
total-frame time; it is faster than TFDM in 30/30 cases. The tighter rendering-
core comparison is 2.17×. Ours passes the primary geometry gate in 30/30 cases;
TFDM passes in 12/30, with 18 position and 16 normal failures. TFDM's maximum
mask mismatch is 0.01365%, but position p99 reaches $9.25\times10^{-4}$ and
normal-vector p99 reaches 1.112 on terraced trefoil. This is the reason the
speed result must retain the equal-input/not-equal-surface qualification.

Frozen evidence: [P21 report](../../../../experiments/p21_tfdm_baseline/full_p20_nopresent_10x30/report.md)
and [summary](../../../../experiments/p21_tfdm_baseline/full_p20_nopresent_10x30/summary.json).

### 14.4 P22: causal all-ray first-order ablation

P22 uses 15 cases (five geometries × three slope-dominant maps), 20 warm-ups,
50 measurements, and five full-render arms: Mode 0 plus SS, SF, FS, and FF.
Only the size-matched node-bound type changes among the four candidate arms.

| Causal measurement | Ratio | Interpretation |
|---|---:|---|
| Primary FO/scalar G-buffer, outgoing scalar | **0.8876×** | 11.24% lower primary time |
| Outgoing FO/scalar path, primary scalar | **0.9193×** | 8.07% lower path time |
| All-FO/all-scalar full frame | **0.9143×** | **1.094×** throughput; 14/15 faster |
| All-scalar/Mode 0 full frame | **0.5701×** | **1.75×** throughput from linear+DDA without FO |
| All-FO/Mode 0 full frame | **0.5213×** | **1.92×** final admitted controlled result |

Ten additional first-order-friendly pairs give 0.9010× primary FO/scalar,
0.9666× all-FO/all-scalar, and 0.4782× all-FO/Mode 0 (2.09×). Four adverse
rough maps give 0.9587× primary, but 1.0174× outgoing path and 1.0096× forced-FF
total. The adverse result justifies static dispatch instead of universal FO.

Between scalar and FO arms, the minimum beauty PSNR is 47.25 dB and maximum
primary mask delta is 0.01124%. Fourteen of 15 cases pass the absolute Mode 0
contract; the remaining twisted case has an inherited dark-region bias in both
scalar and FO. Frozen evidence: [P22 report](../../../../experiments/p22_all_ray_first_order/smooth_v2_full_20x50/final_report.md)
and [summary](../../../../experiments/p22_all_ray_first_order/smooth_v2_full_20x50/summary.json).

### 14.5 P23: complete 13×19 suite

P23 crosses all 13 benchmark geometries with all 19 displacement maps: 247
unique geometry/map pairs. The set contains 169 1K, 13 2K, and 65 4K cases.
Four environments are deterministically balanced 62/62/62/61, with exactly
one environment per pair.

| Metric | Result |
|---|---:|
| Total candidate / Mode 0 geometric mean | **0.6202× (1.61× speedup)** |
| Faster cases | **239/247** |
| G-buffer ratio | 0.6369× |
| Path ratio | 0.6025× |
| Median / best / worst total ratio | 0.6163× / 0.3706× / 1.6633× |
| Strict full accuracy passes | 225/247 |
| Combined speed + strict accuracy passes | 221/247 |
| Primary geometry + beauty gate passes | **247/247** |

All geometry groups are faster in geometric mean. Curved surface is most
favorable at 0.4720× and twisted quad least favorable at 0.8181×. Bunny,
Armadillo, and tapered cone/frustum are each faster in 19/19 pairs, at 0.6642×,
0.6517×, and 0.6216×. All 78 P20-map cross pairs are faster at 0.5680×; P18-map
pairs are 0.6458× and faster in 161/169. All 1K and 2K cases are faster, while
57/65 4K cases are faster.

Automatic dispatch uses all-ray tagged first order on 39 pairs, scalar adaptive
hierarchy on 108, and lean primary plus scalar outgoing traversal on 100. The
selected FO cohort is faster in 39/39 at a descriptive 0.5354×; this is not a
causal FO ratio.

Every case passes the declared primary mask, position, shading-normal, and
whole-image PSNR gates. Across the suite, minimum PSNR is 37.481 dB; maximum
mask mismatch is 0.06964%; maximum common-hit position p99 is
$9.73\times10^{-5}$; maximum normal p99 is 0.00544. The 22 strict exceptions
are signed dark-bias or positive-leak proxy failures, clustered on twisted and
rough/high-frequency cases.

The eight speed regressions are three flat-quad and five twisted-quad rough-4K
scalar cases. A longer 20×128 confirmation gives 1.2423× over that selected
cohort: G-buffer is still faster at 0.8593×, while path is 1.3413×. Quad rocky
trail is 1.6513× total, with 0.7437× G-buffer and 1.9025× path. The renderer did
not serialize actual outgoing-ray counts, so cost-per-identical-ray and a small
changed-ray-stream effect remain unresolved.

Frozen evidence: [P23 complete report](../../../../experiments/p23_complete_suite/full_auto_10x30/complete_report.md),
[summary](../../../../experiments/p23_complete_suite/full_auto_10x30/summary.json),
[performance statistics](../../../../experiments/p23_complete_suite/full_auto_10x30/performance_statistics.json),
and [complete-suite documentation](../../../../docs/p23_complete_suite.md).

![P23 performance matrix](../../../../figures/ray_tracing_paper/rt_p23_performance_matrix.png)

![P23 render agreement](../../../../figures/ray_tracing_paper/rt_p23_render_agreement.png)

### 14.6 P24–P30 artist and paper-beauty audits

The artist scenes add full PBR textures, tangent-space normal maps, HDR
environments, and finite area/point emitters while preserving a matched
intersection comparison. Purchased example images guide art direction only;
Mode 0 is the algorithmic accuracy reference.

#### Development history and accepted replacements

| Phase / scene | Mode 0 total | Ours total | Total speedup | G-buffer speedup | Path speedup | Beauty PSNR | Mask mismatch |
|---|---:|---:|---:|---:|---:|---:|---:|
| P24 organic, automatic | — | — | 1.351× | — | — | 52.56 dB | 0.000286% |
| P24 skulls, automatic | — | — | 1.741× | — | — | 31.36 dB | 0.051117% |
| P24 organic, forced all-FO | — | — | 1.483× | — | — | 52.56 dB | 0.000286% |
| P24 skulls, forced all-FO | — | — | 1.805× | — | — | 31.36 dB | 0.051117% |
| P25 organic, warm-light all-FO | — | — | 1.489× | — | — | 47.815 dB | — |
| P25 skulls, warm-light all-FO | — | — | 1.901× | — | — | 27.030 dB | — |
| **P26 organic, final** | — | — | **1.567×** | 1.390× | 1.626× | **48.352 dB** | 0.000286% |
| **P26 skulls, final visibility fix** | 19.165 ms | 11.246 ms | **1.704×** | 1.677× | 1.720× | **40.375 dB** | 0.051676% |
| P27 motherboard sphere, superseded by board | 18.726 ms | 11.923 ms | 1.571× | 1.411× | 1.634× | 46.723 dB | 0.001678% |
| P27 rock/grass sphere, superseded lighting | 29.678 ms | 18.037 ms | 1.645× | 1.445× | 1.697× | 49.764 dB | 0.000076% |
| **P28 motherboard board, final** | 15.760 ms | 14.766 ms | **1.067×** | 1.346× | 1.035× | **53.800 dB** | 0.035248% |
| **P28 rock/grass warm sphere, final** | 30.184 ms | 18.633 ms | **1.620×** | 1.443× | 1.660× | **49.457 dB** | 0.000076% |
| **P29 trefoil + woven threads** | 26.665 ms | 25.047 ms | **1.065×** | 1.652× | 1.184× | **54.447 dB** | 0.000153% |
| **P29 helicoid + cobble** | 17.142 ms | 9.117 ms | **1.880×** | 1.845× | 1.938× | **56.900 dB** | 0.001667% |
| **P29 torus + terraced bronze** | 52.551 ms | 28.085 ms | **1.871×** | 2.481× | 1.700× | **49.153 dB** | 0.000049% |

An em dash means that the cited phase summary did not promote that field as a
headline number; it must not be guessed. P24's two-scene geometric-mean speedup
is 1.534× for automatic dispatch and 1.636× for forced all-ray first order.
P25's warm-light forced-FO geometric mean is 1.682×. Those phases are useful
development evidence, but P26 supersedes their final organic/skull images.

The P26 skull study isolates a real curved-proxy visibility failure. At
visibility thresholds 0.050, 0.025, 0.010, and 0.005, the measured
PSNR/speedup pairs are 35.324 dB/1.654×, 36.511 dB/1.671×,
37.511 dB/1.662×, and 37.974 dB/1.615×. The selected 0.010 knee recovers most
quality without falling back to nonlinear intersection. The final 256-spp run
then reaches 40.375 dB at 1.704×.

P28 replaces the motherboard sphere with a literal two-triangle board and adds
the exact affine shell-ray specialization. It also establishes that a favorable
G-buffer speedup need not dominate a shader/path-heavy scene: the board's
1.346× G-buffer improvement becomes only 1.067× total. This is a useful
counterexample to intersection-only performance claims.

P29's three new paper scenes force first-order bounds for primary and outgoing
rays and use 256-spp hero pairs, path length three, and 10×30 timing. They are
accuracy-audited beauty images, not selected unpaired candidate renders.

Sources and frozen reports:

- [P24 material workflow](../../../../docs/artist_material_showcase.md),
  [automatic report](../../../../experiments/p24_artist_materials/final_hero_auto_10x30_256spp/report.md),
  and [forced-FO report](../../../../experiments/p24_artist_materials/final_hero_first_order_10x30_256spp/report.md);
- [P25 lighting workflow](../../../../docs/blender_artist_lighting.md) and
  [final report](../../../../experiments/p25_blender_lighting/final_first_order_10x30_256spp/report.md);
- [P26 organic study](../../../../docs/p26_organic_material_ablation.md),
  [organic report](../../../../experiments/p26_organic_material/combined_p26_v3_final_128spp/report.md),
  [P26 skull fix](../../../../docs/p26_skull_visibility_fix.md), and
  [skull report](../../../../experiments/p26_skull_shadow/final_fo_leaf_v001_10x30_256spp/report.md);
- [P27 motherboard](../../../../docs/p27_motherboard_artist_scene.md) and
  [report](../../../../experiments/p27_motherboard/final_cover_u025_rebuild_10x30_256spp/report.md);
- [P27 rock/grass](../../../../docs/p27_rockgrass_artist_scene.md) and
  [report](../../../../experiments/p27_rockgrass/final_first_order_v010_10x30_256spp/report.md);
- [P28 replacements](../../../../docs/p28_artist_board_warm_scenes.md) and
  [final report](../../../../experiments/p28_artist_board_warm/final_affine_strict_10x30_256spp/report.md);
- [P29 woven trefoil](../../../../docs/p29_trefoil_woven_beauty.md),
  [report](../../../../experiments/p29_trefoil_woven/final_10x30_256spp/report.md),
  [P29 cobble helicoid](../../../../docs/p29_helicoid_cobble_beauty.md),
  [report](../../../../experiments/p29_helicoid_cobble/final_ours_10x30_256spp/report.md),
  [P29 terraced torus](../../../../docs/p29_torus_terraced_beauty.md), and
  [report](../../../../experiments/p29_torus_terraced/final_10x30_256spp/report.md).

#### P30 undeformed base-mesh references

P30 renders the actual hardware-triangle proxies with displacement and PBR maps
disabled, neutral Lambert shading, and orange proxy edges. These are not
zero-amplitude displaced images. They expose how little base geometry supports
the final relief and follow the explanatory convention used by TFDM.

| Scene | Proxy triangles | Base reference | Displaced image (ours) |
|---|---:|---|---|
| Organic | 4,096 | [coarse](../../../../figures/paper_beauty_ours/coarse_organic_pink.png) | [beauty](../../../../figures/paper_beauty_ours/ours_organic_pink.png) |
| Skulls | 288 | [coarse](../../../../figures/paper_beauty_ours/coarse_skulls.png) | [beauty](../../../../figures/paper_beauty_ours/ours_skulls.png) |
| Motherboard | 2 | [coarse](../../../../figures/paper_beauty_ours/coarse_motherboard_board.png) | [beauty](../../../../figures/paper_beauty_ours/ours_motherboard_board.png) |
| Rock/grass | 4,096 | [coarse](../../../../figures/paper_beauty_ours/coarse_rockgrass.png) | [beauty](../../../../figures/paper_beauty_ours/ours_rockgrass.png) |
| Woven trefoil | 8,064 | [coarse](../../../../figures/paper_beauty_ours/coarse_trefoil_woven.png) | [beauty](../../../../figures/paper_beauty_ours/ours_trefoil_woven.png) |
| Cobble helicoid | 6,144 | [coarse](../../../../figures/paper_beauty_ours/coarse_helicoid_cobble.png) | [beauty](../../../../figures/paper_beauty_ours/ours_helicoid_cobble.png) |
| Terraced torus | 25,600 | [coarse](../../../../figures/paper_beauty_ours/coarse_torus_terraced_bronze.png) | [beauty](../../../../figures/paper_beauty_ours/ours_torus_terraced_bronze.png) |

The complete image inventory, hashes, and figure-use guidance are in the
[paper beauty README](../../../../figures/paper_beauty_ours/README.md). Exact
P30 commands and scene records are in the
[P30 report](../../../../experiments/p30_coarse_mesh_references/report.json).

### 14.7 P31 purchased-material shape expansion

P31 adds four purchased 4K displacement/PBR materials on four deliberately
different proxies: a rounded sci-fi reactor capsule, an open dry-branches
saddle, a rounded-square quilt cushion, and a tapered wood-block reliquary.
Every scene has an editable Blender lighting file, a renderer-exported
three-light rig, a matched Mode 0/candidate quality audit, a standalone 256-spp
candidate image, and a separately rendered undeformed proxy reference.

Some art-direction runs overlapped on the same GPU, so paper-facing timing was
repeated afterward as a strictly sequential, uncontended 1280x1024 sweep. Each
arm uses 10 warm-up and 30 measured frames, path length three, the same scene
and executable, and no candidate-only arguments. All four candidates use
tagged first-order primary bounds, first-order outgoing bounds, event-segmented
linear shell rays, and front-to-back DDA without nonlinear fallback.

| Scene | Mode 0 | Ours | Speedup | Beauty PSNR | Mask mismatch |
|---|---:|---:|---:|---:|---:|
| Sci-fi reactor | 17.635 ms | 9.705 ms | **1.817x** | 47.080 dB | 0.000229% |
| Dry-branches ground | 24.132 ms | 11.169 ms | **2.161x** | 50.719 dB | 0.028763% |
| Quilt cushion | 16.116 ms | 10.135 ms | **1.590x** | 51.884 dB | 0.000076% |
| Wood-block reliquary | 17.921 ms | 10.877 ms | **1.648x** | 37.212 dB | 0.011063% |

The four-scene geometric-mean speedup is **1.791x** overall, **1.684x** in the
G-buffer, and **1.835x** in path tracing. These artist-directed cases expand
shape and material diversity but do not replace the 247-pair P23 aggregate.
The wood-block case is the weakest quality example: its small position/normal
tails coexist with sparse high-contrast visibility differences, so its
37.212-dB result should be shown with the registered difference image rather
than described as exact agreement. Full provenance is in the
[P31 summary](../../../../docs/p31_purchased_beauties_summary.md) and
[uncontended timing directory](../../../../experiments/p31_uncontended_sequential_10x30_1280x1024).

## 15. Ablations and mechanism evidence

### 15.1 Where the full speedup comes from

The evidence supports the following decomposition:

1. Analytic event construction moves pole/turn/proxy-boundary reasoning ahead
   of the inner grid walk.
2. Error-targeted chords turn the nonlinear shell curve into small affine
   traversal problems.
3. Global front-to-back ordering lets a first hit shorten or eliminate later
   chord work.
4. Hierarchical DDA exploits adjacent cells and skips blocks instead of
   restarting nonlinear node queries.
5. Linear leaf representations and ray-family specialization avoid nonlinear
   leaf solving.
6. Tagged first-order bounds remove predictable height slope on admitted maps
   and skip additional work.

P20's scalar cohort (1.85× versus Mode 0) and P22's matched SS result (1.75×)
show that items 1–5 dominate. P22's FF/SS ratio adds 1.094× on the controlled
admitted cohort, establishing item 6 as real but conditional.

### 15.2 First-order work counters

The counter-enabled binary was run separately from timing. Counts are per
primary candidate invocation:

| Case | Scalar DDA | FO DDA | DDA reduction | Scalar hierarchy | FO hierarchy | Hierarchy reduction |
|---|---:|---:|---:|---:|---:|---:|
| Torus + directional | 2.42 | 1.77 | **26.9%** | 2.07 | 1.57 | **24.2%** |
| Trefoil + swept ridges | 1.44 | 1.06 | **26.4%** | 1.17 | 0.88 | **24.8%** |

Thus instrumentation confirms that primary first-order bounds reduce DDA steps
by approximately **26%** and hierarchy tests by **24–25%**. The counters support
the pruning mechanism; they are not standalone performance ratios.

### 15.3 Negative and corrective ablations

- P15's early general first-order opportunity studies and the older certified
  reference-line residual clipping were weak/negative on broad ordinary
  workloads. The current positive result is narrower: a packed, residual-gated,
  equal-size hierarchy on slope-dominant maps in the event-segmented GPU
  architecture. See [P15](../../../../docs/p15_first_order_hierarchy.md).
- P19 outgoing-only FO gives 0.9505× scalar (1.052×), 14/15 faster and
  byte-exact output. P22 supersedes it with all-ray 0.9143× (1.094×).
- Forced FO on cobble measured 1.0142× scalar; low-gradient quantized maps gave
  1.0053×. Both justify the residual gate.
- Four adverse rough maps regress to 1.0096× under forced FF even though primary
  remains 0.9587×; outgoing arithmetic fails to amortize.
- Strict overflow-to-miss caused twisted secondary/visibility failures.
  Bounded relaxation/retry fixed accuracy; exact paged streaming improved the
  threshold but took 29.527 ms and remains disabled.
- P17's fold-aware domain repair recovered shadow coverage but retained exact
  nonlinear primary refinement. P18 then removed that refinement and rebuilt
  accuracy with a strict linear leaf representation.
- P26's visibility-only threshold sweep identifies 0.010 as the local-light
  quality/performance knee and validates planar-first local linear correction.
- P28's motherboard distinguishes the exact affine optimization from the
  general algorithm and shows that path/shading can bound the final gain.

## 16. Qualitative figures and suggested paper placement

| Paper role | Preferred asset | Notes |
|---|---|---|
| Method teaser | [overview PDF](../../../../figures/ray_tracing_paper/rt_method_overview.pdf) | Proxy shell → events/chords → DDA → hit |
| Segmentation explanation | [events/chords/DDA PDF](../../../../figures/ray_tracing_paper/rt_events_chords_dda.pdf) | Use beside Algorithms 1–2 |
| Shared-representation mechanism | [first-order PDF](../../../../figures/ray_tracing_paper/rt_first_order_mechanism.pdf) | Scalar range versus plane/residual slab |
| Broad performance | [P23 matrix PDF](../../../../figures/ray_tracing_paper/rt_p23_performance_matrix.pdf) | Shows speed regimes and regressions |
| Accuracy | [P23 agreement PDF](../../../../figures/ray_tracing_paper/rt_p23_render_agreement.pdf) | Paired representative render/AOV evidence |
| Hero row | [standalone beauty directory](../../../../figures/paper_beauty_ours/README.md) | Use individual unlabelled ours images, not montages |
| Tessellation-free input explanation | [P30 coarse report](../../../../experiments/p30_coarse_mesh_references/report.json) and [P31 coarse report](../../../../experiments/p31_coarse_mesh_references/report_p31.json) | Place coarse proxy beside displaced beauty |
| New material quartet | [P31 summary](../../../../docs/p31_purchased_beauties_summary.md) | Sci-fi/reliquary above, quilt/branches below; capsule and cushion are the clearest base/result pairs |

The PNG versions are convenient for Markdown and slides; use PDF/SVG in the
paper. The source generator is
[generate_ray_tracing_paper_figures.py](../../../../scripts/generate_ray_tracing_paper_figures.py).

## 17. Limitations and known failures

1. **Not certified relative to Mode 0.** Midpoint chord error, the fast
   visibility envelope, finite leaf refinement, and overflow relaxation do not
   prove a global supremum error or exact hit equivalence.
2. **Dark-region residuals remain.** P23 has 22/247 strict proxy exceptions even
   though every primary geometry and PSNR gate passes. High-contrast visibility
   makes tiny blocker changes perceptually important.
3. **Rough 4K flat/twisted outgoing regressions.** Eight P23 cases are slower;
   path work, not G-buffer work, is the bottleneck. Actual secondary and
   visibility ray counts were not serialized.
4. **First order is regime-dependent.** Rough residuals can erase plane
   tightness while retaining packing/fetch/arithmetic cost. Automatic dispatch
   is part of the method, not an incidental tuning trick.
5. **TFDM is not equal-surface.** P21 supports equal-input throughput against
   the in-repository two-triangle implementation, not universal equal-quality
   superiority.
6. **Steady-state timing omits construction.** Height hierarchy, proxy AS,
   texture loading, OptiX compilation, readback, and output are outside the
   frame timer. A complete system paper still needs memory/build/update data.
7. **Hardware breadth is limited.** Reported aggregate runs use the recorded
   RTX 5090/driver environment. Independent GPUs, drivers, and repeated-run
   confidence intervals remain open.
8. **Dataset dependence.** P20 is a designed procedural expansion and P23 is the
   complete cross product of repository assets, not an untouched production
   holdout. Artist assets improve realism but remain a small set.
9. **Surface class.** The method assumes scalar, single-valued displacement over
   a valid triangle shell. Vector displacement, arbitrary target conversion,
   severe shell singularity, and topology-changing relief are out of scope.
10. **Renderer/material limits.** The organic material uses opaque GGX/diffuse
    shading, not true BSSRDF or transmission transport. Artist references are
    not physical ground truth.
11. **Missing baselines/ablations.** Dense tessellation, DMM, RMIP/PDM variants,
    isolated event/order/DDA component switches, hierarchy memory/build cost,
    and capacity-quality sweeps are not yet publication-complete.
12. **Novelty audit.** The package is a candidate contribution; “novel” or
    “first” still requires a focused prior-art comparison.

## 18. Reproduction and artifact map

Run commands from the repository root. The frozen P23 output directory is an
evidence artifact: use `--report-only` to rebuild its summaries, or choose a new
output directory for new measurements. Do not overwrite the preserved run.

### 18.1 Complete-suite validation and reports

```powershell
& .\.conda-figures-clean\python.exe scripts\run_p23_complete_all_ray_suite.py --validate-only
& .\.conda-figures-clean\python.exe scripts\run_p23_complete_all_ray_suite.py --report-only
```

The first command validates the 13×19 manifest and assets. The second regenerates
the report and contact sheets from existing measurements without rendering. To
run a new subset safely, follow the command template in the
[P23 documentation](../../../../docs/p23_complete_suite.md#commands) and give it
a new `--output` directory.

### 18.2 Causal first-order ablation

```powershell
& .\.conda-figures-clean\python.exe `
  scripts\benchmark_p22_all_ray_first_order_ablation.py `
  --exe experiments\p22_all_ray_first_order\artifacts\p22_all_ray_first_order_20260906\nrtdsm.exe `
  --suite experiments\p19_first_order_gpu\smooth_map_suite_v2.json `
  --cases all `
  --output experiments\p22_all_ray_first_order\my_full_run `
  --warmup 20 --measured 50
```

This is the five-arm Mode 0 / SS / SF / FS / FF experiment used to attribute
the incremental first-order benefit. A new output directory is intentional.

### 18.3 TFDM validation and paper assets

```powershell
& .\.conda-figures-clean\python.exe scripts\benchmark_p21_tfdm_baseline.py --validate-only
& .\.conda-figures-clean\python.exe scripts\generate_ray_tracing_paper_figures.py
& .\.conda-figures-clean\python.exe scripts\render_paper_coarse_meshes.py --validate-only
```

The first command checks the common 30-case TFDM-eligible manifest. The latter
two regenerate/validate the explanatory figures and all eleven undeformed proxy
references. Exact artist-scene rendering commands live in the P26–P31 documents
linked in Sections 14.6–14.7; keeping them there avoids silently simplifying their
camera, material, light, and sampling contracts.

### 18.4 Manuscript build

From `paper_dmap_query`, run a LaTeX engine such as:

```powershell
tectonic ray-tracing-only-draft.tex
```

The source is the maintained
[ray-tracing-only LaTeX draft](../../../../paper_dmap_query/ray-tracing-only-draft.tex),
and the current compiled artifact is
[ray-tracing-only-draft.pdf](../../../../paper_dmap_query/ray-tracing-only-draft.pdf).
The Markdown chapter is the detailed project record; the LaTeX file is the
publication-facing condensation. When a number changes, update the frozen
report first, then this chapter, and finally the LaTeX text/table that cites it.

## 19. Claim ledger

| Status | Statement | Required qualification / evidence |
|---|---|---|
| Supported | The complete automatic candidate is 1.61× faster than nonlinear Mode 0 in P23 geometric-mean frame time. | 247 unique pairs; 239/247 individual wins; same full-render workload; P23 frozen report. |
| Supported | Linear segments + ordered DDA provide most of the speedup. | P22 all-scalar/Mode 0 is 0.5701× (1.75×); do not credit this part to first order. |
| Supported, conditional | Tagged first-order bounds add 1.094× throughput over scalar bounds. | P22 admitted slope-dominant cohort; 14/15 faster; automatic scalar dispatch remains necessary on rough maps. |
| Supported | Primary first-order bounds reduce measured DDA work by about 26% and hierarchy tests by 24–25%. | Separate counter-enabled binary; mechanism evidence, not a timing number. |
| Supported with scope | Ours is 2.27× faster than the in-repository standard TFDM renderer on the equal-input P21 set. | TFDM represents a different local surface and passes the Mode 0 primary geometry gate in only 12/30 cases; this is not equal-surface dominance. |
| Supported | The evaluated strict candidate handles primary, secondary-closest, and visibility rays without calling the nonlinear Mode 0 solver. | Keep the preserved build configuration and distinguish linear local corrections from nonlinear fallback. |
| Not supported | The chord construction or complete renderer is certified/exact relative to Mode 0. | Current midpoint/error-envelope safeguards are approximate and quality-controlled. |
| Not supported | First order explains the complete renderer speedup or is faster on every map. | Rough residuals and the P22 adverse cohort disprove the universal version. |
| Not supported yet | Universal superiority to TFDM, DMM, dense tessellation, RMIP, or PDM. | Requires matched representations, quality, memory, construction cost, and additional baselines. |
| Not supported yet | The method or representation is the first of its kind. | Requires a focused, current prior-art and novelty audit. |

### 19.1 Interpretations, not direct measurements

- The P22 SS/Mode 0 and FF/SS decomposition supports the interpretation that
  ray linearization, ordering, DDA, and linear leaves cause most of the gain,
  while first-order bounds are a smaller supporting accelerator.
- The compensated-height derivation and work counters support the interpretation
  that first order helps by removing predictable local slope.
- P23's eight slow cases have faster G-buffers and slower path stages, suggesting
  an outgoing scalar-DDA bottleneck. Missing ray counts prevent attributing it
  uniquely to cost per identical ray.
- Dark-proxy exceptions are consistent with inherited chord/leaf approximation:
  most affected cases are scalar and all primary geometry gates pass. This does
  not prove a unique error source for every pixel.

### 19.2 Open work

- Serialize per-family ray counts and normalize traversal work for P23's eight
  regressions.
- Report preprocessing time, hierarchy memory, update cost, and first-frame
  latency; current timings are steady state.
- Add matched dense tessellation, DMM, RMIP/PDM where feasible, and an explicit
  equal-input/equal-quality boundary for every external baseline.
- Repeat on more GPUs, views, independent trials, and untouched production
  assets.
- Add isolated event/order/DDA/leaf-depth/capacity/visibility-correction
  ablations.
- Either certify the chord envelope or retain the current measured-error and
  quality-controlled claim.
- Complete a focused prior-art audit before using “novel” or “first.”

## 20. Interface to the three-application paper

The maintained paper organization is:

1. **Application 1 — Area/Sampling.** Intrinsic surface-measure queries consume
   first-order height/gradient information. Its evidence and limitations must
   remain in its own application chapter; this ray result does not validate it.
2. **Application 2 — Ray Tracing (this chapter).** Extrinsic visibility queries
   consume the tagged height-plane/residual hierarchy inside event-segmented,
   globally ordered DDA traversal.
3. **Application 3 — pending.** No algorithm or performance claim should be
   assigned until the query, shared fields, baselines, and evaluation gates are
   frozen.

For the eventual unified manuscript, maintain two layers of attribution:

- **Shared representation:** coordinate convention, local first-order model,
  residual/uncertainty semantics, conservativeness guarantees, storage,
  preprocessing, build/update cost, and memory comparison.
- **Application consumer:** query-specific traversal, estimators or leaf tests,
  correctness/quality gates, baselines, timings, and failure modes.

The ray application currently uses a compact tagged plane-plus-residual record;
other applications may require additional gradient uncertainty. The paper must
say which fields are physically shared, derived, or application-specific rather
than implying that every consumer reads an identical byte layout. Once all
three applications are ready, the umbrella results section should contain a
single cross-application storage/build table plus one causal ablation per
consumer. Until then, this file is the source of truth for Application 2.

## 21. Source provenance and maintenance

The authoritative narrative chain is the
[LaTeX draft](../../../../paper_dmap_query/ray-tracing-only-draft.tex),
[progress log](../../../../docs/progress.md),
[P14](../../../../docs/p14_fast_segment_dda.md),
[P15](../../../../docs/p15_first_order_hierarchy.md),
[P16](../../../../docs/p16_adaptive_hdda.md),
[P17](../../../../docs/p17_accuracy_fix.md),
[P18 linear-only result](../../../../docs/p18_linear_only_accuracy_fix.md),
[P18 claim audit](../../../../docs/p18_method_claim_audit.md),
[P19](../../../../docs/p19_first_order_ray_tracing.md),
[P20 suite](../../../../docs/p20_diversity_suite.md),
[P20 fairness analysis](../../../../docs/p20_fairness_and_speed_analysis.md),
[P21](../../../../docs/p21_tfdm_baseline.md),
[P22](../../../../docs/p22_all_ray_first_order.md), and
[P23](../../../../docs/p23_complete_suite.md). P19 is retained as a superseded
outgoing-only result; P22 supplies the current all-ray causal number.

Artist-scene provenance is in the P26–P29 notes linked in Section 14.6; P30 is
represented by the [machine-readable report](../../../../experiments/p30_coarse_mesh_references/report.json),
[render script](../../../../scripts/render_paper_coarse_meshes.py), and
[image inventory](../../../../figures/paper_beauty_ours/README.md). P31 adds the
[purchased-material inventory](../../../../docs/p31_purchased_material_inventory.md),
[integrated scene report](../../../../docs/p31_purchased_beauties_summary.md),
[uncontended timing artifacts](../../../../experiments/p31_uncontended_sequential_10x30_1280x1024),
and [four-scene coarse report](../../../../experiments/p31_coarse_mesh_references/report_p31.json).

Maintenance rules:

1. Attach each number to its frozen executable/protocol; add a new result rather
   than silently transferring an old result to a new binary.
2. Keep ratios oriented candidate/baseline and give reciprocal speedups.
3. Keep measured facts, interpretations, and open work visibly separate.
4. Preserve the current App 1 Area/Sampling, App 2 Ray Tracing, App 3 pending
   numbering unless the user explicitly changes the unified outline.
5. Revalidate all local links whenever this note or an artifact moves.
6. Edit the LaTeX independently; this Markdown file does not auto-synchronize it.
