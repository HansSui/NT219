# Lab 6 — Post-Quantum Signatures and Certificates

> **Normative source:** `docs/Lab6.txt`
> **Course core:** `docs/course-objectives.txt`, `docs/GradingSystem.txt`; toolchain: `docs/lab requirement.md`
> **Existing implementation:** none (`lab06/` does not exist yet)
> **Tool:** `pqtool`

## 1. What to do

1. Generate ML-DSA key pairs:
   - ML-DSA-44 required (~128-bit classical equivalent, Level 2).
   - ML-DSA-65 for +5 (~192-bit, Level 3).
   - ML-DSA-87 is discussed for the Level 5 trade-off.
2. Produce and verify detached ML-DSA signatures over arbitrary messages.
3. Generate ML-KEM key pairs and perform:
   - Encapsulation -> ciphertext + shared secret.
   - Decapsulation -> the same shared secret.
   - A check that the two shared secrets match.
   - For altered ciphertext, test the library's specified implicit-rejection
     behavior; do not assume decapsulation returns an error.
4. Implement `keygen`, `sign`, `verify`, `encaps`, and `decaps` with the same
   CLI shape as Lab 5.
5. Build the post-quantum certificate mini-project:
   - Generate a CA ML-DSA key pair.
   - Sign a subject public key.
   - Verify the certificate signature.
   - Detect tampering.
6. Add automated unit tests, negative tests, and batch verification/batch
   decapsulation timing.
7. Benchmark ML-DSA keygen/sign/verify and ML-KEM keygen/encaps/decaps at
   1 KiB, 16 KiB, 1 MiB, and 8 MiB; analyze hash-cost dominance, signature
   overhead, memory, and comparison with ECDSA/RSA-PSS.
8. Write the discussion: ML-DSA signature size vs security, deterministic
   signing and rejection sampling, ML-KEM IND-CCA and the Fujisaki-Okamoto
   transform, ciphertext size vs RSA key exchange, why ML-KEM is not used for
   signing, hybrid certificates, and migration to PQ TLS.
9. Optional bonus: formula-level ML-DSA/ML-KEM (NTT arithmetic, modular
   reduction, rejection sampling, constant-time coding) and side-channel
   awareness (timing variance, lattice leakage, constant-time NTT, failure
   handling in decapsulation).

## 2. Requirements

