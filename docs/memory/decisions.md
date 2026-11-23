# Decisions

Last updated: 2026-11-09

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

## 2026-11-09 Lab 3 accepts any OpenSSL 4.0.x

The exact `VERSION_EQUAL 4.0.2` gate rejected the installed 4.0.3 at
`D:\cryptolibrary`. CMake now requires OpenSSL >= 4.0 and accepts newer 4.0.x
patches; the Linux auto-detect matches any `4.0.N` header. The committed Ubuntu
evidence still records 4.0.2. Supersedes the exact pin below.

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

## 2026-11-09 Corrected Lab 4/6 security semantics

Lab 4 can parse certificate structure without an issuer key, but it cannot
verify authenticity; such a certificate remains unverified and must not be
accepted as trusted. Lab 6 negative tests follow ML-KEM implicit rejection:
altered ciphertext or a wrong key may yield a fallback secret rather than a
distinct error. Tests compare against the expected valid secret and do not
assume an error signal. PQ key serialization must match the selected library's
defined encoding.

## 2026-11-15 Lab 2 Bonus C is AES-NI, AES-128 CTR only

Runtime detection uses `__builtin_cpu_supports("aes")` (GCC/Clang) and CPUID
leaf 1 ECX bit 25 (MSVC); non-x86 builds compile a `false`/throw stub. The fast
path lives behind per-function `target("aes,sse2")`, not a global `-maes`, so
the rest of the library keeps the baseline ISA. AES-192/256, counter-wrap cases
and CPUs without AES-NI stay on the table-based path; the wrap case stays in
software so its existing fail-closed error is shared. Reuse the 176-byte
AES-128 schedule (`AES::round_key_data()`) rather than re-expanding or keeping
the raw key, because `AES` wipes the raw key after expansion. Rejected:
bitslicing (much more code for the same +5), AES-192/256 AES-NI (different key
schedules), and enabling `-maes` globally (breaks non-AES hosts).

## 2026-11-18 Lab 4 uses OpenSSL EVP and an honest chosen-prefix fallback

`hashtool` gets SHA-2/SHA-3/SHAKE from OpenSSL EVP (`EVP_MD_fetch`) and X.509
from libcrypto; only SHA-256 is reimplemented, for the length-extension demo.
Why: the brief allows OpenSSL, and reimplementing SHA-3/SHAKE is a scope
increase with no grading benefit. CMake requires OpenSSL >= 3.0 so both the
course 4.0.2 prefix and Ubuntu's 3.5.5 work. Rejected: Crypto++ (manual PEM/
ASN.1 plumbing) and from-scratch SHA-2/3.

The MD5 certificate artifact is an **identical-prefix** collision: `md5_fastcoll`
runs against the exact TBS prefix, so two TBS values share one MD5 signature.
The +10 bonus asks for chosen-prefix, which costs about 2^39 MD5 work and was
not run on this 4-core host. The report and demo README explicitly do not claim
the +10; the core +20 MD5 PNG demo is separate.
