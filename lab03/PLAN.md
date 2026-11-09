# Lab 3 plan

Status: implemented and verified on Ubuntu (2026-11-07). Windows scripts are
present but not run on this host.

Library decision: **OpenSSL 4.0.x or newer (4.0.2+)**. The brief allows Crypto++ or OpenSSL;
OpenSSL owns RSA-OAEP labels, AES-256-GCM, PEM/DER and key checks, so it removes
custom PEM/ASN.1 code. OpenSSL CLI is the independent cross-check.
See `docs/memory/decisions.md`.

## Steps

1. **M0 walking skeleton** — `lab03/CMakeLists.txt`, core library, `rsatool`
   `keygen/encrypt/decrypt` for direct OAEP, one round-trip CLI test.
2. **Key management** — PEM + DER + metadata JSON for 3072/4096; load both
   formats; reject keys below 3072 bits.
3. **Hybrid envelope** — AES-256-GCM with 96-bit IV, RSA-OAEP-wrapped key,
   `RSAENV1` versioned header, header as GCM AAD, automatic switch over the
   direct limit.
4. **Validation** — fail closed on tampered RSA ciphertext, AES ciphertext,
   tag, header, label, wrong key, malformed input, bad encoding, bad key size.
5. **Tests/KAT** — Catch2 unit + negative tests, `ctest`, JSON KAT runner,
   shell CLI test. OpenSSL CLI cross-check for PEM/DER and OAEP ciphertext.
6. **Benchmarks** — RSA keygen/encrypt/decrypt 3072 vs 4096; AES-GCM and hybrid
   at 1 KiB/1 MiB/100 MiB; four worker threads pinned to CPUs 0–3; 1000 runs
   for ≤1 MiB and 100 for 100 MiB; CSV + summary + plots + 95% CI.
7. **Docs** — `README.md`, `BENCHMARKS.md`, `REPORT.md` (OAEP vs PKCS#1 v1.5,
   size limit, envelope design, authenticated decryption, forward secrecy).

## Exit criteria

- `scripts/build_linux.sh gcc` builds and `ctest` passes.
- KAT and CLI negative cases pass; failed decrypt writes no output.
- `keygen` writes PEM, DER and metadata for 3072 and 4096.
- Benchmarks produce CSVs and a summary with mean/median/stddev/95% CI.
- README documents CLI, envelope, key formats and limitations.
