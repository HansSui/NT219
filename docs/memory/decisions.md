# Decisions

Last updated: 2026-11-07

## 2026-11-02 Standalone labs

Each lab is a separate CMake project with no shared library. Why: the grader
builds labs independently, and a shared crypto core adds coupling without a
requirement. Rejected: a top-level `common/` library.

## 2026-11-06 Lab 3 uses OpenSSL, not Crypto++

Lab 3 explicitly allows Crypto++ or OpenSSL. OpenSSL 4.0.2 is already the
course oracle and provides RSA-OAEP labels, AES-256-GCM, PEM/DER and key checks
without custom ASN.1 or PEM code. OpenSSL CLI remains the independent format and
ciphertext cross-check. Rejected: Crypto++ 8.9.0 because the installed package
has no PEM helpers and would force hand-written PEM/DER plus OAEP plumbing.

## 2026-11-07 Lab 3 pins OpenSSL 4.0.2

The course environment's 4.0.2 install is at
`$HOME/CryptoLib/lib/openssl/gcc`; Ubuntu's system `libssl-dev` is 3.5.5.
Lab 3 now requires **4.0.2** in CMake and fails configuration on other versions.
`scripts/build_linux.sh` auto-detects the 4.0.2 prefix and honors
`OPENSSL_ROOT_DIR`; tests and benchmarks link `libcrypto.so.4`. Why: the user
requires one known library version for all Lab 3 builds/evidence.

## 2026-11-07 Lab 3 benchmark is pinned and four-threaded

`run_benchmarks.sh` defaults to `taskset -c 0-3` and `THREADS=4`. Runs are
1000 for RSA and for AES/hybrid up to 1 MiB, 100 for 100 MiB. Each worker times
independent operations, so `elapsed_ms` is per-op latency under four-way
concurrency and aggregate throughput is `MB/s × 4`. Why: the course protocol
asks for 1000 rounds per block, reduced for large payloads; four workers keep
the 100 MiB cases practical while exposing memory-bandwidth contention.

## 2026-11-06 Envelope is self-describing and header-authenticated

`RSAENV1\n` + canonical JSON header + raw ciphertext. The canonical header
(without `tag`) is the AES-GCM AAD. Why: any header tamper fails the tag, and a
stored ciphertext never depends on an out-of-band guess. Rejected: bare JSON
file with the ciphertext base64-encoded inside it (larger, less stream-friendly).
