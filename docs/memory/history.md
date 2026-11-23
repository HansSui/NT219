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
- 2026-11-10 — Lab 3 Windows evidence: `build_windows.bat` now saves CTest and
  KAT logs under `output/windows/test_results/`; MSVC 5/5 + KAT 2/2 and MinGW
  GCC 5/5 + KAT 2/2 are committed. Manual OAEP added (OAEP 36/9, hybrid 36/6).
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
- 2026-11-10 — Synced docs with the committed Windows work: Lab 2 benchmark
  rerun (6.58/6.44/8.64 MiB/s) and partly noisy OpenSSL throughput, Lab 3
  manual OAEP + CTR+HMAC (regrade 99 core + 13 bonus), refreshed `AUDIT.md`,
  READMEs, blueprints and missing audits. Corrected stale identity/PDF,
  Windows-log and MinGW-test-suite claims. Re-ran Linux GCC builds: Lab 1
  `ctest` 2/2, Lab 2 `ctest` 4/4, Lab 3 `ctest` 5/5.
- 2026-11-11 — Rechecked Lab 3 `BENCHMARKS.md`/`REPORT.md` against the committed
  `summary_gcc.csv` and `summary_msvc.csv`; both tables were stale after the
  later benchmark reruns. Updated all rows, ratios and GB/s figures, and
  recorded the remaining Windows/MinGW evidence gaps.
- 2026-11-15 — Expanded `lab02/REPORT.md` and `lab03/REPORT.md` to match the
  Lab 1 report structure (requirements mapping, testing/benchmark methodology,
  robust statistics, security threat model, rubric/checklist, lessons,
  evidence catalog, integrity statement) and normalized metadata/section
  formatting across all three reports. All numbers were traced to the committed
  CSVs/logs; Windows caveats (pre-OpenSSL CTest logs, no run-time log, stale
  MinGW logs) remain explicit. Re-ran Linux verification: Lab 1 ctest 2/2,
  Lab 2 ctest 4/4 + KAT 9/9, Lab 3 ctest 5/5 + KAT 2/2.
- 2026-11-15 — Embedded benchmark figures in the reports and added the missing
  own-vs-OpenSSL comparison. New `lab02/scripts/plot_compare.py` reads both
  `summary_robust.csv` files per platform and writes
  `graphs/compare_openssl_vs_own_{ubuntu,windows}.png` (median throughput, log
  scale, speedup in tick labels). `lab02/REPORT.md` now shows the two summary
  charts plus the two comparison charts; the stale "six per-size Windows plots"
  claim was removed (those PNGs are deleted). `lab03/REPORT.md` embeds its six
  existing RSA/hybrid plots and its benchmark commands were corrected to the
  scripts' actual `--input/--out` usage. No numbers were regenerated; the
  comparison charts are plotted from the committed summaries. The two Lab 2
  summary PNGs were regenerated as well: the Windows one was stale (showed the
  pre-rerun 6.7/7.0/7.1 MiB/s), and `plot_bench.py` now leaves headroom so the
  legend no longer covers bar labels.
- 2026-11-15 — Implemented Lab 2 Bonus C with AES-NI for AES-128 CTR
  (`src/aesni.*`, runtime CPUID/`__builtin_cpu_supports`, 4 blocks per loop,
  software fallback for 192/256, non-x86 and counter wrap). Added
  `bench --impl auto|soft|ni`, an AES-NI-vs-software equivalence test, and
  `scripts/plot_aesni.py`. GCC 15.2 and Clang 21.1 pass `ctest` 4/4 and KAT
  9/9 (unit 370 assertions/12 cases); new logs in
  `output/ubuntu/test_results/*_2026-11-15.txt`. 1-thread comparison at
  1 MiB/100 MiB/1 GiB: 87.4x/67.2x/77.6x encrypt, 83.0x/65.5x/72.6x decrypt,
  evidence under `output/ubuntu/benchmarks/aesni/1t/`. Updated `REPORT.md`
  (§3.4, §6.7, §7.7, rubric 120), `BENCHMARKS.md`, `README.md`, the benchmark
  scripts (`IMPL`, `--impl`) and the Lab 2 audit/blueprint. No Windows AES-NI
  run; MSVC `__cpuid` path is compiled but unverified.
- 2026-11-18 — Lab 4 implemented in `lab04/` (`hashtool`): OpenSSL EVP
  SHA-2/SHA-3/SHAKE with `--outlen` in bits, stream/mmap hashing, 31-vector KAT,
  X.509 extraction with one-link signature verification, own SHA-256
  length-extension, MD5 PNG collision and identical-prefix certificate
  fallback, plus benchmarks/plots. Ubuntu GCC 15.2 and Clang 21.1.8 with
  OpenSSL 4.0.2 pass `ctest` 5/5; system OpenSSL 3.5.5 compatibility build also
  5/5. Benchmark (GCC 4.0.2, warm tmpfs, 1000/100/30): 1 GiB SHA-256
  1370–1440 MB/s, SHA-512 639–663, SHA3-256 386–405, SHA3-512 216. Evidence in
  `lab04/output/ubuntu/`. Open: real TLS host, Windows MSVC/MinGW, full
  chosen-prefix +10 (fallback only; not claimed).
- 2026-11-19 — Lab 4 benchmark analysis added as an executed Jupyter notebook
  (`lab04/analysis/benchmark_analysis.ipynb`): table, ranking and the four
  plots, importing `scripts/plot_bench.py` so it cannot drift from the committed
  PNGs. Executed with `uvx --from jupyterlab --with matplotlib`; no new repo
  dependency and no pandas. `BENCHMARKS.md`/`README.md` point to it.
