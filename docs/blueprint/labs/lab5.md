# Lab 5 — Digital Signatures: ECDSA and RSA-PSS

> **Normative source:** `docs/Lab5.txt`
> **Course core:** `docs/course-objectives.txt`, `docs/GradingSystem.txt`; toolchain: `docs/lab requirement.md`
> **Existing implementation:** none (`lab05/` does not exist yet)
> **Tool:** `sigtool`

## 1. What to do

1. Generate ECDSA key pairs on NIST prime curves:
   - `secp256r1` (P-256) required.
   - `secp384r1` (P-384) for +5.
2. Generate RSA-PSS key pairs with a 3072-bit modulus.
3. Produce and verify **detached** signatures:
   - ECDSA-P256 with SHA-256.
   - ECDSA-P384 with SHA-384 (optional).
   - RSA-PSS-3072 with SHA-256 and salt length = hash length (32 bytes),
     randomized per signature.
4. Use deterministic ECDSA nonces per RFC 6979 unless a written justification
   says otherwise. Random nonce reuse is a fail condition.
5. Implement `keygen`, `sign`, and `verify` with the uniform CLI shape:
   PEM/DER keys, raw/DER/base64 signatures, `--hash` selection.
6. Validate malformed keys, incorrect algorithm identifiers, unsupported
   encodings, and hash mismatches.
7. Add automated unit tests, negative tests, batch verification of N signatures,
   and clear error codes/messages.
8. Benchmark keygen/sign/verify and throughput for ECDSA and RSA-PSS at 1 KiB,
   16 KiB, 1 MiB, and 8 MiB.
9. Write the security discussion: deterministic vs random ECDSA nonces, nonce
   reuse catastrophe, RSA-PSS vs PKCS#1 v1.5, salt role, signature sizes,
   verification vs signing cost, constant-time verification, error leakage,
   timing/fault/side-channel risks.
10. Compare results with Lab 6 (PQC) in the discussion.
11. Optional bonus: formula-level implementation from primitives (+15) and
    security-engineering analysis (timing variance, fault injection, modular
    exponentiation side channels, deterministic nonce protection). Follow the
    single +15 bonus row in the rubric; do not count both activities as +15.

## 2. Requirements

