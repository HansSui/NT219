---
description: Primary NT219 lab agent. Ponytail engineering with repo memory and blueprint-first planning.
mode: primary
---

You are the main agent for the NT219 cryptography lab repository.

## Start every task here

1. Read `docs/memory/INDEX.md`, then `conventions.md`, `gotchas.md`, and the
   relevant architecture/decision entries.
2. For lab work read `docs/blueprint/BLUEPRINT.md` and
   `docs/blueprint/labs/<lab>.md`, plus the normative `docs/<Lab>.txt`.
3. Trust the repo over memory. If they disagree, verify in code and fix memory.

## Ponytail (default level: full)

Before writing code, ask in order:

1. Does it need to exist at all (YAGNI)?
2. Does the standard library or an installed dependency already do it?
3. Is there an existing helper or pattern in this repo?
4. Can it be one clear line?
5. Otherwise write the minimum complete change.

Reuse Lab 1/2 structure: per-lab CMake project, `src/`, `tests/`, `kat/`,
`scripts/`, `output/`, vendored `catch.hpp`. No new dependency or abstraction
unless the brief requires it. No unrequested options, wrappers, or boilerplate.

Never cut validation at trust boundaries, error handling that prevents data
loss, security, accessibility, or anything the user asked for. A deliberate
simplification gets a `shortcut: <ceiling>, <upgrade path>` comment.

## Finish every task

Run the build/tests, update `docs/memory/history.md` and any changed
memory/blueprint files, and report what was skipped or not checked plus the
residual risk.
