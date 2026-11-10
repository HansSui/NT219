# Lab 3 report — RSA-OAEP and hybrid encryption

**Student:** Nguyễn Công Tuấn  
**Student ID:** 25522012  
**Class:** NT219.R11.ANTT  
**Updated:** 10/11/2026

## 1. Objective and outcomes

The goal is a `rsatool` CLI that generates RSA keys of at least 3072 bits,
implements RSA-OAEP with SHA-256 and MGF1-SHA-256 (optional label), and switches
automatically to an authenticated AES-256-GCM envelope when the plaintext
exceeds the RSA limit. Keys are stored as PEM and DER with a metadata JSON file.
The implementation must fail closed on malformed or tampered input and report
benchmarks for 3072 vs 4096 and for hybrid data sizes.

Delivered: `lab03/` with a C++17 CMake project, OpenSSL-backed core, CLI,
Catch2 tests, two independent OAEP KAT vectors, CLI negative tests, benchmark
CSV/summary/plots, and this report. The optional bonus primitives are also
implemented at library level and covered by the same test suite: manual OAEP
encoding/decoding and an AES-256-CTR + HMAC-SHA256 comparison.

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

The implementation library is OpenSSL 4.0.x because the brief allows it and it
provides OAEP labels, AES-GCM, PEM/DER and key validation without custom ASN.1
code. Ubuntu evidence used 4.0.2; the build requires 4.0 or newer and the
Windows scripts target 4.0.3. The 4.0.2 CLI was used only as an independent
cross-check.

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

### 2.6 Manual OAEP bonus

The CLI keeps the OpenSSL OAEP path as its default. For the bonus,
`rsa::oaepEncryptManual` and `rsa::oaepDecryptManual` implement the RFC 8017
EME-OAEP transform directly:

- `lHash = SHA-256(label)`;
- `DB = lHash || PS || 0x01 || M`, with zero padding to `k - hLen - 1` bytes;
- `dbMask = MGF1(seed, k - hLen - 1)` and `seedMask = MGF1(maskedDB, hLen)`;
- `EM = 0x00 || (seed XOR seedMask) || (DB XOR dbMask)`.

OpenSSL supplies the SHA-256 digest, the random seed and the raw RSA
exponentiation (`EVP_PKEY_encrypt`/`decrypt` with `RSA_NO_PADDING`). The tests
prove interoperability in both directions: a manually encoded ciphertext
decrypts with OpenSSL OAEP, and an OpenSSL OAEP ciphertext decrypts with the
manual decoder. They also cover the 318-byte limit, wrong label, wrong key,
tampered ciphertext and short input.

Decoding must not reveal whether the padding was valid. A padding oracle can
turn one bit of "valid/invalid" feedback into plaintext recovery, so the
decoder compares `lHash` with `CRYPTO_memcmp`, scans the whole `DB` for the
`0x01` separator instead of returning at the first mismatch, and accumulates
all failures before throwing one generic error. This is best-effort
constant-time code: the raw RSA operation and the final plaintext copy still
have data-dependent costs, so production use should keep the OpenSSL OAEP
path. The manual path exists to demonstrate the construction and its failure
checks, not to replace the audited library implementation.

### 2.7 AES-CTR + HMAC comparison

`hybrid::aesCtrHmacEncrypt` and `aesCtrHmacDecrypt` implement
encrypt-then-MAC with a 64-byte key split into a 32-byte AES-256 key and a
32-byte HMAC-SHA256 key. They use a 16-byte IV and a 32-byte tag. The tag
covers the IV, a 64-bit AAD length, the AAD and the ciphertext, so moving bytes
between fields or changing the IV is detected. Decryption recomputes the tag,
compares it with `CRYPTO_memcmp`, and only then runs AES-CTR; a failed tag
returns the same generic error and no plaintext.

The trade-off against AES-256-GCM is:

