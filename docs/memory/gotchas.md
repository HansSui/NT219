# Gotchas

Last updated: 2026-11-19

- RSA-OAEP plaintext limit is `k - 2*hLen - 2` bytes (318 at 3072 bits, 446 at
  4096). Off-by-one here breaks the automatic hybrid switch.
- Lab 3 requires OpenSSL **4.0.x or newer**; CMake accepts 4.0.2+ (the Windows
  `D:\cryptolibrary` install is 4.0.3). The build script auto-detects
  `$HOME/CryptoLib/lib/openssl/gcc` or uses `OPENSSL_ROOT_DIR`.
- OpenSSL 4.0.2 `EVP_PKEY_CTX_set0_rsa_oaep_label` takes ownership of an
  `OPENSSL_malloc` buffer; free it only on failure before ownership transfers.
- Decrypt must write nothing on failure, especially after a tag mismatch.
- The GCM tag is not part of its own AAD. Build AAD from the canonical header
  without the `tag` field.
- `wrapped_key`, `iv` and `tag` are base64 in the JSON header but raw bytes in
  the `Envelope` struct; canonical header serialization re-encodes them.
- Base64/hex parsers must reject odd lengths and invalid padding, not silently
  truncate.
- Windows OpenSSL is expected at `D:\cryptolibrary` in this course; use
  `OPENSSL_ROOT_DIR`.
- Lab 1 Crypto++ is static `/MT` on MSVC. Lab 3 links OpenSSL, so the Crypto++
  CRT workaround does not apply.
- Benchmarking 100 MiB × 1000 operations is ~100 GiB; cap block repetitions by
  size and record the block size in the CSV.
- Lab 2 AES-NI: `aeskeygenassist` needs a compile-time round constant; a runtime
  loop / register array segfaults. Expand keys with unrolled calls, or reuse the
  existing schedule. `AES` zeroes its raw key and round keys, so the fast path
  must read `round_key_data()` and never assume the key still exists.
- Lab 2 `--impl ni` is deliberately strict (AES-128 + AES CPU only). Benchmark
  scripts default to `IMPL=soft` so the report baseline reproduces. The
  threaded harness copies worker chunks; at AES-NI speed that copy dominates,
  so compare implementations at `THREADS=1`.
- Lab 4 SHAKE `--outlen` is **bits**, not bytes, and must be a multiple of 8;
  fixed-size hashes reject `--outlen` entirely.
- OpenSSL 4.0 returns `const X509_NAME *` from `X509_get_subject_name`. Build a
  fresh `X509_NAME`, add entries, then call `X509_set_subject_name`; casting
  away const breaks compatibility with 3.5.
- `md5_fastcoll` collision blocks only collide for the exact prefix used. For
  the certificate fallback, compute the TBS prefix offset first and align the
  colliding extension to a 64-byte boundary before invoking fastcoll.
- This shell's `OPENSSL_CONF` points at a missing file. Prefix OpenSSL CLI
  commands with `OPENSSL_CONF=/etc/ssl/openssl.cnf`; `hashtool` does not read
  that variable.
- Lab 4 certificate verification is one link only. `signature verified` in
  `hashtool` never means the chain, root, revocation or hostname were checked.
