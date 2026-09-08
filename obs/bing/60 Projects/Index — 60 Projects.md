---
title: Index — 60 Projects
tags: [index, project-navigation, audit, provenance]
status: current
created: 2026-09-07
updated: 2026-09-07
---

# Index — 60 Projects

> [!important] Current paper map
> The user-facing order is **Application 1: Area/Sampling**,
> **Application 2: Ray Tracing**, and **Application 3: pending**. Start with
> [[Project — Conservative first-order queries on displacement maps]], then the
> [[Guide — Eurographics paper draft]]. The maintained ray chapter is
> [[Paper — Application 2 — Tessellation-free ray tracing]]. Older ray-first
> numbering is historical.

## Purpose

This index is the non-destructive organization layer for `60 Projects`. The
directory contains 45 pre-index research notes spanning multiple stopped,
negative, superseded, and successful experiment lines. They form a provenance
graph rather than 45 current plans. This index records which notes are current,
which remain authoritative only for a frozen phase, and which are retained as
superseded history.

No research note was deleted during the 2026-09-07 audit. The six explicitly
stopped/deprecated architecture notes were moved, with their original filenames,
to `Archive/Stopped and deprecated/`. Their basenames are unique and all inbound
references use Obsidian wikilinks, so those links continue to resolve. Frozen
evidence and merely superseded plans remain in the root because they are still
used heavily for scientific provenance. Several “active” front-matter values
are stale relative to later terminal notes; the classification below is the
current interpretation.

## Status legend

| Code | Meaning |
|---|---|
| **C** | Current maintained entry point. |
| **F** | Frozen phase evidence or terminal decision; authoritative for that phase, not current project state. |
| **S** | Superseded/intermediate plan or handoff; retain for chronology and implementation rationale. |
| **D** | Explicitly stopped, failed, deprecated, or archived architecture. |
| **L** | Legacy proposal with a materially different representation/scope. |

“Inbound before index” counts links that existed before this index linked every
note. Zero does not mean useless; it means the note was an orphan in the flat
vault. Every row below is now directly navigable.

## Current entry points

| Note | Role |
|---|---|
| [[Project — Conservative first-order queries on displacement maps]] | Umbrella history, decisions, and current application map. Read the dated top callout before older body text. |
| [[Guide — Eurographics paper draft]] | Maintained authoring/evidence guide. |
| [[Paper — Application 2 — Tessellation-free ray tracing]] | Current comprehensive Application 2 chapter through P33. |

There is not yet a maintained Application 1 chapter in this directory. The S1–S3
notes document a completed negative/stopped sampling line and must not be
mistaken for a new positive Application 1 result. Application 3 is intentionally
unassigned.

## Full audited inventory

### Current maintained notes

| Code | Inbound before index | Note | Audit interpretation |
|---|---:|---|---|
| C | 11 | [[Guide — Eurographics paper draft]] | Current writing guide; top application-order callout supersedes older ray-first prose. |
| C | 1 | [[Paper — Application 2 — Tessellation-free ray tracing]] | Current ray chapter; direct source of truth through P33. |
| C | 15 | [[Project — Conservative first-order queries on displacement maps]] | Current umbrella/history; later top callout and P18–P33 evidence supersede earlier decisions inside the long note. |

### Frozen reference, decision, and evidence records

| Code | Inbound before index | Note | Audit interpretation |
|---|---:|---|---|
| F | 2 | [[Note — Final ray application decision after Mode 1 recovery]] | Terminal result for the pre-P14 Mode 1 recovery line; later Application 2 method supersedes it as current state. |
| F | 0 | [[Note — R1 full-renderer result for draft]] | Orphaned phase handoff/evidence snapshot; retain for the R1 result only. |
| F | 0 | [[Note — S3 final decision for EG draft]] | Terminal area-sampling decision; important negative evidence despite no pre-index backlinks. |
| F | 6 | [[Plan — First-order ray representation oracle]] | Completed negative oracle on the older fixed topology; does not invalidate later residual-gated P22 result. |
| F | 3 | [[Plan — O2 on-device ordered shell DDA]] | Frozen isolated-query negative/performance record; later full renderer follows a different accepted path. |
| F | 2 | [[Plan — P-D0 practical displacement dataset]] | Completed dataset specification/evidence. |
| F | 5 | [[Plan — P-D1 certified tube-supercover DDA reference]] | Completed certified CPU/reference result; do not transfer “certified” to current GPU chords. |
| F | 4 | [[Plan — P-D2 residual clipping ablation]] | Completed negative residual-clipping decision although front matter still says `active`. |
| F | 1 | [[Plan — R0 hybrid ray rescue oracle]] | Complete gate failure; retained negative evidence. |
| F | 4 | [[Plan — S1 conservative area bounds]] | Phase completed later despite `planned-before-implementation` front matter. |
| F | 3 | [[Plan — S2 area sampling and PDF audit]] | Phase completed later despite stale planning status. |
| F | 3 | [[Plan — S3 area sampling quality and cost decision]] | Completed gate failure; terminal evidence for that sampling architecture. |
| F | 4 | [[Plan — Step 1 controlled ray generator]] | Completed controlled-corpus record. |
| F | 2 | [[Plan — Step 1 dataset loader]] | Completed dataset-loader record. |
| F | 2 | [[Plan — Step 1 fixed traversal decision run]] | Completed decision-run record. |
| F | 2 | [[Plan — Step 1 gate report]] | Completed gate report. |
| F | 2 | [[Plan — Step 1 oracle integration]] | Completed exhaustive-oracle record. |
| F | 6 | [[Plan — Step 1 persistence and instrumentation]] | Completed in later project history; front matter `active-p2-next` is stale. |
| F | 16 | [[Plan — Step 1 ray reference experiment]] | Frozen specification followed by completed implementation; high backlink count makes it a key historical anchor. |
| F | 5 | [[Report — A2 same-surface GPU comparison]] | Frozen negative/mixed report; authoritative only for its exact A2 artifact. |

