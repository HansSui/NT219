# History

- 2026-11-02 — Blueprint for Labs 1–6 written in `docs/blueprint/`.
- 2026-11-05 — Lab 2 Ubuntu finalized (KAT 9/9, CTest 4/4, OpenSSL cross-check,
  1000/100/30 benchmark). Windows rerun still open.
- 2026-11-06 — Set `.opencode/agents/main.md` as the project primary agent,
  added OpenCode project config and this memory. Read Lab 3 brief and planned
  `lab03/` (OpenSSL implementation).
- 2026-11-07 — Lab 3 implemented in `lab03/` with OpenSSL: keygen PEM/DER/meta,
  OAEP-SHA256 + label, hybrid AES-GCM envelope, KAT 2/2, `ctest` 5/5. Linux
  verified; Windows not run here.
- 2026-11-07 — Lab 3 pinned to OpenSSL 4.0.2: CMake rejects other versions,
  Linux build auto-detects the local 4.0.2 prefix, GCC/Clang tests and the
  benchmark were rerun against `libcrypto.so.4`.
- 2026-11-07 — Lab 3 benchmark protocol: 4 worker threads, `taskset -c 0-3`,
  1000 runs for ≤1 MiB and 100 for 100 MiB, per-size run counts. GCC benchmark
  and plots regenerated.
