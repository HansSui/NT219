# Lab 1 — Symmetric Encryption with Crypto++

> **Normative source:** `docs/lab1.txt`
> **Course core:** `docs/lab requirement.md`, `docs/GradingSystem.txt`
> **Existing implementation:** `lab01/`
> **Tool:** `aestool`

## 1. What to do

1. Build one CLI, `aestool`, that selects AES modes at runtime:
   `ecb`, `cbc`, `ofb`, `cfb`, `ctr`, `xts`, `ccm`, `gcm`.
2. Use **Crypto++ for every cryptographic primitive**. OpenSSL, libsodium, and
   other crypto libraries are forbidden. The C++ STL is allowed.
3. Support AES-128/192/256 key sizes; prefer AES-256 in the report.
4. Implement authenticated behavior for CCM and GCM: `--aead`, `--aad FILE`,
   `--aad-text STRING`, tag verification on decrypt, fail closed on tag failure.
5. Implement misuse checks:
   - Warn when ECB is selected.
   - Block ECB files larger than 16 KiB unless `--allow-ecb`.
   - Enforce per-mode IV/nonce length.
   - Auto-generate a missing IV/nonce with `AutoSeededRandomPool` (except
     ECB/XTS) and persist it in an output header or sidecar JSON.
   - Reject reuse of the same key + nonce for CTR, CCM, GCM and document how
     that detection works.
6. Implement the CLI commands `encrypt` and `decrypt`, key input via
   `--key-hex` or `--key`, plaintext via `--in` or `--text`, and output via
   `--out` or stdout.
7. Write a `--kat vectors.json` runner for the official vectors and a negative
   test suite.
8. Benchmark every mode at the required sizes and compare stream vs block,
   AEAD vs non-AEAD, tag overhead, and Windows vs Linux.
9. Write the security section: ECB insecurity, CBC padding-oracle risk, CTR
   nonce-reuse catastrophe, AEAD guarantees, GCM unique-nonce requirement, XTS
   lacks integrity, RNG and key-storage risks.
10. Optional bonus: export the core as `.dll`/`.so` and build a Python or C#
    GUI that calls that shared library.

## 2. Requirements