All shared requirements C-01…C-16 in
[`BLUEPRINT.md`](../BLUEPRINT.md#common-core) apply. The points below are
lab-specific additions or overrides.

### 2.1 Algorithms and parameters

| Family | Parameter set | Level | Status |
|---|---|---|---|
| ML-DSA | ML-DSA-44 | 2 (~128-bit) | Required |
| ML-DSA | ML-DSA-65 | 3 (~192-bit) | +5 |
| ML-DSA | ML-DSA-87 | 5 (~256-bit) | Discuss |
| ML-KEM | ML-KEM-512 | 2 | Required |
| ML-KEM | ML-KEM-768 | 3 | Discuss/optional |
| ML-KEM | ML-KEM-1024 | 5 | Discuss |

### 2.2 CLI and formats

```
pqtool keygen --algo mldsa-44 --pub pub.pem --priv priv.pem
pqtool keygen --algo mlkem-512 --pub pub.pem --priv priv.pem
pqtool sign    --algo mldsa-44 --in msg.bin --out sig.bin
pqtool verify  --algo mldsa-44 --in msg.bin --sig sig.bin --pub pub.pem
pqtool encaps  --algo mlkem-512 --pub pub.pem --ct ct.bin --ss ss.bin
pqtool decaps  --algo mlkem-512 --priv priv.pem --ct ct.bin --ss ss.bin
```

| Item | Requirement |
|---|---|
| Keys | Use the library's supported serialization. PEM/DER is required only when a defined encoding is supported; otherwise document the binary encoding or a clearly labeled wrapper. |
| Signatures | raw or base64 |
| KEM ciphertexts | binary |
| Optional | JSON structured certificate format |

### 2.3 PQ certificate mini-project

Minimal certificate:

```json
{
  "subject": "your Subject",
  "public_key": "<ML-DSA public key>",
  "issuer": "PQ-CA",
  "signature": "<ML-DSA signature by CA>"
}
```

Tasks:

1. Generate the CA key pair (ML-DSA).
2. Sign the subject public key.
3. Verify the certificate signature.
4. Detect tampering by verifying the certificate signature over a canonical,
   documented encoding of the signed fields.

Discuss:

- Why ML-KEM is not used for signing.
- Hybrid certificates (ECDSA + ML-DSA).
- Migration path to PQ TLS.

### 2.4 Validation and negative tests

Must demonstrate:

- Modified message -> verification fails.
- Modified signature -> fails.
- Modified public key -> fails.
- Modified KEM ciphertext -> decapsulation must not yield the sender's shared
  secret. ML-KEM implementations use implicit rejection and may return a
  pseudorandom fallback secret rather than an error.
- Wrong private key -> shared-secret mismatch; do not require a distinct
  decapsulation error.
- Malformed key/ciphertext -> failure.
- Tampered certificate -> failure.

Include automated unit tests, batch verification, and batch decapsulation timing.

### 2.5 Correctness and KATs

- ML-DSA and ML-KEM known-answer tests from the chosen library's official
  vectors or the FIPS 204 / FIPS 203 test vectors.
- Round-trip sign/verify and encaps/decaps.
- Shared-secret equality check.
- Certificate signature verification and tamper detection.
- Unit tests with Catch2/GoogleTest and `ctest`.

### 2.6 Performance

Measure:

| Operation | Metrics |
|---|---|
| ML-DSA keygen | latency, ops/s |
| ML-DSA sign | latency, ops/s |
| ML-DSA verify | latency, ops/s |
| ML-KEM keygen | latency, ops/s |
| ML-KEM encaps | latency, ops/s |
| ML-KEM decaps | latency, ops/s |

Message sizes: 1 KiB, 16 KiB, 1 MiB, 8 MiB.

Discuss:

- Hash-cost dominance.
- Large signature overhead.
- Memory usage.
- Comparison to ECDSA/RSA-PSS (theoretical comparison is acceptable).
- Batch timing.

### 2.7 Security discussion

- Why PQ signatures are large.
- How lattice cryptography works at a high level.
- ML-DSA: deterministic signing, rejection sampling, failure probability.
- ML-KEM: IND-CCA security, Fujisaki-Okamoto transform.
- Ciphertext size vs RSA key exchange.
- Why ML-KEM is a KEM, not a signature scheme.
- Hybrid certificates and migration to PQ TLS.
- Side-channel risks: timing, lattice leakage, constant-time NTT, decapsulation
  failure handling.

### 2.8 Bonus

| Bonus | Points | Required work |
|---|---:|---|
| Formula-level ML-DSA or ML-KEM | +15 | NTT-based polynomial arithmetic, modular reduction, rejection sampling, constant-time considerations. |
| Side-channel awareness | +15 | Timing-variance measurement, input-dependent timing, lattice leakage, constant-time NTT, decapsulation failure handling. Partial validation is acceptable; no exploit. |

The rubric has one +15 bonus block; choose one path or split the credit
explicitly.

## 3. What to expect

### Learning outcomes

After this lab you should be able to:

- Generate ML-DSA and ML-KEM key pairs.
- Sign and verify detached PQ signatures.
- Encapsulate and decapsulate a shared secret.
- Explain PQC parameter selection and security levels.
- Benchmark PQ signatures and KEMs.
- Build and verify a simple PQ certificate structure.

### Definition of done (grader evidence)

- [ ] `pqtool` builds on Ubuntu and Windows with CMake.
- [ ] ML-DSA-44 keygen/sign/verify works; ML-DSA-65 works if claiming +5.
- [ ] ML-KEM-512 keygen/encaps/decaps works and valid shared secrets match;
      altered-ciphertext tests follow the library's implicit-rejection behavior.
- [ ] PQ certificate signs, verifies, and detects tampering.
- [ ] KAT/round-trip tests and all negative tests pass; `ctest` passes on both
      OSes.
- [ ] Batch verification and batch decapsulation timing work.
- [ ] Benchmarks cover both families at 1 KiB/16 KiB/1 MiB/8 MiB with plots
      and CIs.
- [ ] README documents the PQ library, parameter sets, key format, CLI, and
      limitations.
- [ ] Report covers sizes, security levels, KEM vs signature, hybrid certs, and
      migration; ECDSA/RSA-PSS comparison included.
- [ ] Self-grade rubric and academic-integrity statement included.

## 4. Rubric (100 + 15 bonus)

| Criterion | Points | Full-credit evidence |
|---|---:|---|
| Correct ML-DSA sign/verify | 25 | ML-DSA-44 vectors/round-trip pass; ML-DSA-65 if claiming +5. |
| Correct ML-KEM encaps/decaps | 20 | ML-KEM-512 shared secrets match; tampered ciphertext fails. |
| Robust key handling and formats | 15 | PEM/DER or documented PQ encoding, algorithm identifiers, validation. |
| Certificate implementation | 10 | CA signs subject key; verify and tamper detection work. |
| Negative tests and UX | 10 | All §2.4 cases fail closed with clear errors. |
| Performance study and analysis | 15 | Keygen/sign/verify/encaps/decaps at required sizes, plots, comparison. |
| Cross-platform build and documentation | 5 | CMake, Ubuntu + Windows, README, reproducible commands. |
| Report quality | 10 | Objectives, design, tests, performance, security, migration, citations. |
| **Stated subtotal** | **110** | The brief labels this "100 + Bonus" but its table sums to 110. |
| Bonus: advanced implementation / side channels | +15 | Formula-level NTT/rejection sampling or measured side-channel analysis. |

> **Rubric discrepancy:** the Lab 6 table sums to 110, and ML-DSA-65 is listed
> as +5 outside the table. Keep the individual weights, ask the TA which
> criterion absorbs the extra 10 (or normalize pro rata), and do not silently
> drop a criterion. Record the resolution in the report's self-grade section.

## 5. Infrastructure

### Proposed layout (`lab06/`, not yet present)

```
lab06/
  CMakeLists.txt
  README.md
  BENCHMARKS.md
  src/
    mldsa.hpp/.cpp        keygen, sign, verify
    mlkem.hpp/.cpp        keygen, encaps, decaps
    pqcert.hpp/.cpp       JSON certificate sign/verify/tamper check
    keyio.hpp/.cpp        key and signature encoding
    minijson.hpp          JSON parser (reuse the lab01/02 pattern)
    main.cpp              CLI: keygen, sign, verify, encaps, decaps, cert, bench, --kat
  tests/
    catch.hpp
    test_mldsa.cpp
    test_mlkem.cpp
    test_pqcert.cpp
    test_negative.cpp
    cli_test.sh / cli_test.cmake
  kat/
    pq_vectors.json
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
| PQC library | **not specified by the brief** | Default: liboqs (pinned) or PQClean; record the version and parameter sets |
| Catch2 | 2.13.10 | vendored |
| Python | 3.8+ | statistics and plots |

**Decision to record:** the brief requires ML-DSA and ML-KEM but does not name
a library or a key-encoding standard. Use a pinned, audited implementation
(liboqs or PQClean) and document:

- Exact library version and build flags.
- Which parameter sets are enabled.
- How PQ keys are armored/encoded as "PEM/DER" (raw base64 armor with a
  documented label is acceptable if no standard OID is available).
- Whether the library provides official KAT vectors or whether FIPS 204/203
  vectors are used.

### Commands (proposed)

```bash
cd lab06
scripts/build_linux.sh gcc
./build/ubuntu-gcc/pqtool keygen --algo mldsa-44 --pub pub.pem --priv priv.pem
./build/ubuntu-gcc/pqtool sign   --algo mldsa-44 --in msg.bin --out sig.bin
./build/ubuntu-gcc/pqtool verify --algo mldsa-44 --in msg.bin --sig sig.bin --pub pub.pem
./build/ubuntu-gcc/pqtool encaps --algo mlkem-512 --pub pub.pem --ct ct.bin --ss ss.bin
./build/ubuntu-gcc/pqtool decaps --algo mlkem-512 --priv priv.pem --ct ct.bin --ss ss.bin
./build/ubuntu-gcc/pqtool --kat kat/pq_vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
scripts/run_benchmarks.sh gcc
```

```bat
cd lab06
scripts\build_windows.bat msvc
```

### Benchmark matrix

| Family | Operations | Sizes |
|---|---|---|
| ML-DSA-44 (and 65 if bonus) | keygen, sign, verify, ops/s | 1 KiB, 16 KiB, 1 MiB, 8 MiB |
| ML-KEM-512 (and 768 if included) | keygen, encaps, decaps, ops/s | 1 KiB, 16 KiB, 1 MiB, 8 MiB |

Record signature/ciphertext sizes and shared-secret generation throughput.
Keygen is expensive; use fewer repeats than sign/verify and report the count.

### Risks / gaps

- No code exists; commands are proposed.
- PQC library availability is the main blocker. Confirm the environment before
  designing the build.
- PQ key encoding is not standardized in the brief; document the chosen armor
  and keep it stable across tests.
- Decapsulation failure behavior must be explicit: a tampered ciphertext should
  fail, not silently return a wrong secret, unless the API's implicit-rejection
  design is documented and tested.
- The +15 bonus is one block even though the brief lists two activities; do not
  double-count.