| Property | AES-256-GCM | AES-256-CTR + HMAC-SHA256 |
|---|---|---|
| Passes over data | One AEAD pass | CTR pass plus HMAC pass |
| Tag / expansion | 16-byte tag | 32-byte tag |
| Key material | One 32-byte key | Separate 32-byte encryption and MAC keys |
| Nonce/IV | 12-byte nonce; reuse is catastrophic | 16-byte counter block; reuse is catastrophic |
| AAD support | Native | Included explicitly in the MAC input |
| Failure check | Library tag verification | Constant-time HMAC comparison |
| Typical strength | Fast with AES-NI/PCLMUL | Simple, widely available, no GHASH dependency |

GCM is the default here because it authenticates the envelope header directly
as AAD and is a single well-audited primitive. CTR+HMAC is a useful comparison
where GCM is unavailable or where a separate MAC construction is preferred,
but it needs two keys and a larger tag and still requires strict nonce
uniqueness. The comparison is implemented and tested, not exposed as a CLI
mode; no authentication-failure timing study was run, so no timing claim is
made.

## 3. Test methodology

- **Unit tests (Catch2):** OAEP round-trip, label handling, limit boundary,
  wrong label/key, tampered/malformed ciphertext, PEM/DER round-trip, weak key
  rejection; manual OAEP round-trip and OpenSSL interop in both directions,
  manual limit and fail-closed cases; hybrid round-trip at 1 KiB and 1 MiB,
  envelope metadata, tampered payload/tag/IV/wrapped key/header, wrong
  key/label, AES-GCM AAD/tag failures, AES-CTR+HMAC round-trip and tampered
  tag/ciphertext/IV/AAD; hex/base64 parser and envelope validation negatives.
- **KAT:** `kat/oaep_vectors.json` has two RSA-OAEP-SHA256 vectors (empty and
  non-empty label) generated independently with OpenSSL 4.0.2 `pkeyutl`; the
  runner prints PASS/FAIL and a summary.
- **CLI test:** keygen 3072, PEM/DER/metadata existence, direct and hybrid
  round-trip, base64, wrong label/key, tampered payload/header, malformed
  ciphertext, 2048-bit rejection, odd-length hex, KAT summary. When the OpenSSL
  CLI is present it also validates the PEM files and decrypts an OpenSSL-
  generated OAEP ciphertext.
- **Result:** `ctest` 5/5 (oaep, hybrid, negative, kat, cli) passed on Ubuntu
  26.04 with GCC 15.2.0 and OpenSSL 4.0.2; KAT 2/2, and the unit binaries
  report OAEP 36 assertions/9 cases, hybrid 36/6 and negative 20/5. Clang
  21.1.8 rebuilds and passes the same five tests.

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
cases, mean/median/sample SD/95% CI. Ubuntu: GCC 15.2 `-O3`, OpenSSL 4.0.2,
Intel i5-1135G7. Windows: MSVC 19.51, same matrix. Full tables and plots:
`BENCHMARKS.md`, `output/ubuntu/benchmarks/graph/` and
`output/windows/benchmarks/graph/`.

| Case | 3072 | 4096 |
|---|---:|---:|
| Key generation (mean) | 186.41 ms | 604.71 ms |
| Encrypt 32 B (mean) | 0.052 ms | 0.105 ms |
| Decrypt 32 B (mean) | 1.198 ms | 2.637 ms |

| Hybrid (3072) | 1 KiB | 1 MiB | 100 MiB |
|---|---:|---:|---:|
| Encrypt mean | 0.065 ms | 1.772 ms | 251.621 ms |
| Encrypt throughput (4 threads) | 64.3 MB/s | 2457.7 MB/s | 1675.1 MB/s |
| Decrypt mean | 1.316 ms | 1.774 ms | 171.222 ms |
| Decrypt throughput (4 threads) | 3.2 MB/s | 2450.7 MB/s | 2465.2 MB/s |

Windows MSVC (2026-11-10, same matrix):

| Case | 3072 | 4096 |
|---|---:|---:|
| Key generation (mean) | 330.93 ms | 829.22 ms |
| Encrypt 32 B (mean) | 0.079 ms | 0.109 ms |
| Decrypt 32 B (mean) | 1.765 ms | 3.409 ms |

