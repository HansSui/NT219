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
- 2026-11-09 — Lab 2 plot is now a grouped bar chart comparing encrypt,
  decrypt and total (aggregate both directions) per size; Ubuntu and Windows
  combined summaries regenerated. The six older Windows per-size plots were
  left unchanged.
- 2026-11-09 — Reconciled the six-lab documentation with current implementation
  status; clarified Lab 4 certificate validation, Lab 5 timing language, Lab 6
  ML-KEM implicit-rejection and key-format semantics, and the archive binary
  requirement conflict.
- 2026-11-09 — Regraded Labs 1–3 on Ubuntu (Lab 1 KAT 29/29 ctest 2/2; Lab 2
  KAT 9/9 ctest 4/4; Lab 3 KAT 2/2 ctest 5/5). Converted Lab 3 plots to grouped
  bars with CI/value labels, added labels to Lab 2's summary, reconciled Lab 3
  report/benchmark tables with the committed CSV, and wrote
  `docs/missing/lab3.md`. Windows and report PDFs still unverified.
- 2026-11-09 — Relaxed the Lab 3 OpenSSL gate from exact 4.0.2 to >= 4.0 so the
  installed `D:\cryptolibrary` 4.0.3 works; the Linux auto-detect now accepts any
  4.0.x header. Added `lab03/scripts/run_benchmarks.bat` and filled the Lab 2/3
  report identity. Windows build/benchmark still unrun.
- 2026-11-09 — Fixed MinGW Windows OpenSSL discovery for the nested
  `D:\cryptolibrary\libs\openssl\gcc` prefix and linked the required Windows
  system libraries for Lab 2 oracle targets and Lab 3. Lab 2 and Lab 3 now
  build and pass CTest locally with OpenSSL 4.0.3.
- 2026-11-09 — Fixed `lab01/scripts/package_lab1.py` self-recursion when staging
  `output/` under `output/package/` (the evidence copy now ignores `package`).
  Packaged Lab 1 with the student identity into
  `lab01/output/package/lab01-submission.zip` (5.5 MiB, 255 files); generated
  output is not committed.
