# Windows benchmark audit - 2026-11-05

Input files:

| File | Rows | Bytes | SHA-256 |
|---|---:|---:|---|
| `bench_windows_msvc_ctr_1048576.csv` | 1000 | 46981 | `2D82BBACA550D4420FCD333C745BDAFA89A3A772FEAED85FFE256E63D6A7BA10` |
| `bench_windows_msvc_ctr_104857600.csv` | 100 | 5281 | `DDC1571FA7D6EDEA9EB4DC91D099558EF0DF83B2AF04179573AE9DBC15AA0745` |
| `bench_windows_msvc_ctr_1073741824.csv` | 30 | 1731 | `59E0EC60B237D22A9D30B0BD676114D148296DB26DD879225B1C73EC97C2ED35` |

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
| 1 MiB | 1000 | 148837.743 us | 146208.300 us | 6.32% | 147471.127 us | 145860.650 us | 6.24% | 6.741 | 6.801 | ok |
| 100 MiB | 100 | 14254751.949 us | 14204528.500 us | 1.33% | 14263820.188 us | 14194372.900 us | 1.37% | 7.016 | 7.012 | ok |
| 1 GiB | 30 | 144547416.503 us | 144153493.400 us | 0.69% | 144574139.337 us | 144385364.750 us | 0.64% | 7.085 | 7.083 | ok |

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
3. **Thread count is not in the CSV schema.** The ~7 MiB/s result is
   consistent with the 4-thread script, but the files alone cannot prove it.
4. **No Crypto++ throughput comparison.** Cross-validation is correctness
   only.
5. A stale zero-byte `bench_msvc_ctr_1073741824.csv` from an earlier failed
   direct invocation was removed during this audit; it contained no data.
6. Current host audit (not a run-time capture): Windows 11 Home
   10.0.26300, Dell Vostro 3500, Intel i5-1135G7, 4 cores / 8 logical CPUs,
   2.42 GHz. `powercfg /getactivescheme` currently reports High performance.
   This does not prove the power plan during the recorded run.

## Verdict

The Windows benchmark data is internally valid and usable for the report:
three required sizes, the chosen 1000/100/30 counts, N >= 30 at 100 MiB/1 GiB,
stable CVs, and complete summary/plot artifacts. It lacks a run-time
environment log, so that limitation must remain visible in the report and
regrade.
