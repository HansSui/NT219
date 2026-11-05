# Lab 2 — AES-128 in Pure C++ and CTR Mode

> **Normative source:** `docs/Lab2.txt`
> **Deep implementation reference:** `docs/lab2_reference.md` (normative brief
> still wins; the reference is an implementation guide)
> **Course core:** `docs/lab requirement.md`, `docs/GradingSystem.txt`
> **Existing implementation:** `lab02/`
> **Report:** `lab02/REPORT.md`
> **Audit / regrade:** `docs/missing/lab2.md`
> **Tool:** `aestool`
>
> **Status (2026-11-05):** AES-128/CTR core and bonuses A/B are implemented.
> Windows MSVC, MinGW GCC and MinGW Clang each pass 9/9 KAT and 4/4 CTest
> (unit, KAT, CLI, cross-validation). Windows MSVC benchmark data now exists
> for 1 MiB/100 MiB/1 GiB (chosen 1000/100/30, N >= 30) with summary and
> plots. Ubuntu logs, a Windows run-time environment log and the Ubuntu
> benchmark rerun are still open; see `docs/missing/lab2.md`.

## 1. What to do

1. Implement AES-128 from scratch in C++17 using only the STL:
   `AddRoundKey`, `SubBytes`, `ShiftRows`, `MixColumns`, and `KeyExpansion`
   (10 rounds, 44 words).
2. Use the FIPS-197 **column-major** state mapping:
   `state[col][row] = block[col * 4 + row]`.
3. Implement CTR mode per NIST SP 800-38A:
   - 128-bit IV (nonce + counter).
   - Big-endian counter increment.
   - Arbitrary-length plaintext, partial final block XOR, no padding.
   - Counter-overflow detection: fail closed instead of wrapping to zero.
   - Encryption and decryption both use the forward AES cipher.
4. Build the CLI commands `encrypt` and `decrypt` with `--mode ctr`; key exactly
   16 bytes, IV exactly 16 bytes; support raw binary and optional hex debugging.
5. Validate with FIPS-197 Appendix C and NIST SP 800-38A CTR vectors through a
   `--kat vectors.json` runner.
6. Cross-validate byte-for-byte against Crypto++ or OpenSSL AES-128-CTR. The
   oracle is test-only; `aestool` must not link an external crypto library.
7. Run negative tests and explain why CTR cannot detect tampering.
8. Benchmark AES-128 CTR at 1 MiB, 100 MiB, and 1 GiB if feasible; compare
   table-based vs computed S-box, memory footprint, cache behavior, branch
   prediction, and Crypto++ throughput.
9. Write the security section: timing/cache leakage from table S-boxes and
   T-tables, constant-time coding, keystream reuse/two-time pad, CTR
   malleability, confidentiality without authenticity, why CTR is not AEAD,
   and how AES-NI changes the risk profile.
10. Optional bonuses: AES-192/256, AES-XTS, AES-NI or bit-sliced AES.

## 2. Requirements

