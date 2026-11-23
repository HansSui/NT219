# Windows benchmark audit - 2026-11-10

Input files:

| File | Rows | Bytes | SHA-256 |
|---|---:|---:|---|
| `bench_windows_msvc_ctr_1048576.csv` | 1000 | 46981 | `88F7FB5F90927A975E1F07D44F26DE7CDFAD8B006BA83606AB33FF08A647A8F2` |
| `bench_windows_msvc_ctr_104857600.csv` | 100 | 5281 | `92AE3D2E71C6869324B8AE92033D364FEE51C3EACF84B77CDEB1B3FA975E9F66` |
| `bench_windows_msvc_ctr_1073741824.csv` | 30 | 1738 | `D0FDB6F348D8B63C424B24705CF1FEA78362D7A599F9D5B1EA9DE6E0F3E6C54A` |

## Validation

- Header matches `run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,throughput_decrypt_MBps`.
- `run` is sequential from 0 to N-1 for every file.
- Every row uses `mode=ctr` and one constant `size_bytes` per file.
- No zero/negative latency or throughput values.
- No duplicate or missing rows in the recorded ranges.
- Statistical summary: `summary.csv`; robust summary: `summary_robust.csv`.
- Plots generated for all three sizes under `graphs/`.

## Results

| Size | Runs | Enc mean | Enc median | Enc CV | Dec mean | Dec median | Dec CV | Enc MiB/s | Dec MiB/s | Stability |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 1 MiB | 1000 | 153831.808 us | 145036.450 us | 11.96% | 153273.536 us | 144445.900 us | 12.13% | 6.580 | 6.605 | ok |
| 100 MiB | 100 | 15583789.522 us | 15668296.000 us | 6.37% | 15572474.350 us | 15749530.750 us | 6.91% | 6.444 | 6.455 | ok |
| 1 GiB | 30 | 119328113.467 us | 121040361.850 us | 8.43% | 120224194.507 us | 122959054.700 us | 8.00% | 8.644 | 8.574 | ok |

All three configurations are marked `ok` by `summary_robust.csv`
(CV <= 0.25 and outlier percentage <= 5%).

## Gaps and caveats

1. **Run counts:** the committed files are the chosen 1000 / 100 / 30.
   The blueprint's nominal 1000 / 500 / 50 is optional; the chosen counts
   satisfy N >= 30 and have stable CVs. The scripts default to 100/30.
2. **No benchmark system log:** the CSVs do not contain CPU model, power
   plan, governor, affinity, priority, warm-up, thread count or exact command.
   `run_benchmarks.bat` printed these to the console but did not tee them to a
   file. Do not invent them retroactively.
3. **Thread count is not in the CSV schema.** The 6.4–8.6 MiB/s results are
   consistent with the 4-thread script, but the files alone cannot prove it.
4. **Windows OpenSSL throughput is now measured** under
   `output/windows/benchmarks/openssl/`. It is noisier than Ubuntu: 1 MiB both
   directions (58.4%/55.7% CV), 100 MiB encrypt (28.2% CV) and 1 GiB decrypt
   (26.2% CV) are flagged `noisy`. Use medians or order-of-magnitude ratios,
   not the means.
5. A stale zero-byte `bench_msvc_ctr_1073741824.csv` from an earlier failed
   direct invocation was removed during this audit; it contained no data.
6. Current host audit (not a run-time capture): Windows 11 Home
   10.0.26300, Dell Vostro 3500, Intel i5-1135G7, 4 cores / 8 logical CPUs,
   2.42 GHz. `powercfg /getactivescheme` currently reports High performance.
   This does not prove the power plan during the recorded run.

## Verdict

The Windows benchmark data is internally valid and usable for the report:
three required sizes, the chosen 1000/100/30 counts, N >= 30 at 100 MiB/1 GiB,
zero flagged outliers, and complete summary/plot artifacts. CVs are 6.4–12.1%
rather than the older sub-1% run, and the 1 GiB point is faster than 100 MiB;
report those as host/affinity effects. It lacks a run-time environment log, so
that limitation must remain visible in the report and regrade.
