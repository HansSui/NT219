# Gotchas

Last updated: 2026-11-07

- RSA-OAEP plaintext limit is `k - 2*hLen - 2` bytes (318 at 3072 bits, 446 at
  4096). Off-by-one here breaks the automatic hybrid switch.
- Lab 3 requires OpenSSL **4.0.2**; CMake rejects other versions. The build
  script auto-detects `$HOME/CryptoLib/lib/openssl/gcc` or uses
  `OPENSSL_ROOT_DIR`.
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