All shared requirements C-01…C-16 in
[`BLUEPRINT.md`](../BLUEPRINT.md#common-core) apply. The points below are
lab-specific additions or overrides.

### 2.1 Constraints

- **No external crypto library for primitives.** Crypto++/OpenSSL may appear
  only in a separate cross-validation/oracle target.
- Pure C++17 STL for the AES core and CTR mode.
- CMake >= 3.16; Ubuntu GCC/Clang and Windows MSVC/MinGW64.
- Fail closed on key/IV length, counter overflow, and I/O errors.

### 2.2 CLI and formats

```
aestool encrypt --mode ctr --key key.bin --iv iv.bin --in msg.bin --out ct.bin
aestool decrypt --mode ctr --key key.bin --iv iv.bin --in ct.bin --out msg.bin
```

| Item | Requirement |
|---|---|
| Key | Exactly 16 bytes for AES-128. |
| IV | Exactly 16 bytes; document endianness and overflow behavior. |
| Input/output | Raw binary by default; `--encode`/`--decode hex\|base64\|raw` and `--out-hex` for debugging. |
| Modes | `ctr` required; `xts` only as Bonus B. |
| Key sources | `--key` or `--key-hex`, never both. |
| IV sources | `--iv`, `--iv-hex`, `--nonce` or `--nonce-hex`, only one. |
| Bench | `--warmup`, `--threads`, `--out csv`, `--verbose` progress/ETA; unknown options fail closed. |
| No padding | CTR requires no padding and no block alignment. |

### 2.3 Validation and negative tests

- Wrong key -> incorrect plaintext.
- Wrong IV -> incorrect plaintext.
- Tampered ciphertext -> corrupted plaintext.
- Invalid key length -> rejection.
- Invalid IV length -> rejection.
- Counter overflow -> fail closed.
- File I/O error -> clear failure.

Explain in the report why CTR cannot detect the tampering cases.

### 2.4 Correctness and KATs

- FIPS-197 Appendix C AES core vectors.
- NIST SP 800-38A CTR vectors.
- `--kat vectors.json`: PASS/FAIL per case and summary.
- Cross-validation target asserts exact ciphertext equality with Crypto++
  AES-128-CTR or OpenSSL AES-128-CTR using the same key and IV.
- Unit tests with Catch2 and `ctest`.

### 2.5 Performance

| Payload | Required runs | Purpose |
|---|---:|---|
| 1 MiB | 1000 | fits in cache; raw algorithm cost |
| 100 MiB | 500 | exceeds cache; memory bandwidth |
| 1 GiB | 50 (optional if feasible) | sustained streaming |

Measure AES-128 CTR encryption and decryption throughput. Analyze:

- Table-based vs computed S-box.
- Memory footprint and cache-line behavior.
- Branch-prediction impact.
- Comparison with Crypto++.
- Throughput table (MB/s) and plots.

Use the course protocol for warm-up, repetitions, and 95% CIs.

### 2.6 Security discussion

- AES core: timing leakage from table-based S-box, cache/T-table attacks,
  constant-time coding.
- CTR: keystream reuse, two-time pad, why IV reuse is catastrophic, why CTR is
  confidentiality only, why it needs a MAC, why it is not AEAD.
- Hardware acceleration: why AES-NI mitigates software leakage, and the
  performance/security trade-off.

### 2.7 Bonus

| Bonus | Points | Required work |
|---|---:|---|
| AES-192 / AES-256 | +10 | Full key schedules (12/14 rounds, AES-256 `SubWord` at `i % 8 == 4`), FIPS-197 C.2/C.3 KATs. |
| AES-XTS | +5 | Two-key construction, tweak multiplication in GF(2^128), IEEE P1619 ciphertext stealing, disk-encryption use case, Crypto++ parity. |
| AES-NI or bit-sliced | +5 | Runtime CPUID detection + AES-NI intrinsics, or bitsliced software; compare with pure software and explain constant-time impact. |

## 3. What to expect

### Learning outcomes

After this lab you should be able to:

- Explain the internal AES round structure and key schedule.
- Implement a block cipher and a stream mode from the specification.
- Explain why nonce reuse destroys confidentiality.
- Explain why CTR must be combined with a MAC.
- Explain why constant-time implementation matters.
- Explain how AES-NI changes the security and performance posture.

### Definition of done (grader evidence)

- [ ] Pure C++ AES-128 core passes FIPS-197 vectors.
- [ ] CTR passes NIST SP 800-38A vectors, including partial final blocks.
- [ ] Counter overflow fails closed.
- [ ] Cross-validation proves byte equality with Crypto++/OpenSSL.
- [ ] Negative tests cover wrong key/IV, tampering, length errors, overflow.
- [ ] `ctest` passes on Linux and Windows.
- [ ] Benchmarks cover 1 MiB and 100 MiB (1 GiB if feasible) with statistics
      and plots; Crypto++ comparison included.
- [ ] Security analysis covers cache/timing, two-time pad, malleability, and
      AES-NI.
- [ ] README documents build, CLI, key/IV rules, and limitations.
- [ ] Self-grade rubric and academic-integrity statement included.

## 4. Rubric (100 + 20 bonus)

| Criterion | Points | Full-credit evidence |
|---|---:|---|
| FIPS-197 AES-128 core | 30 | Manual transformations, correct column-major state, correct 10-round key expansion, all vectors pass. |
| CTR mode correctness | 15 | Big-endian counter, partial blocks, arbitrary length, no padding, overflow fail-closed. |
| KATs and cross-validation | 15 | FIPS-197 + SP 800-38A vectors and byte-identical Crypto++/OpenSSL parity. |
| Engineering and documentation | 10 | CMake, modular code, fail-closed CLI, Catch2/CTest, clear README. |
| Performance evaluation | 10 | Required sizes, table-vs-computed analysis, memory/cache/branch discussion, Crypto++ comparison, plots. |
| Security engineering discussion | 20 | Timing/cache attacks, constant-time coding, keystream reuse, malleability, no authentication, AES-NI. |
| **Subtotal** | **100** | |
| Bonus A: AES-192/256 | +10 | Full schedules and FIPS-197 C.2/C.3 KATs. |
| Bonus B: AES-XTS | +5 | Two-key XTS, GF(2^128) tweak, CTS, disk use case, parity test. |
| Bonus C: AES-NI / bitsliced | +5 | Runtime detection + intrinsics or bitsliced core, comparison, constant-time analysis. |

## 5. Infrastructure

### Current layout (`lab02/`)

```
lab02/
  CMakeLists.txt
  README.md
  BENCHMARKS.md
  src/
    AES.hpp/.cpp          pure C++ FIPS-197 AES core (128/192/256)
    modes.hpp/.cpp        CTR required + XTS bonus
    minijson.hpp          zero-dependency JSON parser
    main.cpp              CLI: encrypt, decrypt, bench, --kat
  tests/
    catch.hpp             vendored Catch2 2.13.10
    test_aes.cpp          FIPS-197 vectors
    test_ctr.cpp          SP 800-38A CTR + negative tests
    test_crossval.cpp     Crypto++ oracle parity (CTR + XTS)
    cli_test.sh / cli_test.cmake
  kat/vectors.json
  scripts/                build, benchmark, summarize, plot
  output/                 logs, CSVs, graphs
```

### Dependencies

| Dependency | Version | Notes |
|---|---|---|
| CMake | >= 3.16 (verified 4.2.3 Linux, 4.3.1 Windows) | out-of-source |
| C++ compiler | C++17: GCC 15.2, Clang 21.1.8, MSVC 19.51, MinGW GCC 16.2 | |
| Crypto++ | 8.9.0 | **oracle only**, linked to `test_crossval` |
| Catch2 | 2.13.10 | vendored, offline |
| Python | 3.8+ | optional statistics/plots |

### Commands

Linux:

```bash
cd lab02
scripts/build_linux.sh gcc          # or: scripts/build_linux.sh clang
(cd build/ubuntu-gcc && ctest --output-on-failure)   # expected 4/4
./build/ubuntu-gcc/aestool --kat kat/vectors.json     # expected 9/9
THREADS=4 PIN_CPU=0-3 scripts/run_benchmarks.sh gcc
THREADS=4 PIN_CPU=0-3 BENCH_1GB=0 scripts/run_benchmarks.sh gcc  # skip 1 GiB
```

Windows (x64 Native Tools prompt):

```bat
cd lab02
scripts\build_windows.bat msvc      rem or: gcc / clang
cd build\windows-msvc && ctest -C Release --output-on-failure
cd ..\..
rem Windows benchmark already run on 2026-11-05 at the chosen 1000/100/30.
rem Add RUNS_100MB=500 RUNS_1GB=50 only if the nominal 500/50 is required.
scripts\run_benchmarks.bat msvc
```

Direct benchmark examples (new time-based warm-up, optional threads):

```bash
./lab02/build/ubuntu-gcc/aestool bench --mode ctr --size 1048576 --runs 1000 \
  --warmup 1.0 --threads 4 \
  --out lab02/output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_1048576.csv
./lab02/build/ubuntu-gcc/aestool bench --mode ctr --size 104857600 --runs 500 \
  --warmup 1.0 --threads 4 \
  --out lab02/output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_104857600.csv
```

### Benchmark matrix

AES-128 CTR, sizes 1 MiB / 100 MiB / 1 GiB, nominal runs 1000 / 500 / 50.
This submission uses the chosen 1000 / 100 / 30 (N >= 30); the nominal 500/50
is available with `RUNS_100MB=500 RUNS_1GB=50`. Keep the system-tracking log
(CPU model, governor, MHz, load, RAM, swap, process CPU/RSS, exact command)
next to the CSVs. Generate summary statistics and plots before writing the
report.

### Status / remaining gaps (2026-11-05)

Closed in the implementation pass:

- Windows build/test/KAT/CLI evidence exists for MSVC, MinGW GCC and MinGW
  Clang: 9/9 KAT and 4/4 CTest each.
- Windows UTF-8 paths and `--text` are fixed and covered by `cli_test.cmake`.
- KAT coverage expanded to 9 vectors: CTR AES-128/192/256, partial block,
  FIPS-197 C.1/C.2/C.3, IEEE P1619 XTS and CAVP XTS.
- Unsupported KAT modes are counted as skip failures.
- Cross-validation is mandatory when `ENABLE_CROSSVAL=ON`.
- CLI flags `--encode`, `--decode`, `--nonce`, `--threads`, `--verbose`;
  `bench --verbose` reports progress/ETA; conflicting key/IV sources and
  unknown bench options are rejected.
- Key/round-key zeroization and a computed-S-box reference test.
- Time-based warm-up, CPU pinning in the Linux script, High Performance /
  `/high /affinity` in Windows, and `THREADS=4` (`--threads 4`) for both
  scripts.
- Windows MSVC benchmark for 1 MiB / 100 MiB / 1 GiB with summary, robust
  summary and six plots; see `lab02/output/windows/benchmarks/AUDIT.md`.
- `lab02/REPORT.md` and the audit/regrade in `docs/missing/lab2.md`.

Still open:

- Ubuntu `--kat`/`ctest` logs are not committed; no Linux host was available.
- The committed Ubuntu benchmark CSVs used the old 5-iteration warm-up,
  `powersave`, one thread, and no CPU pinning. Rerun with the new protocol
  before citing them as final.
- Windows uses the chosen 1000/100/30 (N >= 30); the nominal 1000/500/50 is
  optional. It still has no run-time environment log.
- Ubuntu has no 1 GiB plot; Windows has all six plots.
- No Crypto++ **throughput** comparison (cross-validation is correctness only).
- No PDF/DOCX; identity fields are blank.
- Bonus C (AES-NI/bitsliced) is not implemented.
- CTR/XTS are whole-file; streaming is future work.

Treat those as work items, not as done.
