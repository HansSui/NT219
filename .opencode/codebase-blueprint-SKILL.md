---
name: codebase-blueprint
description: Plan-only skill. Reads a spec, brief, notes or idea dump (.txt, .md, pasted text) and produces a deep understanding plus a full blueprint of the codebase (infrastructure, repo layout, modules, data model, milestones, formulas, rubric grading) BEFORE any code is written, and always saves it as docs in the repo. Use whenever the user hands over a text/spec/PRD/idea and wants to build an app, tool, service, library or whole project, says "plan the codebase", "architecture first", "think before building", "design the infrastructure", "how should I structure this", or starts a greenfield build, even if they never say "blueprint". Do not write source code while this skill is active.
---

# codebase-blueprint

Turn a text description into a verified plan of the whole codebase, and write that plan into the repo, before anyone builds anything.

## Why this exists

The most expensive mistakes in a build are structural: the wrong module boundaries, a missing requirement, a data model that can't grow. They are cheap to fix on paper and costly after code exists. So this skill separates *understanding and designing* from *building*, and leaves a durable document trail so the build (and every later session) starts from shared understanding instead of guesses.

## Hard boundary

The output of this skill is documents only. Do not create source files, install dependencies, scaffold projects, or run builds. If the user says "go" or "start building", this skill ends; hand off to the build with the blueprint as the plan (and to `repo-memory` if installed). Why: mixing planning with coding makes the plan bend to whatever got coded first.

## Workflow

### Phase 1: Ingest (read everything, trust nothing)

1. Read the entire input file(s) end to end. If there is a repo, also skim README, manifests, tree, and any existing `docs/` so the plan fits what is already there.
2. Build a **requirements ledger** with stable IDs:
   - `R-01…` functional requirements, each traced to where the input says it (line, heading, or quote under 15 words)
   - `N-01…` non-functional (performance, security, cost, compliance, platforms)
   - `C-01…` constraints (stack mandated, deadline, team size, budget)
   - Tag each: `explicit` (stated), `implied` (necessary consequence), or `gap` (needed but missing).
3. Write a **glossary** of domain terms exactly as the input uses them. Misnamed concepts become misnamed modules.
4. **Understanding check**: restate the project in at most 5 lines (who it is for, the core job, the hard part, what success looks like, what is out of scope). If you cannot, the input is not understood yet.
5. Resolve ambiguity. Ask the user only blocking questions (max 5, each with your recommended default). Anything non-blocking becomes a numbered assumption `A-01…` in the docs, so it is visible and reversible. If the user is not available, proceed on assumptions and say so at the top.

### Phase 2: Model the system

- Actors and top use cases (`U-01…`) mapped to requirements.
- Domain entities, their relationships and lifecycles.
- Data flow end to end, plus system boundaries (what is ours vs external services).
- The **hard part**: name the 1 to 3 riskiest or most uncertain pieces. These drive milestone ordering later.

### Phase 3: Design the infrastructure

Cover each of these, briefly and concretely:

- **Stack decisions** as a table: option, fit to constraints, cost, team familiarity, risk, score (see formulas). Record the winner and why in the decision log; rejected options too.
- **Repo layout**: an annotated directory tree. Every directory has one stated responsibility.
- **Modules**: responsibility, public interface, dependencies. Dependencies point one way (no cycles); say which layer may import which.
- **Data model**: entities, key fields, storage choice, migrations approach.
- **Interfaces/contracts**: API routes or function signatures at the boundaries, error model, auth.
- **Cross-cutting**: config and secrets, logging/observability, error handling, security, testing strategy (what is unit vs integration vs end to end).
- **Delivery**: build, CI, environments, deploy, rollback.
- **Milestones**: M0 is a walking skeleton (thinnest end-to-end slice that runs and is tested), then vertical slices ordered by risk first, value second. Each milestone lists files/modules touched, requirement IDs covered, exit criteria, and the verification command.

### Phase 4: Formulas (only where the project has quantities)

Skip this phase explicitly ("no quantitative model needed: reason") if nothing is measurable. Otherwise write each formula with its variables, units, and a worked example using the numbers from the input, flagged as estimate vs given. Useful defaults:

