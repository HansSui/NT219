# Lab 3 — RSA-OAEP and Hybrid Encryption

> **Normative source:** `docs/Lab3.txt`
> **Course core:** `docs/course-objectives.txt`, `docs/GradingSystem.txt`; toolchain: `docs/lab requirement.md`
> **Existing implementation:** `lab03/` (OpenSSL 4.0.2, GCC 15.2)
> **Tool:** `rsatool`

## 1. What to do

1. Generate RSA key pairs of **at least 3072 bits**, plus RSA-4096 for the
   performance comparison. Save each key as PEM and DER, and write a metadata
   JSON file.
2. Implement **RSA-OAEP with SHA-256 and MGF1-SHA-256**, including an optional
   OAEP label.
3. Implement direct RSA-OAEP encryption/decryption for messages that fit
   `mLen <= k - 2hLen - 2`.
4. Implement **hybrid encryption** for larger messages:
   - Generate a random 256-bit AES key.
   - Encrypt the data with AES-256-GCM and a 96-bit IV.
   - Wrap the AES key with RSA-OAEP.
   - Emit a structured envelope with the wrapped key, IV, tag, and algorithm
     metadata.
5. Make the CLI switch to hybrid mode automatically when the plaintext exceeds
   the direct RSA limit.
6. Implement `keygen`, `encrypt`, and `decrypt`; validate malformed ciphertext,
   wrong labels, unsupported key sizes, and bad encodings.
7. Add automated unit tests and negative tests; all failures must be fail
   closed and must not leak which check failed.
8. Benchmark RSA keygen/encrypt/decrypt for 3072 vs 4096, AES-GCM throughput,
   and hybrid encryption at 1 KiB/1 MiB/100 MiB.
9. Write the security discussion: OAEP vs PKCS#1 v1.5, the plaintext size
   limit, why RSA cannot encrypt large files, envelope design, authenticated
   decryption, and the forward-secrecy limitation of pure RSA.
10. Optional bonuses: manual OAEP padding, and a deeper hybrid security analysis
    (TLS/ECDHE/ML-KEM migration, AES-CTR+HMAC comparison, failure-timing
    variance).

## 2. Requirements

