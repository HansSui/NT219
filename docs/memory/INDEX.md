# Repo memory — NT219 labs

Last updated: 2026-11-09

Entry point for the project brain. Read this first, then only the files the task
needs. The code wins over memory.

## What this repo is

Six self-contained NT219 cryptography labs. `lab01/`, `lab02/`, and `lab03/`
(RSA-OAEP + hybrid AES-GCM) have implementations; Lab 3 is verified on Ubuntu.
Labs 4–6 have briefs and blueprints only.

## Map

| File | Contents |
|---|---|
| [architecture.md](architecture.md) | Lab layout, module boundaries, data flow |
| [conventions.md](conventions.md) | Code, build, test and doc patterns to copy |
| [decisions.md](decisions.md) | ADR-style choices and rejected alternatives |
| [gotchas.md](gotchas.md) | Traps and environment quirks |
| [history.md](history.md) | One line per completed task |
| [backlog.md](backlog.md) | Open work and ideas |

## Sources of truth

- Normative lab briefs: `docs/lab1.txt`, `docs/Lab2.txt` … `docs/Lab6.txt`.
- Course core: `docs/course-objectives.txt`, `docs/GradingSystem.txt`,
  `docs/lab requirement.md`.
- Plan: `docs/blueprint/BLUEPRINT.md`, `docs/blueprint/labs/lab1.md` through
  `lab6.md`.
- Build evidence: each lab's `README.md`, `BENCHMARKS.md`, `output/`.