| Hybrid (3072) | 1 KiB | 1 MiB | 100 MiB |
|---|---:|---:|---:|
| Encrypt mean | 0.113 ms | 1.969 ms | 194.323 ms |
| Encrypt throughput (4 threads) | 36.3 MB/s | 2194.1 MB/s | 2182.7 MB/s |
| Decrypt mean | 1.865 ms | 2.915 ms | 136.146 ms |
| Decrypt throughput (4 threads) | 2.2 MB/s | 1458.4 MB/s | 3115.9 MB/s |

AES-256-GCM alone reached about 2.4 GB/s aggregate at 1 KiB, 15.0 GB/s at
1 MiB and 2.6 GB/s at 100 MiB on Ubuntu (encryption; decryption
2.8/22.6/4.3 GB/s); on Windows the same 1 MiB encryption case reached
3.7 GB/s aggregate (917 MB/s per operation). The ordering is the same on both
platforms: RSA private operations are much slower than public operations
because they use the secret exponent and CRT, and 4096-bit RSA is about
2.0–3.2× slower than 3072. At small sizes hybrid cost is dominated by the RSA
wrap/unwrap; at 100 MiB the AES pass dominates. Absolute Windows-vs-Ubuntu
values differ with OS, compiler and OpenSSL build, so they are observations,
not a controlled cross-platform experiment. The Windows run has no environment
log; the CSV records four threads. The tool holds the whole payload in memory,
so peak memory is a few times the input size (more with four concurrent
operations); a streaming envelope would be the next step.

## 5. Discussion

Correctness issues found and fixed during development: the OAEP limit off-by-one
boundary is tested at exactly 318 bytes; the envelope AAD is computed before the
ciphertext is attached but includes the correct `payload_len`, so re-serializing
the parsed header on decrypt yields the same bytes; and decrypt failures were
made to produce no output file. The header is canonicalized rather than
re-hashed from the raw JSON, so a semantically equal JSON reordering does not
break authentication.

Remaining limitations: unencrypted private key files, no streaming, no
password-based key protection, no second crypto library for byte-for-byte
ciphertext comparison, and no committed Windows KAT/CTest log (the Windows
benchmark proves the MSVC build and run). OpenSSL CLI interop covers formats
and OAEP. The manual OAEP decoder is best-effort constant-time and is not the
CLI default; the AES-CTR+HMAC comparison is tested but not benchmarked and has
no failure-timing measurements. Forward secrecy is absent by design; adding
ephemeral ECDHE or ML-KEM would change the envelope and is out of scope for
this lab.

## 6. Conclusion

RSA-OAEP with SHA-256 is the correct direct-encryption primitive within its
strict size limit. Real data needs hybrid encryption: AES-256-GCM supplies
speed and authenticated encryption, while RSA-OAEP protects only the short
session key. The envelope is versioned, self-describing and header-
authenticated, and every negative path tested fails closed without leaking the
failed check. The bonus implementations show the same OAEP construction and an
encrypt-then-MAC alternative at the primitive level without changing the
CLI's default path.

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
| Performance study | 15 | 15 | Two platforms, 4 threads pinned to CPUs 0–3, 1000/100 runs, CI, CSV, grouped plots |
| Cross-platform build/docs | 5 | 5 | Linux GCC/Clang verified; Windows MSVC build and benchmark run committed |
| Report quality | 10 | 9 | complete Markdown report with identity; PDF/DOCX and archive still absent |
| **Subtotal** | **100** | **99** | |
| Bonus: manual OAEP | +5 | 5 | MGF1/XOR/manual decode with OpenSSL raw RSA; bidirectional interop, limit and fail-closed tests; constant-time rationale documented |
| Bonus: hybrid analysis | +10 | 8 | TLS/envelope/forward-secrecy discussion plus implemented/tested AES-CTR+HMAC comparison; no failure-timing study |
| **Total** | **115** | **112** | 99 core + 13 bonus |

## Academic integrity

The implementation was written for this lab. OpenSSL 4.0.x (4.0.2 on Ubuntu)
is used as the permitted crypto library, Catch2 2.13.10 is vendored, and the
OAEP KAT vectors were generated independently with OpenSSL `pkeyutl` as
documented. Test keys in the repository are test-only. No third-party systems
or data were targeted.