### Superseded plans and handoff snapshots

| Code | Inbound before index | Note | Audit interpretation |
|---|---:|---|---|
| S | 0 | [[Note — EG draft handoff after O2 end-to-end]] | Intermediate handoff superseded by later renderer work. |
| S | 0 | [[Note — EG draft handoff after ordered closest-hit O0]] | Intermediate handoff; orphaned before this index. |
| S | 2 | [[Note — EG draft handoff after P-D1]] | Historical certified-reference handoff, not the current approximate GPU method. |
| S | 1 | [[Note — EG draft handoff after separator T1]] | Intermediate stopped-path handoff. |
| S | 0 | [[Note — EG draft handoff after T1.5 timing]] | Intermediate handoff superseded by ordered closest-hit/O2 work. |
| S | 1 | [[Note — EG draft handoff after topology T0]] | Intermediate stopped-path handoff. |
| S | 4 | [[Plan — A1 GPU mathematical port and differential tests]] | Large historical implementation plan; downstream A2 stop gate closes this line. |
| S | 1 | [[Plan — Historical Mode 1 correctness repair]] | Historical repair plan; the title already marks its scope. |
| S | 0 | [[Plan — Mode 1 certified tube-supercover integration]] | Intermediate integration plan, correctness/cost stage complete but noncompetitive. |
| S | 1 | [[Plan — Mode 1 performance recovery]] | Large recovery log; later terminal decision and P14–P33 line supersede current status. |
| S | 0 | [[Plan — O0.5 event-local height contraction]] | Passed intermediate gate; superseded by later ordered traversal stages. |
| S | 9 | [[Plan — Practical first-order DDA ray traversal]] | Historical decision plan with high reuse; not the current measured GPU chapter. |
| S | 11 | [[Plan — Ray application method, prototype, and baselines]] | Early prototype umbrella; useful derivations, superseded experimental state. |
| S | 4 | [[Plan — T1.5 measured separator-DDA performance]] | Explicitly superseded for closest-hit. |
| S | 7 | [[Plan — T1.6 ordered closest-hit shell DDA]] | Passed intermediate oracle; later O2 result closes the pending device step. |

### Explicitly stopped/deprecated architectures

| Code | Inbound before index | Note | Audit interpretation |
|---|---:|---|---|
| D | 1 | [[Plan — A2 isolated CERT-DDA-MM implementation]] | Predictive gate failed and frozen. |
| D | 4 | [[Plan — Area and product sampling application]] | Stopped at S3 gate; keep as the umbrella for that negative application line. |
| D | 16 | [[Plan — Certified ray architecture comparison]] | A2 stop gate triggered; heavily referenced historical architecture. |
| D | 3 | [[Plan — R1 full-renderer validation of ordered shell DDA]] | Explicitly archived; Mode 6 removed. |
| D | 3 | [[Plan — T1 separator-sign shell-ray certificate]] | Stopped on structural gates. |
| D | 3 | [[Plan — Topology-preserving shell-ray DDA]] | Stopped at predictive gate. |

### Legacy scope

| Code | Inbound before index | Note | Audit interpretation |
|---|---:|---|---|
| L | 0 | [[Project — Triangle-proxy displaced CC ray tracing]] | Separate early Catmull–Clark/proposal scope; not the current triangle-shell Application 2 representation. |

## Duplicate and overlap audit

- **Byte-identical duplicates:** none among the 45 pre-index Markdown files
  (SHA-256 grouping found zero duplicate groups).
