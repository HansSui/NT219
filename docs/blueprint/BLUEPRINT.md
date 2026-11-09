# NT219 Labs 1-6 — Blueprint

> Plan-only document set. No source code. Written 2026-11-02.
>
> **Normative sources:** `docs/lab1.txt`, `docs/Lab2.txt`, `docs/Lab3.txt`,
> `docs/Lab4.txt`, `docs/Lab5.txt`, `docs/Lab6.txt`.
> **Course core:** `docs/course-objectives.txt` and `docs/GradingSystem.txt`;
> toolchain: `docs/lab requirement.md`.
> **Implemented-lab evidence:** `lab01/README.md`, `lab01/REPORT.md`,
> `lab02/README.md`, `lab02/REPORT.md`, `lab03/README.md`, `lab03/REPORT.md`,
> `docs/lab2_reference.md`.
>
> If a lab brief and this blueprint disagree, the lab brief wins. If the course
> core and a lab brief disagree, the lab brief wins for that lab.

## Understanding check

- **Who:** NT219 students submitting six cryptography labs as one archive.
- **Core job:** six CLI tools that implement or apply the assigned primitives,
  prove correctness with official vectors, fail closed on misuse, benchmark
  honestly, and explain the security.
- **Hard parts:** byte-exact KAT compliance, nonce/key lifecycle, hybrid and
  post-quantum formats, and statistically defensible benchmark evidence.
- **Success:** CMake builds on Ubuntu and Windows, KATs and `ctest` pass,
  negative tests fail closed, the report contains tables/plots with 95% CIs,
  and the self-grade rubric is filled in.
- **Out of scope:** production key management, live targets, third-party data,
  and malicious exploitation. Labs 4-6 have specifications but no code in the
  repo yet.

## Document map

| File | Contents |
|---|---|
| `BLUEPRINT.md` | Shared course core, lab index, repo model, grading policy, gaps |
| `labs/lab1.md` | Lab 1 — symmetric encryption with Crypto++ |
| `labs/lab2.md` | Lab 2 — AES-128 pure C++ and CTR |
| `labs/lab3.md` | Lab 3 — RSA-OAEP and hybrid encryption |
| `labs/lab4.md` | Lab 4 — hashing, PKI, and controlled attacks |
| `labs/lab5.md` | Lab 5 — ECDSA and RSA-PSS signatures |
| `labs/lab6.md` | Lab 6 — ML-DSA, ML-KEM, and PQ certificates |

Each lab doc carries its own **what to do, requirements, expected outcomes,
rubric, and infrastructure**. This file carries only the rules that apply to
all six, so they are not repeated six times.

## Common core (applies to every lab)

Extracted from `docs/course-objectives.txt` and `docs/GradingSystem.txt`;
toolchain versions and setup come from `docs/lab requirement.md`.