All shared requirements C-01…C-16 in
[`BLUEPRINT.md`](../BLUEPRINT.md#common-core) apply. The points below are
lab-specific additions or overrides.

### 2.1 Algorithms and parameters

| Parameter | Required setting |
|---|---|
| RSA modulus | >= 3072 bits; add 4096 for comparison |
| OAEP hash | SHA-256 |
| MGF | MGF1 with SHA-256 |
| OAEP label | Optional, must be authenticated by the OAEP transform |
| Hybrid data cipher | AES-256-GCM |
| Hybrid IV | 96 bits |
| Hybrid tag | Verified before returning plaintext |
| Key formats | PEM and DER |
| Metadata | JSON: `creation_time`, `modulus_bits`, `hash` |

### 2.2 CLI and formats

```
rsatool keygen  --bits 3072 --pub pub.pem --priv priv.pem
rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin --label optional
rsatool decrypt --in ct.bin --priv priv.pem --out msg.bin
```

| Item | Requirement |
|---|---|
| Ciphertext | raw, hex, or base64 (`--encode`) |
| Envelope header | JSON |
| Keys | PEM or DER |
| Key generation | Writes PEM, DER, and metadata JSON; document the metadata path/flag you choose |
| Oversized input | Automatic hybrid mode |

### 2.3 Hybrid envelope

Header example:

```json
{
  "mode": "RSA-OAEP-AES-GCM",
  "rsa_modulus": 3072,
  "hash": "SHA-256",
  "wrapped_key": "<base64>",
  "iv": "<base64>",
  "tag": "<base64>"
}
```

Payload: AES-256-GCM ciphertext. The envelope must be unambiguous (versioned or
self-describing) so decryption never guesses the format.

### 2.4 Validation and negative tests

Must demonstrate:

- Altered RSA ciphertext -> decryption fails.
- Altered AES-GCM ciphertext -> tag failure.
- Wrong private key -> failure.
- Wrong OAEP label -> failure.
- Tampered envelope header -> failure.
- Malformed ciphertext -> failure.
- Unsupported key size -> failure.
- Incorrect encoding -> failure.

Use automated tests and clear, non-leaky error messages. Fail closed on every
parsing error.

### 2.5 Correctness and KATs

- RSA-OAEP test vectors (NIST CAVP or an equivalent cited source) through the
  `--kat` runner.
- Round-trip tests for direct OAEP and hybrid mode.
- Cross-check PEM/DER output with OpenSSL (`openssl pkey`, `openssl rsautl` or
  `openssl pkeyutl`) where useful.
- Unit tests with Catch2/GoogleTest and `ctest`.

### 2.6 Performance

RSA:

- Key generation time.
- Encryption time.
- Decryption time.
- 3072 vs 4096 comparison.

Hybrid:

- AES-GCM encryption throughput.
- Hybrid encryption time at 1 KiB, 1 MiB, 100 MiB.

Required analysis:

- RSA decryption vs encryption cost.
- Key-size vs performance trade-off.
- Hybrid efficiency advantage.
- Why symmetric encryption dominates throughput.
- Memory and CPU considerations.
- Latency and throughput plots with 95% CIs.

### 2.7 Security discussion

- Why OAEP is IND-CCA2 secure.
- Why PKCS#1 v1.5 encryption is insecure.
- Plaintext size limit `mLen <= k - 2hLen - 2`.
- Why RSA cannot encrypt large files directly.
- Envelope design and the TLS/PGP analogy.
- Authenticated decryption and constant-time tag verification
  (library-backed is acceptable).
- Forward secrecy: pure RSA lacks it; discuss ECDHE or ML-KEM.
- Error handling that does not leak failure reasons.

### 2.8 Bonus

| Bonus | Points | Required work |
|---|---:|---|
| Manual OAEP padding | +5 | Implement MGF1, XOR masking, and padding validation; explain constant-time decoding. |
| Hybrid security analysis | +10 | Compare envelope vs direct RSA, explain TLS hybrid design, analyze forward-secrecy limits, propose ECDHE/ML-KEM, and optionally compare AES-CTR+HMAC and failure-timing variance. |

## 3. What to expect

### Learning outcomes

After this lab you should be able to:

- Generate and store RSA key pairs safely.
- Implement RSA-OAEP with SHA-256.
- Respect plaintext size limits and design an envelope format.
- Detect and handle cryptographic failures.
- Benchmark RSA across key sizes.
- Explain why hybrid encryption is mandatory in practice.

### Definition of done (grader evidence)

- [ ] `rsatool` builds on Ubuntu and Windows with CMake.
- [ ] Keygen writes PEM, DER, and metadata JSON for 3072 and 4096 bits.
- [ ] Direct OAEP round-trip and KAT vectors pass.
- [ ] Hybrid envelope round-trip works above the RSA limit.
- [ ] Tampered ciphertext, tag, header, label, key, and encoding all fail
      closed.
- [ ] `ctest` passes on both OSes.
- [ ] Benchmarks cover RSA 3072/4096 and hybrid 1 KiB/1 MiB/100 MiB, with
      statistics and plots.
- [ ] README documents the CLI, envelope format, key formats, and limitations.
- [ ] Report covers OAEP, hybrid design, performance, and forward secrecy.
- [ ] Self-grade rubric and academic-integrity statement included.

## 4. Rubric (100 + 15 bonus)

| Criterion | Points | Full-credit evidence |
|---|---:|---|
| Correct RSA-OAEP (SHA-256) | 20 | 3072-bit OAEP with MGF1-SHA-256, optional label, vectors/round-trip pass. |
| Correct hybrid envelope (AES-GCM + RSA) | 20 | Automatic switch, AES-256-GCM, 96-bit IV, wrapped key, authenticated decrypt. |
| Key management and formats | 15 | PEM + DER + metadata JSON, 3072/4096, clear key handling. |
| Negative tests and secure error handling | 15 | All §2.4 cases pass, fail closed, errors do not leak the failed check. |
| Performance study and analysis | 15 | RSA and hybrid benchmarks, CIs, plots, correct interpretation. |
| Cross-platform build and documentation | 5 | CMake, Ubuntu + Windows, README, reproducible commands. |
| Report quality | 10 | Objectives, design, tests, performance, security, limitations, citations. |
| **Subtotal** | **100** | |
| Bonus: manual OAEP | +5 | Working MGF1/masking/validation and constant-time discussion. |
| Bonus: hybrid security analysis | +10 | Depth beyond implementation, TLS/forward-secrecy/PQC migration analysis. |

## 5. Infrastructure

### Layout (`lab03/`)

```
lab03/
  CMakeLists.txt
  README.md
  BENCHMARKS.md
  REPORT.md
  src/
    rsa.hpp/.cpp          keygen, OAEP encrypt/decrypt, label, PEM/DER
    hybrid.hpp/.cpp       AES-256-GCM envelope, header parse/serialize/AAD
    util.hpp              hex/base64/JSON/file helpers
    minijson.hpp          small JSON parser (reused from Lab 1)
    main.cpp              CLI: keygen, encrypt, decrypt, bench, --kat
  tests/
    catch.hpp
    test_oaep.cpp
    test_hybrid.cpp
    test_negative.cpp
    cli_test.sh / cli_test.cmake
  kat/
    oaep_vectors.json
  scripts/
    build_linux.sh
    build_windows.bat
    run_benchmarks.sh
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
| C++ compiler | C++17: GCC 15.2, Clang 21.1.8, MSVC 19.51, MinGW GCC 16.2 | |
| Crypto++ | 8.9.0 | not used for Lab 3 (no PEM helpers in the installed build) |
| OpenSSL | 4.0.2 (required) | **chosen implementation library**; CLI used for cross-checks |
| Catch2 | 2.13.10 | vendored |
| Python | 3.8+ | statistics and plots |

**Decision to record:** the brief allows Crypto++ or OpenSSL. Pick one as the
implementation library and use the other only for verification. The default in
this blueprint is Crypto++ 8.9.0, with the OpenSSL CLI for format checks.

### Commands (proposed, mirroring Labs 1-2)

```bash
cd lab03
scripts/build_linux.sh gcc
./build/ubuntu-gcc/rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem
./build/ubuntu-gcc/rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin
./build/ubuntu-gcc/rsatool decrypt --in ct.bin --priv priv.pem --out msg.bin
./build/ubuntu-gcc/rsatool --kat kat/oaep_vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
scripts/run_benchmarks.sh gcc
```

```bat
cd lab03
scripts\build_windows.bat msvc
```

### Benchmark matrix

| Case | Sizes / variants | Metric |
|---|---|---|
| RSA keygen | 3072, 4096 bits | latency (ms), ops/s |
| RSA encrypt | small messages, 3072/4096 | latency, throughput |
| RSA decrypt | small messages, 3072/4096 | latency, throughput |
| AES-256-GCM | 1 KiB, 1 MiB, 100 MiB | MB/s |
| Hybrid | 1 KiB, 1 MiB, 100 MiB | latency, MB/s |

Keep the environment table with every run. The committed protocol uses four
pinned workers, 1000 runs for RSA and <=1 MiB payloads, and 100 runs for
100 MiB; reduce RSA keygen repeats if wall-clock time is constrained.

### Risks / gaps

- Ubuntu is verified; Windows build/test evidence still needs a Windows host.
- The envelope must stay versioned; retrofitting a format later breaks stored
  ciphertext.
- OAEP label handling is easy to get wrong; the tests cover non-empty and wrong
  labels.
- "Automatic hybrid" uses the `k - 2*hLen - 2` threshold; the README and report
  record it.
- Library choice changes key serialization; OpenSSL 4.0.2 is pinned in CMake,
  the build scripts, README, report and memory. Builds on other OpenSSL versions
  are rejected on purpose.

### Status 2026-11-09

Ubuntu GCC 15.2 + OpenSSL 4.0.2 re-verified: KAT 2/2, `ctest` 5/5, unit
counts 24/7 (OAEP), 27/5 (hybrid), 20/5 (negative). The three plots are now
grouped bars with 95% CI and value labels, and `BENCHMARKS.md`/`REPORT.md`
tables were reconciled with the committed `summary_gcc.csv`. Windows build,
KAT, CTest and benchmark remain unrun; the report identity/PDF/archive is
still missing. See `docs/missing/lab3.md`.
