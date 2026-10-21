# Ubuntu negative / edge-case test evidence

Generated from the Release GCC build (`build/ubuntu-gcc/aestool`) with a random
32-byte AES-256 key (64-byte key for XTS). Each case has a captured command log
in this directory: the command line, combined stdout/stderr and the exit code.
The `.aestool_nonces` registry used by the nonce-reuse case was created in a
temporary working directory, not in the repository.

| Case | Expected | Exit code | Observed evidence |
|---|---|---:|---|
| `neg_gcm_wrongkey` | reject | 1 | `authentication failed: ... MAC not valid` |
| `neg_gcm_badtag` | reject, no output | 1 | tag failure; `output_created=false` |
| `neg_gcm_tampered_ct` | reject, no output | 1 | ciphertext tamper fails authentication; `output_created=false` |
| `neg_cbc_wrong_iv` | corrupted plaintext (non-AEAD) | 0 | `corrupted=true`; no integrity claim |
| `neg_ctr_wrongkey` | corrupted plaintext (non-AEAD) | 0 | `corrupted=true`; no integrity claim |
| `neg_ctr_tampered_ct` | corrupted plaintext (non-AEAD) | 0 | `corrupted=true`; no integrity claim |
| `neg_truncated` | reject | 1 | `input is too short to contain an aestool header`; `output_created=false` |
| `neg_iv_length` | reject | 1 | `invalid IV/nonce length for CBC: got 8 bytes` |
| `neg_ecb_blocked` | warning + reject | 1 | warning + `ECB is blocked for payloads larger than 16 KiB` |
| `neg_nonce_reuse` | reject second use | 1 | `nonce reuse rejected: this key and nonce were already used` |
| `neg_odd_hex` | reject | 1 | `hex input has an odd number of digits` |
| `neg_xts_short` | reject | 1 | `XTS requires at least 16 bytes` |

Notes:

- Non-AEAD modes cannot detect modification. CBC with the wrong IV, CTR with the
  wrong key and CTR with a flipped ciphertext byte all produced silent
  corruption. This is expected and is why AEAD is required when integrity
  matters.
- AEAD and malformed-header failures leave no plaintext output file: decryption
  writes the output only after parsing and tag verification succeed.
- KAT, ctest and GUI self-test logs are in `../test_results/`.
