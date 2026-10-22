# Windows negative / edge-case test evidence

Generated from the Release MSVC build (`build/windows-msvc/Release/aestool.exe`)
with a 32-byte AES-256 key and a UTF-8 Vietnamese plaintext. Each case has a
captured command log in this directory. The `.aestool_nonces` registry used by
the nonce-reuse case was created in a temporary working directory, not in the
repository.

| Case | Expected | Exit code | Observed evidence |
|---|---|---:|---|
| `positive_ecb_enc` / `positive_ecb_dec` | round-trip | 0 | warning printed; plaintext matches |
| `positive_cbc_enc` / `positive_cbc_dec` | round-trip | 0 | plaintext matches |
| `positive_ofb_enc` / `positive_ofb_dec` | round-trip | 0 | plaintext matches |
| `positive_cfb_enc` / `positive_cfb_dec` | round-trip | 0 | plaintext matches |
| `positive_ctr_enc` / `positive_ctr_dec` | round-trip | 0 | plaintext matches |
| `positive_xts_enc` / `positive_xts_dec` | round-trip | 0 | plaintext matches |
| `positive_gcm_enc` / `positive_gcm_dec` | round-trip | 0 | plaintext matches |
| `positive_ccm_enc` | encryption | 0 | container written |
| `neg_xts_short` | reject | 1 | `XTS requires at least 16 bytes` |
| `neg_gcm_wrongkey` | reject | 1 | `authentication failed: ... MAC not valid` |
| `neg_gcm_badtag` | reject, no output | 1 | tag failure; `output-created=False` |
| `neg_gcm_tampered_ct` | reject, no output | 1 | tag failure; `output-created=False` |
| `neg_gcm_wrong_aad` | reject | 1 | tag failure |
| `neg_ccm_badtag` | reject, no output | 1 | tag failure; `output-created=False` |
| `neg_cbc_wrong_iv` | corrupted plaintext (non-AEAD) | 0 | `corrupted=True`; no integrity claim |
| `neg_cbc_tampered_ct` | corrupted plaintext or padding failure | 1 | `invalid PKCS #7 block padding found` |
| `neg_iv_length` | reject | 1 | `invalid IV/nonce length for CBC: got 8 bytes` |
| `neg_ecb_blocked` | warning + reject | 1 | warning + `ECB is blocked for payloads larger than 16 KiB` |
| `neg_ecb_allowed` | warning + allow | 0 | warning only; override accepted |
| `positive_ctr_fixed_nonce_1` | first use allowed | 0 | container written |
| `neg_nonce_reuse` | reject second use | 1 | `nonce reuse rejected: this key and nonce were already used` |
| `neg_truncated` | reject | 1 | `ciphertext header lengths are out of range` |
| `neg_odd_hex` | reject | 1 | `hex input has an odd number of digits` |
| `neg_aad_nonaead` | reject | 1 | `AAD is only valid with ccm or gcm` |
| `neg_mode_mismatch` | reject | 1 | `--mode does not match the ciphertext header` |
| `neg_missing_aead` | reject | 1 | `mode GCM is AEAD; pass --aead` |

Notes:

- Non-AEAD modes cannot detect every modification. CBC with the wrong IV
  produced silent corruption because the modification did not break PKCS #7
  padding. This is expected and is exactly why AEAD is required when integrity
  matters.
- AEAD failures leave no plaintext output file: decryption writes the output
  only after the authentication tag has been verified.
- Unix registers `tests/cli_test.sh`; Windows registers the portable
  `tests/cli_test.cmake`. The refreshed MSVC `ctest` log is 2/2 (`aescore` + `cli`),
  and the cases above remain the manual Windows negative-test evidence.
- Wrong-key behaviour for non-AEAD modes is covered by the Ubuntu
  `neg_ctr_wrongkey` case (corrupted plaintext). The Windows GCM wrong-key case
  demonstrates the fail-closed AEAD behaviour instead.
