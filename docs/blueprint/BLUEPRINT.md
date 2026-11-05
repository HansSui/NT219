# NT219 Labs 1-6 — Blueprint

> Plan-only document set. No source code. Written 2026-11-02.
>
> **Normative sources:** `docs/lab1.txt`, `docs/Lab2.txt`, `docs/Lab3.txt`,
> `docs/Lab4.txt`, `docs/Lab5.txt`, `docs/Lab6.txt`.
> **Course core:** `docs/lab requirement.md` and `docs/GradingSystem.txt`.
> **Implemented-lab evidence:** `lab01/README.md`, `lab02/README.md`,
> `lab02/REPORT.md`, `docs/lab2_reference.md`.
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
  and malicious exploitation. Labs 3-6 have specifications but no code in the
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

Extracted from `docs/lab requirement.md` and `docs/GradingSystem.txt`.

| ID | Requirement | Source |
|---|---|---|
| C-01 | CMake >= 3.16, out-of-source build; compiles on Ubuntu LTS, Windows MSVC, and MinGW64. | `lab requirement.md` §1, §4.1 |
| C-02 | Top-level and per-lab `README.md`: dependency versions, install steps for both OSes, build commands, CLI examples, known limitations. | `lab requirement.md` §1A |
| C-03 | Uniform CLI shape: `tool <command> [--in\|--text] [--out] [--key\|--key-hex] [--iv] [--nonce] [--mode] [--aead] [--encode hex\|base64\|raw] [--threads N] [--kat FILE] [--verbose]`. | `lab requirement.md` §1A |
| C-04 | UTF-8 input/output; binary-safe `--in`/`--out`; console defaults to hex for ciphertext/digest/signature; file output defaults to raw; `--encode` overrides. | `lab requirement.md` §1B |
| C-05 | Secure randomness only: Crypto++ `AutoSeededRandomPool` or OpenSSL `RAND_bytes`. `rand()` and `std::random` are forbidden for keys, IVs, nonces, salts. | `lab requirement.md` §2 |
| C-06 | Enforce IV/nonce length per mode; auto-generate securely when allowed; persist IV/nonce in a header or sidecar; prevent nonce reuse where prohibited. | `lab requirement.md` §2 |
| C-07 | Minimum key sizes: AES 128/192/256 (prefer 256 in reports), RSA >= 3072 bits, ECDSA >= P-256. | `lab requirement.md` §2 |
| C-08 | KAT runner with JSON vectors, PASS/FAIL per case, and summary. Use official NIST/FIPS/RFC vectors where applicable. | `lab requirement.md` §3 |
| C-09 | Negative tests: wrong key, wrong IV/nonce, tampered ciphertext, tampered tag, malformed input, corrupted PEM/key where applicable. Fail closed. | `lab requirement.md` §3 |
| C-10 | Unit tests with Catch2 or GoogleTest; `ctest` passes on Windows and Linux. | `lab requirement.md` §3 |
| C-11 | Performance protocol: 1-2 s warm-up, ~1000 operations per block, 30-100 repeats, mean/median/stddev/95% CI, comparison tables, plots, and interpretation. | `lab requirement.md` §4; `GradingSystem.txt` |
| C-12 | Security section: threat model, misuse scenarios, relevant attacks, secure defaults, fail-closed behavior, limitations. | `lab requirement.md` §5 |
| C-13 | Final archive: source, CMake, README, tests, Windows and Linux binaries, report PDF/DOCX, self-grade checklist, academic-integrity statement. | `lab requirement.md` §6 |
| C-14 | One report for all labs: objectives, implementation, tests, performance method, statistics/plots, security, cross-platform comparison, lessons; ~60 pages typical. | `lab requirement.md` §7 |
| C-15 | GUI bonus is optional: export the core as `.dll`/`.so`/`.a`/`.lib`; Python or C# GUI must call the same compiled library, never duplicate crypto logic. | `lab requirement.md` §8 |
| C-16 | Ethics: own work, cite libraries/vectors/references, offline test artifacts only, test keys only, disclose AI/tool assistance. | `lab requirement.md` §9 |

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
| 3 | RSA-OAEP + AES-GCM hybrid envelope | `rsatool` | Crypto++ or OpenSSL allowed | Specified only |
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
| M2 | Lab 2 `aestool` | FIPS-197/SP 800-38A KAT, Crypto++ parity, `ctest`, 1 MiB/100 MiB/1 GiB benchmarks | Core/tests/report done; Windows 9/9 KAT + 4/4 CTest on MSVC/MinGW; Windows MSVC benchmark chosen 1000/100/30 with summary/plots/audit. Ubuntu logs, run-time env log and Crypto++ throughput still open (`lab02/REPORT.md`, `docs/missing/lab2.md`) |
| M3 | Lab 3 `rsatool` | OAEP + hybrid KAT, negative tests, 3072/4096 benchmarks | Not started |
| M4 | Lab 4 `hashtool` | Hash KAT, X.509 parse/verify, both attack demos, benchmarks | Not started |
| M5 | Lab 5 `sigtool` | ECDSA/RSA-PSS vectors, negative tests, benchmarks | Not started |
| M6 | Lab 6 `pqtool` | ML-DSA/ML-KEM vectors, PQ certificate verify/tamper test, benchmarks | Not started |

Each milestone is done only when its lab doc's **definition of done** is
satisfied and the evidence is in `output/`.

## Assumptions and gaps

Non-blocking assumptions are recorded here so a later session can reverse them.

| ID | Assumption / gap | Why it matters | Default |
|---|---|---|---|
| A-01 | Labs 3-6 code does not exist yet; their docs describe the target, not current state. | Prevents treating a plan as evidence. | Build each lab from its brief. |
| A-02 | Lab 3 library choice is not fixed beyond "Crypto++ / OpenSSL". | Changes key-format and build code. | Use Crypto++ 8.9.0 to match Lab 1; use OpenSSL CLI only for format checks. |
| A-03 | Lab 4 does not say whether hashes must be from scratch. | From-scratch SHA-2/SHA-3 is a large scope increase. | Use OpenSSL/Crypto++ implementations; document the choice. |
| A-04 | Lab 4 attack tools (`hashclash`, `hashpump`) may not be installed. | Blocks the two mandatory demos. | Use the tools for the demo; implement the +5 length extension yourself if needed. |
| A-05 | Lab 5 RFC 6979 support depends on the chosen library. | Random ECDSA nonces are a fail condition. | Use library RFC 6979 support or implement the derivation; state which. |
| A-06 | Lab 6 does not name a PQC library or key-encoding standard. | Determines build and PEM/DER handling. | Use liboqs or PQClean with a pinned version; document custom PEM armor if used. |
| A-07 | Windows evidence for Labs 3-6 needs a Windows host or VM. | Cross-platform build is graded. | Collect MSVC + MinGW logs, not just Linux. |
| A-08 | Total report length (~60 pages) is a guideline, not a per-lab quota. | Report effort is often underestimated. | Write each lab section as its evidence is produced. |

## Academic integrity

Include the statement of originality, the AI/tool-assistance disclosure, and
citations for every library, vector set, and external tool. Attack demos stay
offline and use only files the team created. Keys committed to the repo are
test keys only.
