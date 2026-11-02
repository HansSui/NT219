---
name: repo-memory
description: Persistent project memory kept in docs/memory/ inside the repo. Use at the START of any build, feature, refactor or bugfix task in a repo (read memory first, then propose improvements and a one-shot plan) and at the END of the task (write back what was learned). Also use when the user says "init memory", "remember this", or "update memory".
---

# repo-memory

A repo-local brain. Every build task starts by reading it and ends by updating it, so each task is smarter and cheaper than the last, and the work can be done in one shot instead of re-discovering the codebase every time.

## Memory layout

All memory lives in `docs/memory/`. Keep every file short, scannable, and linked to the others (like a small knowledge graph).

```
docs/memory/
  INDEX.md          # entry point: what the project is, map of the other files, last-updated date
  architecture.md   # modules/components, how they connect, data flow, key entry points (file paths)
  conventions.md    # code style, naming, patterns to follow, libs preferred/banned, test approach
  decisions.md      # ADR-style log: date, decision, why, alternatives rejected
  gotchas.md        # traps, flaky areas, "never do X because Y", env/setup quirks
  history.md        # one line per past task: date, what was built, files touched, outcome
  backlog.md        # improvement ideas, tech debt, open questions, not-yet-done items
```

Rules for the files:
- Facts must be verifiable. Reference real paths (`src/auth/session.ts`), not vague descriptions.
- Prefer bullets and tables. No prose walls. Target under ~150 lines per file; split or prune when bigger.
- Every entry gets a date (`YYYY-MM-DD`). Mark stale entries `[STALE?]` instead of silently trusting them.
- Link between files with relative links, e.g. `see [decisions](decisions.md#2026-11-02-use-zod)`.
- Never store secrets, tokens, or credentials.

## Phase 0: Init (only if docs/memory/ does not exist)

1. Scan the repo: README, package/build manifests, top-level tree, entry points, test setup, CI config, recent git log.
2. Create the layout above. Fill what you can verify; leave `TODO` markers for the rest. Do not invent.
3. Tell the user memory was created and summarize it in 3 to 5 lines.

## Phase 1: Read first (mandatory before building)

Before writing any code for a task:

1. Read `docs/memory/INDEX.md`, then only the files relevant to the task (always skim `gotchas.md` and `conventions.md`; read `architecture.md` and `decisions.md` when touching structure).
2. Spot-check 2 or 3 claims against the actual code. If memory disagrees with the repo, trust the repo and queue a fix for Phase 4.
3. Search `history.md` and `backlog.md` for prior work on the same area.

## Phase 2: Think and suggest (the pre-build brief)

Produce a short brief for the user before coding. Keep it tight:

- **Understanding**: the task restated in 1 or 2 lines, plus assumptions.
- **Relevant memory**: the facts, decisions and gotchas that matter here (cite files).
- **Deep thinking**: what could go wrong, edge cases, cross-module impact, hidden coupling, performance/security angles, simpler alternatives.
- **Improvements**: 1 to 3 concrete suggestions that would make the result better (design, naming, reuse of existing code, tests). Mark each `[do now]` or `[backlog]`.
- **One-shot plan**: ordered steps with exact files to create/edit, the pattern to copy from an existing file, the commands to verify (test, lint, build), and the definition of done.

Then:
- If the task is clear and low-risk, proceed straight into building unless the user wants to review the brief.
- If there is a real fork (the choice changes the outcome and is costly to reverse), ask one focused question first.

## Phase 3: Build

Follow the plan and `conventions.md`. Reuse existing patterns. Run the verification commands from the plan. If reality contradicts the plan, note it, adapt, and remember it for Phase 4.

## Phase 4: Write back (mandatory after building)

Update memory as part of finishing the task, not as an afterthought:

- `history.md`: add one line (date, what, key files, result).
- `decisions.md`: add an entry for any non-obvious choice, with the reason and rejected alternatives.
- `gotchas.md`: add anything that surprised you or cost time.
- `architecture.md` / `conventions.md`: update if structure or patterns changed; fix anything found stale in Phase 1.
- `backlog.md`: add `[backlog]` suggestions and open questions; remove items now done.
- `INDEX.md`: bump the last-updated date and fix links.

End the reply with a 2 to 3 line summary of what was built and which memory files changed.

## Principles

- Memory is a cache of understanding, not a source of truth. The code wins.
- Small, accurate, and current beats big and stale. Prune on every update.
- Capture the why, not just the what. The code already shows the what.
- Only record what will help a future task. Skip trivia and one-off details.
- Be proactive but not noisy: the brief is for decisions that matter, not a ritual.