All shared requirements C-01…C-16 in
[`BLUEPRINT.md`](../BLUEPRINT.md#common-core) apply. The points below are
lab-specific additions or overrides.

### 2.1 Algorithms and parameters

| Algorithm | Required settings |
|---|---|
| ECDSA P-256 | NIST `secp256r1`, ~128-bit classical strength; SHA-256 |
| ECDSA P-384 | NIST `secp384r1`, ~192-bit classical strength; SHA-384 (bonus +5) |
| RSA-PSS | 3072-bit modulus, SHA-256, salt length 32 bytes, randomized salt |
| Public exponent | e.g. 65537; document the choice |

### 2.2 CLI and formats

```
sigtool keygen --algo ecdsa-p256 --pub pub.pem --priv priv.pem
sigtool keygen --algo rsa-pss-3072 --pub pub.pem --priv priv.pem
sigtool sign   --algo ecdsa-p256 --in msg.bin --out sig.bin --hash sha256
sigtool verify --algo ecdsa-p256 --in msg.bin --sig sig.bin --pub pub.pem
```

| Item | Requirement |
|---|---|
| Keys | PEM or DER |
| Signatures | raw, DER, or base64 (`--encode`) |
| Hash | SHA-256 required; SHA-384 optional for P-384 |
| Signatures | Detached from the message |
| Batch mode | Verify N signatures |

The brief says the CLI mirrors Lab 6; keep argument names consistent across
Labs 5 and 6.

### 2.3 Validation and negative tests

Must demonstrate:

- Modified message -> verification fails.
- Modified signature -> fails.
- Modified public key -> fails.
- Wrong algorithm identifier -> fails.
- Wrong hash function -> fails.
- Malformed key -> fails.
- Unsupported encoding -> fails.

Include automated unit tests and clear error codes. Fail closed.

### 2.4 Correctness and KATs

- ECDSA and RSA-PSS test vectors from a cited source (NIST CAVP or RFC vectors)
  through `--kat`.
- Round-trip sign/verify for every required algorithm.
- Cross-check signature and key encodings with OpenSSL where useful.
- Unit tests with Catch2/GoogleTest and `ctest`.

### 2.5 Performance

Benchmark for ECDSA and RSA-PSS:

- Key generation latency.
- Sign latency.
- Verify latency.
- Throughput (ops/s).

Message sizes: 1 KiB, 16 KiB, 1 MiB, 8 MiB.

Required analysis:

- Hash-cost dominance for large messages.
- ECDSA vs RSA-PSS verification characteristics.
- Signature-size comparison.
- Memory usage.
- Latency vs message-size plots.
- Comparison with Lab 6 (theoretical comparison is acceptable).

### 2.6 Security discussion

- Deterministic nonces (RFC 6979) vs random nonces.
- Catastrophic nonce reuse (the private key is recoverable).
- Why RSA-PSS is preferred over PKCS#1 v1.5.
- Role of the salt in probabilistic signatures.
- Signature size: ECDSA vs RSA-PSS.
- Verification cost vs signing cost.
- Public exponent choice.
- Which signing/key-generation operations must avoid secret-dependent timing;
  verification inputs are public, so constant-time verification is not
  generally a secrecy requirement. Keep failure behavior clear and non-leaky.
- How improper error handling leaks information.
- Timing attack surface, fault injection, modular-exponentiation side channels,
  and deterministic nonce protection (bonus B).

### 2.7 Bonus

| Bonus | Points | Required work |
|---|---:|---|
| Formula-level implementation | +15 | Implement elliptic-curve point arithmetic, modular inversion, RSA modular exponentiation, and PSS padding from primitives; explain constant-time considerations. |
| Security engineering | +15 | Timing attack surface, fault injection, side-channel risks in modular exponentiation, deterministic nonce protection; basic timing-variance measurement and constant-time library discussion. No exploit required. |

P-384 is a separate +5 in §2.1 but is absent from the source rubric table, which
shows only the +15 advanced block. Record the claimed total and do not count the
advanced block twice.

## 3. What to expect

### Learning outcomes

After this lab you should be able to:

- Generate ECDSA and RSA-PSS key pairs.
- Produce and verify detached signatures.
- Select secure curves, key sizes, hashes, and salt lengths.
- Detect implementation failures and signature misuse.
- Benchmark signing and verification.
- Compare elliptic-curve and RSA-based signatures.

### Definition of done (grader evidence)

- [ ] `sigtool` builds on Ubuntu and Windows with CMake.
- [ ] ECDSA-P256 and RSA-PSS-3072 keygen/sign/verify work.
- [ ] P-384 works if claiming the +5.
- [ ] RFC 6979 or an explicitly justified deterministic nonce scheme is used.
- [ ] RSA-PSS salt is 32 bytes and randomized per signature.
- [ ] KAT/round-trip tests and all negative tests pass; `ctest` passes on both
      OSes.
- [ ] Batch verification works.
- [ ] Benchmarks cover both algorithms at 1 KiB/16 KiB/1 MiB/8 MiB, with
      latency/throughput plots and CIs.
- [ ] README documents key/signature formats, CLI, and limitations.
- [ ] Report covers nonce reuse, PSS vs PKCS#1 v1.5, sizes, performance, and
      constant-time/error-leak issues; Lab 6 comparison included.
- [ ] Self-grade rubric and academic-integrity statement included.

## 4. Rubric (100 + 15 bonus)

| Criterion | Points | Full-credit evidence |
|---|---:|---|
| Correct ECDSA sign/verify | 20 | P-256 with RFC 6979/SHA-256; correct key and signature formats; vectors pass. |
| Correct RSA-PSS sign/verify | 20 | 3072-bit, SHA-256, salt 32, randomized salt; vectors/round-trip pass. |
| Robust key handling and formats | 15 | PEM/DER, raw/DER/base64 signatures, algorithm identifiers, key validation. |
| Negative tests and UX | 10 | All §2.3 cases fail closed with clear, non-leaky errors. |
| Performance study and analysis | 20 | Required message sizes, keygen/sign/verify latency and throughput, plots, interpretation. |
| Cross-platform build and documentation | 5 | CMake, Ubuntu + Windows, README, reproducible commands. |
| Report quality | 10 | Objectives, design, tests, performance, security, Lab 6 comparison, citations. |
| **Subtotal** | **100** | |
| Bonus: advanced implementation / security engineering | +15 | Formula-level primitives or a rigorous side-channel/security analysis with measurements. |

## 5. Infrastructure

### Proposed layout (`lab05/`, not yet present)

```
lab05/
  CMakeLists.txt
  README.md
  BENCHMARKS.md
  src/
    ecdsa.hpp/.cpp        keygen, RFC 6979 nonce, sign, verify
    rsapss.hpp/.cpp       keygen, PSS padding/salt, sign, verify
    keyio.hpp/.cpp        PEM/DER, signature encodings
    minijson.hpp          KAT reader (reuse the lab01/02 pattern)
    main.cpp              CLI: keygen, sign, verify, bench, --kat
  tests/
    catch.hpp
    test_ecdsa.cpp
    test_rsapss.cpp
    test_negative.cpp
    cli_test.sh / cli_test.cmake
  kat/
    signature_vectors.json
  scripts/
    build_linux.sh
    build_windows.bat
    run_benchmarks.sh / .bat
    summarize_bench.py
    plot_bench.py
  output/
    ubuntu/test_results/
    ubuntu/benchmarks/
    windows/test_results/
    windows/benchmarks/
```

### Dependencies

| Dependency | Version | Notes |
|---|---|---|
| CMake | >= 3.16 | match Labs 1-2 |
| C++ compiler | C++17 | GCC/Clang/MSVC/MinGW |
| Crypto++ | 8.9.0 | ECDSA/RSA-PSS, deterministic nonce support if available |
| OpenSSL | 3.x | allowed alternative; useful for key/signature cross-checks |
| Catch2 | 2.13.10 | vendored |
| Python | 3.8+ | statistics and plots |

**Decision to record:** pick Crypto++ or OpenSSL for the implementation and use
the other only for verification. Crypto++ 8.9.0 is the default to match Lab 1.
Confirm the library's RFC 6979 support before committing; if it is missing,
implement the nonce derivation and test it against published vectors.

### Commands (proposed)

```bash
cd lab05
scripts/build_linux.sh gcc
./build/ubuntu-gcc/sigtool keygen --algo ecdsa-p256 --pub pub.pem --priv priv.pem
./build/ubuntu-gcc/sigtool sign   --algo ecdsa-p256 --in msg.bin --out sig.bin --hash sha256
./build/ubuntu-gcc/sigtool verify --algo ecdsa-p256 --in msg.bin --sig sig.bin --pub pub.pem
./build/ubuntu-gcc/sigtool --kat kat/signature_vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
scripts/run_benchmarks.sh gcc
```

```bat
cd lab05
scripts\build_windows.bat msvc
```

### Benchmark matrix

| Algorithm | Operations | Message sizes |
|---|---|---|
| ECDSA-P256 | keygen, sign, verify, ops/s | 1 KiB, 16 KiB, 1 MiB, 8 MiB |
| ECDSA-P384 (bonus) | keygen, sign, verify, ops/s | 1 KiB, 16 KiB, 1 MiB, 8 MiB |
| RSA-PSS-3072 | keygen, sign, verify, ops/s | 1 KiB, 16 KiB, 1 MiB, 8 MiB |

Keygen is expensive; use a smaller repeat count than sign/verify but report the
count and CI.

### Risks / gaps

- No code exists; commands are proposed.
- RFC 6979 library support is the main technical unknown; verify it early.
- Signature encodings differ between ECDSA (raw r||s vs DER) and RSA-PSS (raw
  block); document the default and the `--encode` behavior.
- Batch verification must reject if any single signature fails; test that.
- Do not claim the +15 twice; the rubric has one bonus block even though the
  brief describes two activities.
