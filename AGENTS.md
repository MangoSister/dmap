# Project instructions

This file is the single instruction file for every coding agent (Codex, Claude Code, and others). `CLAUDE.md` only imports it. Edit this file, not `CLAUDE.md`.

This repository holds a research vault (`obs/`, Obsidian) and, later, prototype code for the project "Conservative metric queries without tessellation".

Master plan (Yang): `obs/60 Projects/Project — Conservative metric queries without tessellation.md`
Running log (Yang): `obs/60 Projects/Log — Conservative metric queries.md`

## Document ownership (hard rule)

Two people keep notes in `obs/`, on different but overlapping parts of the project.

- `obs/bing/` belongs to Bing. Bing's entry point is `obs/bing/60 Projects/Index — 60 Projects.md`.
- Everything else under `obs/` belongs to Yang. The master plan and log linked above are Yang's.

Rules for any agent working in this repository:

- Work only inside the current user's area. Do not read, cite, edit, or link to the other user's notes unless the user explicitly asks for it. When the user does ask, say which notes you read.
- Never edit the other user's notes. If the user needs a change there, describe it and let the owner apply it.
- When creating a note, do not reuse a basename that already exists in the other user's area. Obsidian resolves `[[wikilinks]]` by basename across the whole vault, so a duplicate basename makes links ambiguous.
- Identify the current user before touching `obs/`. Run `git config user.name`. `MangoSister` is Yang. `Bing Xu` is Bing. Any other value: ask the user which area is theirs before editing anything.

## Document organization (hard rule)

- The master plan is stateless and always current. When a decision changes, rewrite the affected text in place. Never append "UPDATE:" notes to it.
- The only fast-changing content in the master plan is the `## Status` block at the top. Keep it short and edit it in place.
- The log note is append-only, reverse-chronological, dated. Every meaningful work session, measurement, or decision gets an entry there. Record the reasoning in the log; put the resulting plan change in the master plan.
- Experiment results go into their own notes, linked from the plan and the log. Do not paste result data into the plan.

## Coding style (hard rule, unless explicitly instructed otherwise)

- Prioritize clarity over aggressive optimization. Clear, modular, educational code.
- Small functions that map one-to-one to the formulas in the notes. Name variables after the math (`G0`, `B0`, `C0`, `a`, `gh`).
- Comments only for what the code cannot say. Cite the note and section for non-obvious formulas.
- Each experiment script answers one question and prints its verdict.
- On Yang's machine, all Python for this project runs in the conda env `dmap`. `conda` is not on PATH in that shell; invoke the interpreter directly: `C:\Users\yyp05\miniconda3\envs\dmap\python.exe`. On other machines, ask the user which Python environment to use.

## Writing and talking style (hard rule, applies to all output)

Applies to conversation, notes, documentation, commit messages, and code comments alike.

- Use simple, plain vocabulary. Avoid over-engineered sentences and decorated or rhetorical wording.
- In technical and math writing, use standard technical terms. Do not invent words or coin new names when a standard term exists.
- Avoid excessive em-dashes. Avoid repetitive summaries. Avoid buzzwords.
- Avoid long, nested sentences. Prefer short, clear sentences with tight logical connection.
- Do not abbreviate or nickname previously introduced concepts or terms. State the full term. Common, widely adopted technical abbreviations (e.g. PDE, BVH, CDF) are fine.