All shared requirements C-01…C-16 in
[`BLUEPRINT.md`](../BLUEPRINT.md#common-core) apply. The points below are
lab-specific additions or overrides.

### 2.1 Constraints

- Crypto++ only for primitives; no OpenSSL/libsodium.
- CMake >= 3.16; build on Ubuntu, Windows MSVC, and MinGW64.
- C++17; UTF-8 input/output; fail closed on malformed input.
- Same CLI shape as the course core (`BLUEPRINT.md` C-03/C-04).

### 2.2 CLI and formats

```
aestool encrypt --mode gcm --key key.bin --in msg.txt --out ct.bin
aestool decrypt --mode gcm --key key.bin --in ct.bin --out msg.txt
```

| Item | Requirement |
|---|---|
| Key | `--key KEYFILE` or `--key-hex HEX`; key file may be raw binary or hex with header. |
| IV/nonce | `--iv IVFILE`, `--nonce NONCEFILE`; auto-generate when omitted except ECB/XTS; persist. |
| Input | `--in FILE` (binary-safe) or `--text "..."` (UTF-8). |
| Output | Raw ciphertext to `--out`; hex or Base64 on screen; `--encode` override. |
| Sidecar header | JSON with `alg`, `mode`, `iv`, `aad` (if any), `tag` (AEAD). |
| AEAD | `--aead`, `--aad FILE`, `--aad-text STRING`; verify tag on decrypt. |

### 2.3 Validation and negative tests

Must demonstrate and record:

- Wrong key -> incorrect plaintext.
- Wrong IV -> incorrect plaintext.
- Tampered ciphertext, non-AEAD -> corrupted output.
- Tampered ciphertext, AEAD -> authentication failure.
- Invalid tag -> decryption refusal.
- Invalid IV length -> rejection.
- ECB above 16 KiB without `--allow-ecb` -> refusal.
- Nonce reuse for CTR/CCM/GCM -> refusal.

All malformed inputs must terminate securely.

### 2.4 Correctness and KATs

- NIST SP 800-38A vectors for CBC, CFB, OFB, CTR.
- NIST GCM vectors.
- NIST CCM vectors (RFC 3610 vectors are acceptable if cited).
- JSON vector file, PASS/FAIL per case, summary counts.
- `--kat` must be runnable from the CLI, not only from unit tests.

### 2.5 Performance

Required payload sizes: 1 KB, 4 KB, 16 KB, 256 KB, 1 MB, 8 MB.

Required metrics: throughput (MB/s) and latency per operation. Follow the
course protocol: 1-2 s warm-up, ~1000 operations per block, 30-100 repeats,
mean/median/stddev/95% CI.

Required comparisons:

- Windows vs Linux.
- Stream vs block modes.
- AEAD vs non-AEAD.
- GCM/CCM tag overhead.
- OS scheduling and system-call overhead.

Include tables, plots, and interpretation, not just numbers.

### 2.6 Security discussion

- Why ECB is insecure.
- CBC padding-oracle risk.
- CTR nonce reuse and the two-time pad.
- AEAD guarantees vs non-AEAD modes.
- Why GCM requires unique nonces.
- XTS confidentiality without integrity.
- Randomness generation, safe IV storage, tag verification, fail-closed
  behavior, key-storage risk.
- RNG, filesystem, and performance differences across OSes.

### 2.7 Bonus

- Export encryption core as `.dll`/`.so`/`.a`/`.lib`.
- Minimal Python or C# GUI.
- GUI must call the compiled library, not reimplement crypto.

## 3. What to expect

### Learning outcomes

After this lab you should be able to:

- Use Crypto++ safely across block, stream, and AEAD modes.
- Manage IV/nonce lifecycles and detect reuse.
- Design a binary-safe file and encoding pipeline.
- Validate against official NIST vectors.
- Benchmark symmetric encryption and explain the results.
- Recognise common cryptographic misuse.

### Definition of done (grader evidence)

- [ ] `aestool` builds on Ubuntu GCC/Clang and Windows MSVC/MinGW.
- [ ] KAT log shows PASS for every vector and a summary.
- [ ] `ctest` passes on both OSes.
- [ ] Negative-test log covers every case in §2.3.
- [ ] Benchmark CSVs exist for every mode and required size; statistics and
      plots are generated.
- [ ] README lists dependencies, install steps, build commands, CLI examples,
      and known limitations.
- [ ] Header/sidecar format is documented, including nonce-reuse detection.
- [ ] Report section covers correctness, performance, and the security topics
      in §2.6.
- [ ] Self-grade rubric filled in.
- [ ] Academic-integrity statement included.

## 4. Rubric (100 + 5 bonus)

| Criterion | Points | Full-credit evidence |
|---|---:|---|
| Correctness and KATs | 25 | All eight modes work; official vectors pass; `--kat` output is clear. |
| Security hygiene | 15 | ECB guard, IV/nonce validation, auto-generation and persistence, reuse detection, fail-closed AEAD. |
| UX and I/O design | 10 | Uniform CLI, UTF-8, binary-safe files, hex/base64/raw, useful errors, documented header. |
| Performance methodology | 20 | Required sizes, warm-up/repeats, mean/median/SD/95% CI, mode and OS comparisons, plots and interpretation. |
| Cross-platform build | 10 | CMake builds and tests on Ubuntu and Windows with documented commands. |
| Report quality | 20 | Objectives, design, tests, performance, security analysis, limitations, citations. |
| **Subtotal** | **100** | |
| Bonus: library export + GUI | +5 | Shared core plus GUI that calls it through the exported ABI. |

## 5. Infrastructure

### Current layout (`lab01/`)

```
lab01/
  CMakeLists.txt
  README.md
  BENCHMARKS.md
  REPORT.md
  src/
    aescore.hpp/.cpp      crypto core and container format
    aesgui.h/.cpp         C ABI for the GUI
    cryptopp_all.hpp      single Crypto++ include point
    minijson.hpp          JSON reader for KAT files
    main.cpp              CLI: encrypt, decrypt, bench, --kat
  tests/
    catch.hpp             vendored Catch2 2.13.10
    test_core.cpp
    cli_test.sh / cli_test.cmake
  kat/vectors.json
  gui/                    Tkinter GUI + build/run helpers
  scripts/                build, benchmark, summarize, plot
  output/                 logs, CSVs, graphs
```

### Dependencies

| Dependency | Version | Notes |
|---|---|---|
| CMake | >= 3.16 (verified 4.3.1 Windows, 4.2.3 Linux) | out-of-source builds |
| C++ compiler | C++17: GCC 15.2, Clang 21.1.8, MSVC 19.51 | Windows + Linux |
| Crypto++ | 8.9.0 | required for all primitives |
| Catch2 | 2.13.10 | vendored header, offline build |
| Python | 3.8+ | optional: plots and GUI |
| Tkinter / matplotlib | system / recent | optional: GUI and plots |

### Commands

Linux:

```bash
cd lab01
scripts/build_linux.sh gcc        # or: scripts/build_linux.sh clang
scripts/run_benchmarks.sh gcc
./build/ubuntu-gcc/aestool --kat kat/vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
python3 scripts/plot_crossplatform.py
```

Windows (x64 Native Tools prompt):

```bat
cd lab01
scripts\build_windows.bat msvc
scripts\bench_windows_controlled.bat
scripts\run_benchmarks.bat msvc
python scripts\plot_crossplatform.py
```

GUI (optional):

```bash
./gui/build_gui.sh && ./gui/run_gui.sh
```

```bat
gui\build_gui.bat
gui\run_gui.bat
```

### Benchmark matrix

Eight modes x six sizes (1 KB, 4 KB, 16 KB, 256 KB, 1 MB, 8 MB) x 1000 runs,
plus the controlled Windows run. Raw CSVs and plots live under
`lab01/output/`. Keep the environment table (CPU, cores, RAM, OS, compiler,
flags) with the data.

### Evidence already in the repo

- Windows MSVC KAT 29/29 and `ctest` 2/2, plus 19 negative/edge cases in
  `cli_test.cmake` and per-case logs.
- MinGW GCC/Clang KAT 29/29 and `ctest` 2/2.
- Ubuntu GCC benchmark matrix and test logs (KAT log predates the 29-vector
  expansion).
- Tkinter GUI self-test on Windows and Linux.

### Risks / gaps

- The report is being refreshed for the latest Windows run; keep the number
  consistent between `README.md`, `BENCHMARKS.md`, and `REPORT.md`.
- Ubuntu Clang was built and tested but not benchmarked; say so in the report
  instead of mixing compiler results.
- Nonce-reuse detection is only as good as the persisted history; document its
  scope (per header/sidecar, not global).
