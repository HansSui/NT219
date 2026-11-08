# Lab 3 report — RSA-OAEP and hybrid encryption

## 1. Objective and outcomes

The goal is a `rsatool` CLI that generates RSA keys of at least 3072 bits,
implements RSA-OAEP with SHA-256 and MGF1-SHA-256 (optional label), and switches
automatically to an authenticated AES-256-GCM envelope when the plaintext
exceeds the RSA limit. Keys are stored as PEM and DER with a metadata JSON file.
The implementation must fail closed on malformed or tampered input and report
benchmarks for 3072 vs 4096 and for hybrid data sizes.

Delivered: `lab03/` with a C++17 CMake project, OpenSSL-backed core, CLI,
Catch2 tests, two independent OAEP KAT vectors, CLI negative tests, benchmark
CSV/summary/plots, and this report.

## 2. Design and security

### 2.1 Parameters

| Item | Choice |
|---|---|
| RSA modulus | 3072 bits (4096 for comparison); < 3072 rejected |
| OAEP hash / MGF | SHA-256 / MGF1-SHA-256 |
| OAEP label | Optional, authenticated by OAEP |
| Hybrid data cipher | AES-256-GCM |
| Hybrid IV | 96 bits, fresh random per message |
| Hybrid tag | 128 bits, verified before plaintext is returned |
| Key formats | PEM (PKCS#8 private, SPKI public) and DER (PKCS#1 private, SPKI public) |
| Metadata | `creation_time`, `modulus_bits`, `hash`, key paths |

The implementation library is OpenSSL 4.0.2 because the brief allows it and it
provides OAEP labels, AES-GCM, PEM/DER and key validation without custom ASN.1
code. The build requires and links the 4.0.2 library; the 4.0.2 CLI is used only
as an independent cross-check.

### 2.2 Why OAEP rather than PKCS#1 v1.5

RSA without padding is deterministic and malleable. PKCS#1 v1.5 encryption
padding is also deterministic and its error behavior enabled Bleichenbacher's
adaptive chosen-ciphertext attack; a padding oracle can recover plaintext.
OAEP randomizes the encoding with a seed and a hash/MGF construction. Under the
random-oracle model RSA-OAEP is IND-CCA2: without the private key, an adversary
cannot distinguish ciphertexts even with a decryption oracle, and cannot forge a
valid ciphertext for an unseen message. The label binds the ciphertext to a
context and is itself authenticated by the OAEP transform.

### 2.3 Plaintext limit

For modulus `k` bytes and hash `hLen = 32`, direct OAEP accepts
`mLen <= k - 2*hLen - 2`. At 3072 bits (`k = 384`) that is 318 bytes; at 4096
bits (`k = 512`) it is 446 bytes. The CLI checks this boundary before
encryption and switches to hybrid mode above it. RSA cannot encrypt large files
directly because the modulus is a fixed-size integer: every RSA operation is
one exponentiation over a value smaller than `k`, and OAEP consumes most of `k`
for padding and the seed.

### 2.4 Hybrid envelope

```text
RSAENV1\n
{"version":1,"mode":"RSA-OAEP-AES-GCM","rsa_modulus":3072,"hash":"SHA-256",
 "label":"...","wrapped_key":"...","iv":"...","payload_len":N,"tag":"..."}\n
<raw AES-GCM ciphertext>
```

Encryption generates a random 32-byte AES key and a 12-byte IV, wraps the key
with RSA-OAEP under the optional label, encrypts the data with AES-256-GCM, and
stores the ciphertext after the header. The canonical header without `tag` is
the GCM AAD. Any change to the mode, modulus, hash, label, wrapped key, IV or
payload length therefore changes the AAD and fails tag verification. The tag is
checked before any plaintext is written. The format is versioned and
self-describing; decryption never guesses direct vs hybrid from a length.

This is the same envelope pattern used by TLS (asymmetric key exchange/wrap,
symmetric record protection) and PGP (public-key encrypted session key,
symmetric message encryption). The envelope does not provide forward secrecy:
compromise of the long-term RSA private key later exposes every wrapped session
key. Production protocols add ephemeral ECDHE (or a KEM such as ML-KEM) so the
session key is never recoverable from the long-term key alone.

### 2.5 Failure handling

`decrypt` validates the encoding, envelope structure, key size, label, RSA
unwrap and GCM tag, and reports only `error: decryption failed`. A failed
decryption writes no output file. This prevents an attacker from learning which
check failed (a padding/tag oracle). AES-GCM tag comparison is performed by
OpenSSL's library implementation, which is the accepted library-backed
constant-time path; no custom tag comparison is used.

## 3. Test methodology

- **Unit tests (Catch2):** OAEP round-trip, label handling, limit boundary,
  wrong label/key, tampered/malformed ciphertext, PEM/DER round-trip, weak key
  rejection; hybrid round-trip at 1 KiB and 1 MiB, envelope metadata, tampered
  payload/tag/IV/wrapped key/header, wrong key/label, AES-GCM AAD/tag failures;
  hex/base64 parser and envelope validation negatives.
- **KAT:** `kat/oaep_vectors.json` has two RSA-OAEP-SHA256 vectors (empty and
  non-empty label) generated independently with OpenSSL 4.0.2 `pkeyutl`; the
  runner prints PASS/FAIL and a summary.
- **CLI test:** keygen 3072, PEM/DER/metadata existence, direct and hybrid
  round-trip, base64, wrong label/key, tampered payload/header, malformed
  ciphertext, 2048-bit rejection, odd-length hex, KAT summary. When the OpenSSL
  CLI is present it also validates the PEM files and decrypts an OpenSSL-
  generated OAEP ciphertext.
- **Result:** `ctest` 5/5 (oaep, hybrid, negative, kat, cli) passed on Ubuntu
  26.04 with GCC 15.2.0 and OpenSSL 4.0.2; KAT 2/2.

Reproduce:

```bash
cd lab03
scripts/build_linux.sh gcc
(cd build/ubuntu-gcc && ctest --output-on-failure)
./build/ubuntu-gcc/rsatool --kat kat/oaep_vectors.json
```

## 4. Performance

Method: four worker threads pinned to CPUs 0–3, 1 s warm-up per worker,
1000 runs for RSA and for AES/hybrid 1 KiB/1 MiB, 100 runs for the 100 MiB
cases, mean/median/sample SD/95% CI,
GCC 15.2 `-O3`, OpenSSL 4.0.2 library, Intel i5-1135G7. Full table and plots:
`BENCHMARKS.md`, `output/ubuntu/benchmarks/`.

| Case | 3072 | 4096 |
|---|---:|---:|
| Key generation (mean) | 207.04 ms | 687.47 ms |
| Encrypt 32 B (mean) | 0.055 ms | 0.108 ms |
| Decrypt 32 B (mean) | 1.316 ms | 3.090 ms |

| Hybrid (3072) | 1 KiB | 1 MiB | 100 MiB |
|---|---:|---:|---:|
| Encrypt mean | 0.081 ms | 1.887 ms | 274.844 ms |
| Encrypt throughput (4 threads) | 52.0 MB/s | 2250.1 MB/s | 1538.4 MB/s |
| Decrypt mean | 1.421 ms | 2.017 ms | 185.481 ms |
| Decrypt throughput (4 threads) | 3.0 MB/s | 2204.5 MB/s | 2274.3 MB/s |

AES-256-GCM alone reached about 2.2 GB/s aggregate at 1 KiB, 11.3 GB/s at
1 MiB and 2.4 GB/s at 100 MiB (encryption; decryption 2.8/20.8/3.6 GB/s),
which is why bulk data must not use RSA. RSA private operations are much slower
than public operations because they use the secret exponent and CRT; 4096-bit
RSA is about 2.0–3.3× slower than 3072 in this four-thread run. At small sizes
hybrid cost is dominated by the RSA wrap/unwrap; at 100 MiB the AES pass
dominates. The tool holds the whole payload in memory, so peak memory is a few
times the input size (more with four concurrent operations); a streaming
envelope would be the next step.

## 5. Discussion

Correctness issues found and fixed during development: the OAEP limit off-by-one
boundary is tested at exactly 318 bytes; the envelope AAD is computed before the
ciphertext is attached but includes the correct `payload_len`, so re-serializing
the parsed header on decrypt yields the same bytes; and decrypt failures were
made to produce no output file. The header is canonicalized rather than
re-hashed from the raw JSON, so a semantically equal JSON reordering does not
break authentication.

Remaining limitations: unencrypted private key files, no streaming, no
password-based key protection, and no second crypto library for byte-for-byte
ciphertext comparison. OpenSSL CLI interop covers formats and OAEP. Forward
secrecy is absent by design; adding ephemeral ECDHE or ML-KEM would change the
envelope and is out of scope for this lab.

## 6. Conclusion

RSA-OAEP with SHA-256 is the correct direct-encryption primitive within its
strict size limit. Real data needs hybrid encryption: AES-256-GCM supplies
speed and authenticated encryption, while RSA-OAEP protects only the short
session key. The envelope is versioned, self-describing and header-
authenticated, and every negative path tested fails closed without leaking the
failed check.

## Appendix — CLI examples

```bash
rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem
rsatool encrypt --in small.bin --pub pub.pem --out small.ct --label ctx
rsatool decrypt --in small.ct --priv priv.pem --out small.out --label ctx
rsatool encrypt --in large.bin --pub pub.pem --out large.env --label ctx
rsatool decrypt --in large.env --priv priv.pem --out large.out --label ctx
rsatool --kat kat/oaep_vectors.json
```

## Self-grade rubric

| Criterion | Max | Self-score | Evidence |
|---|---:|---:|---|
| Correct RSA-OAEP (SHA-256) | 20 | 20 | OAEP/MGF1-SHA256, label, limit tests, KAT 2/2 |
| Correct hybrid envelope | 20 | 20 | auto switch, AES-256-GCM, 96-bit IV, wrapped key, authenticated decrypt |
| Key management and formats | 15 | 15 | PEM + DER + metadata for 3072/4096; weak keys rejected |
| Negative tests / secure errors | 15 | 15 | all §2.4 cases, no output on failure, generic decrypt error |
| Performance study | 15 | 14 | 4 threads pinned to CPUs 0–3, 1000/100 runs, CI, CSV, three plots |
| Cross-platform build/docs | 5 | 4 | Linux verified; Windows scripts present, not run here |
| Report quality | 10 | 9 | this report + README + BENCHMARKS |
| **Subtotal** | **100** | **97** | |
| Bonus: manual OAEP | +5 | 0 | not implemented (library OAEP) |
| Bonus: hybrid analysis | +10 | 6 | TLS/envelope/forward-secrecy discussion; no AES-CTR+HMAC timing study |

## Academic integrity

The implementation was written for this lab. OpenSSL 4.0.2 is used as the
permitted crypto library, Catch2 2.13.10 is vendored, and the OAEP KAT vectors
were generated independently with OpenSSL `pkeyutl` as documented. Test keys in
the repository are test-only. No third-party systems or data were targeted.
