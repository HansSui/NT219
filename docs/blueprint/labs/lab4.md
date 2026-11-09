# Lab 4 — Hashing, PKI, and Practical Attacks

> **Normative source:** `docs/Lab4.txt`
> **Course core:** `docs/course-objectives.txt`, `docs/GradingSystem.txt`; toolchain: `docs/lab requirement.md`
> **Existing implementation:** none (`lab04/` does not exist yet)
> **Tool:** `hashtool` plus offline demo material
>
> **Ethics:** defensive education only. Use offline files you own in an isolated
> environment. Do not target real systems, live services, or third-party data.

## 1. What to do

1. Build `hashtool`, a CLI hashing tool covering:
   - SHA-224, SHA-256, SHA-384, SHA-512.
   - SHA3-224, SHA3-256, SHA3-384, SHA3-512.
   - SHAKE128 and SHAKE256 with `--outlen`.
2. Support streamed I/O so multi-GB files work; console output defaults to hex,
   file output defaults to raw binary.
3. Add KATs, algorithm-identifier validation, and fail-closed handling of
   malformed parameters.
4. Parse an X.509 certificate and extract subject, issuer, SPKI algorithm and
   parameters, signature algorithm, validity, key usage, and SANs.
5. Verify the certificate signature with the issuer public key when available.
   If it is unavailable, perform only structural and algorithm-consistency
   checks, report the signature as unverified, and do not accept the
   certificate as trusted or valid.
6. Deploy HTTPS on Apache or Nginx with TLS 1.2 or 1.3, a trusted root (e.g.
   ZeroSSL), and an ECDSA certificate where possible. Save a screenshot/log, the
   config snippet, and a trust-chain explanation.
7. Demonstrate a controlled MD5 collision with `hashclash`: either two benign
   PNGs with the same MD5, or two short C++ programs with the same MD5. Deliver
   both files, the matching digest, and a one-page explanation.
8. Demonstrate a length-extension attack on `MAC = H(k || m)` with `hashpump`
   (or your own implementation for +5): recover `H(k || m || pad || m')`
   without knowing `k`. Deliver the original message, forged message, matching
   MAC, padding diagram, and mitigation.
9. Benchmark SHA-256, SHA-512, SHA3-256, and SHA3-512 at 1 MiB, 100 MiB, and
   1 GiB if feasible; analyze throughput, streaming vs memory-mapped I/O, CPU
   usage, SHA-2 vs SHA-3 differences, and cache/memory effects.
10. Write the required discussion: Merkle-Damgard vs sponge, collision vs
    preimage resistance, XOF use cases, X.509 TBS vs signature, chain of trust,
    why MD5/SHA-1 are banned, certificate transparency, and HMAC.
11. Optional bonuses: custom SHA-256 length extension (+5) and a sandboxed
    chosen-prefix certificate demo (+10).

## 2. Requirements