| Purpose | Formula |
|---|---|
| Request rate | `peak_rps = (DAU × actions_per_user_per_day / 86400) × peak_factor` |
| Storage | `bytes_per_year = records_per_day × avg_record_bytes × 365 × replication_factor` |
| Cost | `monthly_cost = Σ(unit_price × usage)` per service, plus a 20% buffer |
| Latency budget | `p95_total ≥ Σ p95 of each hop`; assign each hop a share of the target |
| Option score | `score = Σ(weight_i × rating_i)`, weights sum to 1, ratings 1 to 5 |
| Work priority | `priority = (value × confidence) / effort` |
| Risk | `risk = likelihood × impact` (each 1 to 5); anything ≥ 15 gets a mitigation |
| Effort | `estimate = (optimistic + 4×likely + pessimistic) / 6` |

Show sensitivity for any number that drives a decision: if DAU is 10× higher, does the design still hold?

### Phase 5: Rubric grading

Produce two rubrics in `rubric.md`, with weights, 0 to 4 levels, and what evidence earns each level:

1. **Blueprint rubric** (grade your own plan before showing it). Default criteria and weights:

| Criterion | Weight | 4 means |
|---|---|---|
| Requirement coverage | 25 | every `R/N/C` maps to a module and a milestone; zero orphans |
| Structural soundness | 20 | clear boundaries, one-way dependencies, no god modules |
| Feasibility and risk handling | 15 | hard parts are first in the order and have mitigations |
| Specificity | 15 | someone could start M0 today without asking a question |
| Verifiability | 15 | each milestone has runnable exit criteria; tests strategy is concrete |
| Traceability and clarity | 10 | IDs link everything; assumptions and decisions are explicit |

   `blueprint_score = Σ(weight_i × level_i / 4)` out of 100. **Hard gates** (fail regardless of score): any uncovered requirement, any dependency cycle, any milestone without exit criteria. Iterate on the plan until it clears the gates and scores ≥ 85, or state honestly why it can't.

2. **Build acceptance rubric**: how the finished codebase will be judged later (requirements met, tests pass, matches the architecture, docs updated, performance targets from the formulas). Derive criteria from the ledger so the grading is tied to what the input asked for. Use the same weights/levels format so the build phase can score itself.

### Phase 6: Write the docs (always)

Always leave documentation in the repo, even if the user did not ask. Use `docs/blueprint/` (create `docs/` if missing; if there is no repo yet, create the folder in the working directory):

```
docs/blueprint/
  BLUEPRINT.md        # one-page overview: understanding check, architecture summary, milestones, links
  requirements.md     # ledger (R/N/C IDs, source trace, explicit/implied/gap), glossary, assumptions
  infrastructure.md   # stack table, repo tree, modules, data model, contracts, cross-cutting, delivery
  formulas.md         # quantitative model and sensitivity (or the "not needed" note)
  rubric.md           # blueprint rubric with the self-grade, plus build acceptance rubric
  open-questions.md   # unresolved questions, assumptions to confirm, risks
```

Rules:
- If these files already exist, read them first and update in place; never overwrite silently. Note what changed at the bottom of `BLUEPRINT.md` under a dated "Changes" list.
- Keep each file scannable: tables and bullets, real names and paths, no filler.
- If `docs/memory/` (from `repo-memory`) exists, add a pointer to `docs/blueprint/BLUEPRINT.md` in its `INDEX.md` and log major decisions in its `decisions.md`.
- No secrets in docs.

## Traceability

Every requirement points to at least one module and one milestone; every module points back to at least one requirement. A module with no requirement is scope creep; a requirement with no module is a hole. Check this explicitly before grading; it is the cheapest way to catch real problems.

## What to tell the user at the end

Keep it short: the 5-line understanding, the architecture in one paragraph, the milestone list, the blueprint score with any failed gates, the top 3 risks, the open questions, and the path to the docs. Close with: "Say go and I'll start at M0."

## Principles

- Understand before designing; design before building.
- Prefer the simplest structure that satisfies the requirements and the stated growth, and say what would trigger a redesign.
- Be specific: real file paths, field names and numbers beat adjectives.
- Surface uncertainty instead of hiding it: assumptions and gaps are first-class output.
- The plan is a living document: when reality disagrees, update the docs, not just the code.