| ID | Requirement | Source |
|---|---|---|
| C-01 | CMake >= 3.16, out-of-source build; compiles on Ubuntu LTS, Windows MSVC, and MinGW64. | `lab requirement.md` §1; `course-objectives.txt` §1A |
| C-02 | Top-level and per-lab `README.md`: dependency versions, install steps for both OSes, build commands, CLI examples, known limitations. | `course-objectives.txt` §1A |
| C-03 | Uniform CLI shape: `tool <command> [--in\|--text] [--out] [--key\|--key-hex] [--iv] [--nonce] [--mode] [--aead] [--encode hex\|base64\|raw] [--threads N] [--kat FILE] [--verbose]`. | `course-objectives.txt` §1A |
| C-04 | UTF-8 input/output; binary-safe `--in`/`--out`; console defaults to hex for ciphertext/digest/signature; file output defaults to raw; `--encode` overrides. | `course-objectives.txt` §1B |
| C-05 | Use the required crypto library's secure RNG (Crypto++ `AutoSeededRandomPool` or OpenSSL `RAND_bytes`; Lab 6 must use its PQ library's approved RNG). Never use `rand()` or `std::random` for secrets, keys, IVs, nonces, or salts. | `course-objectives.txt` §2; Lab 6 library requirements |
| C-06 | Enforce IV/nonce length per mode; auto-generate securely when allowed; persist IV/nonce in a header or sidecar; prevent nonce reuse where prohibited. | `course-objectives.txt` §2 |
| C-07 | Minimum key sizes: AES 128/192/256 (prefer 256 in reports), RSA >= 3072 bits, ECDSA >= P-256. | `course-objectives.txt` §2 |
| C-08 | KAT runner with JSON vectors, PASS/FAIL per case, and summary. Use official NIST/FIPS/RFC vectors where applicable. | `course-objectives.txt` §3 |
| C-09 | Negative tests: wrong key, wrong IV/nonce, tampered ciphertext, tampered tag, malformed input, corrupted PEM/key where applicable. Fail closed. | `course-objectives.txt` §3 |
| C-10 | Unit tests with Catch2 or GoogleTest; `ctest` passes on Windows and Linux. | `course-objectives.txt` §3 |
| C-11 | Performance protocol: 1-2 s warm-up, ~1000 operations per block, 30-100 repeats, mean/median/stddev/95% CI, comparison tables, plots, and interpretation. | `course-objectives.txt` §4; `GradingSystem.txt` |
| C-12 | Security section: threat model, misuse scenarios, relevant attacks, secure defaults, fail-closed behavior, limitations. | `course-objectives.txt` §5 |
| C-13 | Final archive: source, CMake, README, tests, report PDF/DOCX, self-grade checklist, and academic-integrity statement. The course objectives require Windows and Linux binaries, while `GradingSystem.txt` allows binaries **or** clear build instructions; confirm which rule the grader applies. | `course-objectives.txt` §6; `GradingSystem.txt` submission checklist |
| C-14 | One report for all labs: objectives, implementation, tests, performance method, statistics/plots, security, cross-platform comparison, lessons; ~60 pages typical. | `course-objectives.txt` §7 |
| C-15 | GUI bonus is optional: export the core as `.dll`/`.so`/`.a`/`.lib`; Python or C# GUI must call the same compiled library, never duplicate crypto logic. | `course-objectives.txt` §8 |
| C-16 | Ethics: own work, cite libraries/vectors/references, offline test artifacts only, test keys only, disclose AI/tool assistance. | `course-objectives.txt` §9 |

### Course-wide grading bands

`docs/GradingSystem.txt` gives ranges for the overall submission. Each lab
brief gives a concrete 100-point table; use the lab table for that lab and the
bands below to sanity-check the report and archive.

| Criterion | Points |
|---|---|
| Correctness and KATs | 35-40 |
| Security hygiene / misuse checks | 10-20 |
| Cross-platform build and UX | 10 |
| Performance study quality | 10-20 |
| Report quality and clarity | 5-10 |
| Negative tests | 10-15 |
| Bonus | up to +15 |

Every lab also needs the same evidence pack: KAT log, `ctest` log, negative-test
log, benchmark CSVs, summary statistics, plots, environment table, and a filled
self-grade table.

## Lab index

| Lab | Topic | Tool | Crypto dependency | Repo status |
|---|---|---|---|---|
| 1 | AES modes: ECB/CBC/OFB/CFB/CTR/XTS/CCM/GCM | `aestool` | Crypto++ **required**; OpenSSL forbidden for primitives | Implemented in `lab01/` |
| 2 | AES-128 core + CTR, pure C++ | `aestool` | STL only for primitives; Crypto++/OpenSSL **oracle only** | Implemented in `lab02/` |
| 3 | RSA-OAEP + AES-GCM hybrid envelope | `rsatool` | OpenSSL 4.0.2 | Implemented in `lab03/` |
| 4 | SHA-2/SHA-3/SHAKE, X.509, MD5/length-extension demos | `hashtool` | OpenSSL/Crypto++ plus `hashclash`/`hashpump` | Specified only |
| 5 | ECDSA P-256/P-384 and RSA-PSS-3072 | `sigtool` | Crypto++/OpenSSL allowed | Specified only |
| 6 | ML-DSA-44/65, ML-KEM-512/768, PQ certificate | `pqtool` | PQC library not named by the brief | Specified only |

## Repo model

Keep each lab self-contained with the same skeleton so the archive and the
grader's build commands stay predictable. Labs 1 and 2 already follow it.