All shared requirements C-01…C-16 in
[`BLUEPRINT.md`](../BLUEPRINT.md#common-core) apply. The points below are
lab-specific additions or overrides.

### 2.1 Hashing CLI

```
hashtool --algo sha256 --in file.bin
hashtool --algo sha512 --in large.iso --stream
hashtool --algo shake128 --outlen 128 --in file.bin
hashtool --algo shake256 --outlen 64 --in file.bin
```

| Algorithm | Output |
|---|---|
| SHA-224 / SHA-256 / SHA-384 / SHA-512 | 224 / 256 / 384 / 512 bits |
| SHA3-224 / SHA3-256 / SHA3-384 / SHA3-512 | 224 / 256 / 384 / 512 bits |
| SHAKE128 / SHAKE256 | variable via `--outlen` |

Required: KATs, streamed I/O for multi-GB files, hex on console, raw binary to
file, algorithm-identifier validation, fail closed on unsupported parameters.

### 2.2 PKI and certificate analysis

Required extraction:

- Subject.
- Issuer.
- Subject Public Key Info: algorithm and parameters.
- Signature algorithm.
- Validity period.
- Key usage.
- Subject Alternative Names.

Signature verification:

- Verify with the issuer public key when provided.
- If the issuer key is unavailable, check structure and algorithm consistency
  only; report the signature as unverified and do not accept the certificate as
  trusted or valid.

TLS deployment:

- Apache or Nginx.
- TLS 1.2 or TLS 1.3.
- Trusted root (e.g. ZeroSSL).
- ECDSA certificate preferred.
- Deliver a screenshot or logs, a config snippet, and a short trust-chain
  explanation.

### 2.3 Controlled MD5 collision

Choose one:

- Option A: two benign PNG files with the same MD5.
- Option B: two short C++ programs with the same MD5.

Deliver both inputs, the matching MD5 digest, and a one-page explanation of why
MD5 is broken, collision vs preimage, and historical incidents. No real-world
targets.

### 2.4 Length-extension attack

Target: `MAC = H(k || m)`.

Given `H(k || m)` and the length of `k`, compute `H(k || m || pad || m')`
without knowing `k`.

Deliver:

- Original message.
- Forged extended message.
- Matching forged MAC.
- Padding diagram.
- Mitigation explanation: why Merkle-Damgard enables it, why HMAC prevents it,
  and prefix-free constructions.

### 2.5 Validation and negative tests

- Unsupported algorithm identifier -> rejection.
- Invalid `--outlen` (zero, too large, wrong for fixed-size algorithms) ->
  rejection.
- Missing/unreadable input -> failure.
- Malformed certificate -> failure.
- Invalid signature or inconsistent algorithm -> failure.
- Tampered certificate -> failure.
- Attack demos stay in an isolated sandbox.

### 2.6 Performance

Benchmark SHA-256, SHA-512, SHA3-256, SHA3-512 at 1 MiB, 100 MiB, and 1 GiB if
feasible. Analyze:

- Throughput (MB/s).
- Streaming vs memory-mapped I/O.
- CPU utilization.
- SHA-2 vs SHA-3 differences.
- Cache and memory effects.
- Windows vs Linux if possible.

Include performance plots.

### 2.7 Security discussion

- Merkle-Damgard vs sponge construction.
- Collision resistance vs preimage resistance.
- Why SHA-3 differs structurally from SHA-2.
- XOF use cases and domain separation.
- X.509 structure: TBS vs signature.
- Chain of trust and certificate validation.
- Why SHA-1 and MD5 are banned.
- Certificate transparency and CA/B requirements.
- Why length extension works and why HMAC prevents it.

### 2.8 Bonus

| Bonus | Points | Required work |
|---|---:|---|
| Custom length-extension implementation | +5 | Reconstruct the SHA-256 internal state, apply padding, continue compression; explain state manipulation. |
| Chosen-prefix certificate demo | +10 | Sandbox only: MD5/RSA self-signed cert, chosen-prefix collision, second cert with the same MD5 signature, policy mitigations. No operational exploit. |

## 3. What to expect

### Learning outcomes

After this lab you should be able to:

- Implement and benchmark modern hash functions.
- Parse and validate X.509 certificates.
- Demonstrate MD5 collision weaknesses safely.
- Demonstrate length-extension attacks on naive MACs.
- Explain why legacy hashes are insecure.
- Propose mitigations: HMAC, SHA-2/SHA-3, policy controls.

### Definition of done (grader evidence)

- [ ] `hashtool` builds on Ubuntu and Windows and supports all required
      algorithms and SHAKE output lengths.
- [ ] KATs pass for every algorithm; `ctest` passes on both OSes.
- [ ] Streaming mode hashes a multi-GB file without loading it into memory.
- [ ] Certificate parser extracts every required field and verifies or fails
      closed.
- [ ] TLS deployment evidence includes screenshot/log, config snippet, and
      trust-chain explanation.
- [ ] MD5 collision demo includes both files, digest, and explanation.
- [ ] Length-extension demo includes original/forged message, MAC, padding
      diagram, and mitigation.
- [ ] Benchmarks cover all four hashes at the required sizes with plots.
- [ ] README documents CLI, build, dependencies, and limitations.
- [ ] Self-grade rubric and academic-integrity statement included.

## 4. Rubric (100 + 15 bonus)

| Criterion | Points | Full-credit evidence |
|---|---:|---|
| Hash suite correctness and KATs | 25 | All SHA-2/SHA-3/SHAKE variants, correct output sizes, vectors pass. |
| Streaming and large-file performance | 10 | Multi-GB streaming works; performance methodology and analysis are sound. |
| X.509 parsing and signature verification | 25 | All fields extracted, verification/fail-closed path correct, TLS deployment delivered. |
| MD5 collision demo and explanation | 20 | Two files with identical MD5, safe setup, accurate explanation. |
| Length-extension demo and mitigation analysis | 20 | Forged MAC demonstrated, padding explained, HMAC mitigation explained. |
| **Subtotal** | **100** | |
| Bonus: custom length extension | +5 | Manual SHA-256 state reconstruction and compression continuation. |
| Bonus: chosen-prefix certificate demo | +10 | Sandboxed demo plus policy mitigation analysis. |

## 5. Infrastructure

### Proposed layout (`lab04/`, not yet present)

```
lab04/
  CMakeLists.txt
  README.md
  BENCHMARKS.md
  src/
    hash_core.hpp/.cpp     SHA-2, SHA-3, SHAKE wrappers/implementation
    x509_inspect.hpp/.cpp  certificate parse, fields, verify
    main.cpp               CLI: hash, cert, bench, --kat
  tests/
    catch.hpp
    test_hashes.cpp
    test_x509.cpp
    test_cli.sh / .cmake
  kat/
    hash_vectors.json
  demos/
    md5_collision/         two inputs + digest + README
    length_extension/      original/forged message + MAC + padding diagram
    tls/                   config snippet + logs/screenshot
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
| OpenSSL | 3.x | hash and X.509 APIs; `libssl-dev` / `D:\cryptolibrary` |
| Crypto++ | 8.9.0 | optional alternative for hashes |
| Catch2 | 2.13.10 | vendored |
| `hashclash` | current | MD5 collision demo |
| `hashpump` | current | length-extension demo; own implementation earns +5 |
| Python | 3.8+ | statistics and plots |
| Apache or Nginx | current | TLS deployment task |

**Decision to record:** the brief does not require from-scratch hashes. The
default is library-backed SHA-2/SHA-3/X.509 via OpenSSL, which keeps the scope on
correctness, streaming, PKI, and analysis. If the instructor requires an
implementation from scratch, that becomes a separate milestone.

### Commands (proposed)

```bash
cd lab04
scripts/build_linux.sh gcc
./build/ubuntu-gcc/hashtool --algo sha256 --in file.bin
./build/ubuntu-gcc/hashtool --algo shake256 --outlen 64 --in file.bin
./build/ubuntu-gcc/hashtool --kat kat/hash_vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
scripts/run_benchmarks.sh gcc
```

```bat
cd lab04
scripts\build_windows.bat msvc
```

TLS evidence is collected from the Apache/Nginx host and stored under
`demos/tls/`. Attack demos run offline in a VM or container with no network.

### Benchmark matrix

| Algorithm | Sizes | Metric |
|---|---|---|
| SHA-256, SHA-512 | 1 MiB, 100 MiB, 1 GiB if feasible | MB/s, CPU |
| SHA3-256, SHA3-512 | 1 MiB, 100 MiB, 1 GiB if feasible | MB/s, CPU |

Compare streaming vs memory-mapped I/O. Record the hardware/OS/compiler table.

### Risks / gaps

- No code exists; commands are proposed.
- `hashclash` and `hashpump` may not be installed; the +5 option allows a
  custom length-extension implementation, but the collision demo still needs a
  tool or a prepared pair.
- The TLS task depends on a trusted certificate and a reachable host; start it
  early and keep logs.
- A chosen-prefix certificate demo is optional and sandbox-only; do not let it
  delay the required deliverables.