- **Semantic overlap:** the six `EG draft handoff after ...` notes intentionally
  restate successive evidence cutoffs; they are snapshots, not accidental
  duplicates.
- The umbrella project and writing guide overlap in status summaries but serve
  distinct navigation and authoring roles.
- The new Application 2 chapter consolidates P14–P33 for current writing. It
  does not make earlier certified-reference or negative-result notes deletable,
  because their assumptions and claim boundaries differ.

## Link audit

Before this index was added, the 45-note vault contained a dense wikilink graph.
Notable high-inbound anchors were the umbrella project, the Step 1 reference,
the certified architecture comparison, the early ray-method plan, and the
writing guide. Several phase handoffs were genuine orphans; this index makes
them discoverable without rewriting their scientific content.

The audit fixed one malformed multiline wikilink in the writing guide:
`Paper — Application 2 — Tessellation-free ray tracing` is now a single valid
wikilink. All local Markdown path links resolve after that repair.

A directory-only check initially made 24 references to ten literature/concept
notes look unresolved. A full `dmap` vault check over 118 Markdown notes and all
vault assets resolves every outbound wikilink from `60 Projects`, including
those literature notes. All 105 local Markdown-path link occurrences from this
directory also resolve. No project note is orphaned after adding this index.
The broader vault has one duplicate basename (`README` in two locations), which
does not collide with any `60 Projects` note.

## Ambiguous or stale metadata left in place

The following notes have front matter that predates their downstream outcome:

- [[Plan — P-D2 residual clipping ablation]] says `active`, but the umbrella
  project records its completed negative decision.
- [[Plan — S1 conservative area bounds]] and
  [[Plan — S2 area sampling and PDF audit]] still say
  `planned-before-implementation`, although subsequent records say they ran.
- [[Plan — Step 1 persistence and instrumentation]] says `active-p2-next`, and
  [[Plan — Step 1 ray reference experiment]] says
  `specification-frozen-before-implementation`; the later Step 1 chain is
  complete.
- [[Plan — A1 GPU mathematical port and differential tests]] still says
  `active-device-differential`, but the downstream A2 architecture stopped.
- [[Plan — Practical first-order DDA ray traversal]] says `active`; later oracle
  and GPU evidence narrowed/superseded its claims.
- [[Note — EG draft handoff after P-D1]] says `active`; it is now a historical
  evidence-cutoff snapshot.

These values were not batch-rewritten because the original status is useful
chronology and some notes mix plan and result. This index provides the current
interpretation without falsifying their contemporaneous metadata.

## Physical archive status

The first conservative archive batch was applied on 2026-09-07:

See [[Archive — Stopped and deprecated project lines]] for the archive warning
and contents.

```text
60 Projects/
  Archive/
    Stopped and deprecated/
      Plan — A2 isolated CERT-DDA-MM implementation.md
      Plan — Area and product sampling application.md
      Plan — Certified ray architecture comparison.md
      Plan — R1 full-renderer validation of ordered shell DDA.md
      Plan — T1 separator-sign shell-ray certificate.md
      Plan — Topology-preserving shell-ray DDA.md
```

This batch is navigation-only: no note content was removed, filenames were
preserved, no local path-style links originate in the moved notes, and no script
references their old paths. Repository-wide link validation is required after
every future archive batch.

## Possible later archive batches (not applied)

If physical folders become desirable, use `git mv` in the nested `dmap`
repository and validate after every batch:

```text
60 Projects/
  Index — 60 Projects.md
  Project — Conservative first-order queries on displacement maps.md
  Guide — Eurographics paper draft.md
  Paper — Application 2 — Tessellation-free ray tracing.md
  Archive/
    2026-08 Step 1 and certified reference/
    2026-08 Scalar CERT-DDA A-line/
    2026-08 Mode 1 recovery and O-T-R line/
    2026-08 Area sampling S1-S3/
    Legacy proposals/
```

Do not move the frozen or superseded groups in bulk until a maintained
Application 1 chapter exists. Obsidian basename links would generally survive,
but relative Markdown paths, external links, scripts, and human bookmarks need
a repository-wide validation. Keeping those remaining notes physically flat is
the safer current choice.

## Maintenance rules

1. New current paper/application notes go in the root and must be added to
   “Current entry points.”
2. A plan stays in the inventory after completion; update this index's label
   rather than deleting negative evidence.
3. Use one maintained chapter per application. Handoff notes are immutable
   evidence snapshots, not alternate current drafts.
4. Before moving a note, check wiki and Markdown backlinks repository-wide,
   preserve basename uniqueness, use a history-preserving move, and validate
   both link styles afterward.
5. Keep historical numbering explicitly historical. Current numbering changes
   only on the user's instruction.