```
NT219/
  README.md                  # top-level: deps, build, CLI examples, limitations
  docs/                      # briefs + this blueprint
  lab01/ ... lab06/          # one self-contained CMake project per lab
    CMakeLists.txt
    README.md
    src/                     # crypto core, formats, CLI
    tests/                   # Catch2/GoogleTest + CLI smoke tests
    kat/                     # JSON known-answer vectors
    scripts/                 # build, test, benchmark, summarize, plot
    output/                  # logs, CSVs, graphs (gitignored or curated)
```

Per-lab layout, module names, dependencies, and commands are in each lab doc.
No cross-lab library is required; shared code between labs would add coupling
without a requirement.

## Milestones and status

| Milestone | Scope | Exit gate | Status |
|---|---|---|---|
| M1 | Lab 1 `aestool` | KAT 8 modes, `ctest`, negative tests, benchmark CSVs, report section | Done in `lab01/`; report refresh noted in README |
| M2 | Lab 2 `aestool` | FIPS-197/SP 800-38A KAT, oracle parity, `ctest`, 1 MiB/100 MiB/1 GiB benchmarks | Core/tests/report done; Ubuntu KAT 9/9, CTest 4/4, OpenSSL cross-validation and 1000/100/30 benchmark committed. Windows KAT/unit logs valid; Windows CTest/cross-validation need a rerun after the OpenSSL switch. Open: Windows run-time environment log, Windows OpenSSL throughput, PDF/identity (`lab02/REPORT.md`, `docs/missing/lab2.md`) |
| M3 | Lab 3 `rsatool` | OAEP + hybrid KAT, negative tests, 3072/4096 benchmarks | Done in `lab03/`: OpenSSL 4.0.2 + GCC 15.2, KAT 2/2, CTest 5/5, CPU-pinned 4-thread benchmark (1000/100 runs) and grouped-bar plots. Regrade 97/100 core + 6 bonus; see `docs/missing/lab3.md`. Windows build scripts present, not run on this host. |
| M4 | Lab 4 `hashtool` | Hash KAT, X.509 parse/verify, both attack demos, benchmarks | Not started |
| M5 | Lab 5 `sigtool` | ECDSA/RSA-PSS vectors, negative tests, benchmarks | Not started |
| M6 | Lab 6 `pqtool` | ML-DSA/ML-KEM vectors, PQ certificate verify/tamper test, benchmarks | Not started |

Each milestone is done only when its lab doc's **definition of done** is
satisfied and the evidence is in `output/`.

## Assumptions and gaps

Non-blocking assumptions are recorded here so a later session can reverse them.

| ID | Assumption / gap | Why it matters | Default |
|---|---|---|---|
| A-01 | Labs 4-6 code does not exist yet; their docs describe the target, not current state. | Prevents treating a plan as evidence. | Build each lab from its brief. |
| A-02 | Lab 4 does not say whether hashes must be from scratch. | From-scratch SHA-2/SHA-3 is a large scope increase. | Use OpenSSL/Crypto++ implementations; document the choice. |
| A-03 | Lab 4 demo tools may not be installed. | Blocks the controlled offline demonstrations. | Confirm tool availability before the lab; keep demonstrations to owned artifacts. |
| A-04 | Lab 5 RFC 6979 support depends on the chosen library. | Random ECDSA nonces are a fail condition. | Use library RFC 6979 support or implement the derivation; state which. |
| A-05 | Lab 6 does not name a PQC library or key-encoding standard. | Determines build and serialization choices. | Choose a pinned, vetted implementation and document its supported encodings. |
| A-06 | Windows evidence for Labs 4-6 needs a Windows host or VM. | Cross-platform build is graded. | Collect MSVC + MinGW logs, not just Linux. |
| A-07 | Total report length (~60 pages) is a guideline, not a per-lab quota. | Report effort is often underestimated. | Write each lab section as its evidence is produced. |
| A-08 | The binary requirement conflicts between the course objectives and grading checklist. | Affects what the final archive must contain. | Confirm with the instructor; do not omit binaries without approval. |

## Changes

- 2026-11-09 — Corrected the implementation status, randomness guidance for
  Lab 6, and the unresolved archive-binary requirement; see per-lab notes for
  corrected certificate-validation and ML-KEM test semantics.

## Academic integrity

Include the statement of originality, the AI/tool-assistance disclosure, and
citations for every library, vector set, and external tool. Attack demos stay
offline and use only files the team created. Keys committed to the repo are
test keys only.
