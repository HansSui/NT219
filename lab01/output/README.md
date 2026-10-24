# Benchmark artifacts

Everything under `output/` is measurement evidence produced by `aestool bench`
and by the negative-test runs. Raw CSVs and summaries are kept as evidence;
`graph/` folders are produced by the plotting scripts. Paths recorded inside
older logs refer to the build layout used at run time; current builds live under
`lab01/build/` (for example `build/ubuntu-gcc/aestool`).

## Layout

```
output/
  ubuntu/
    benchmarks/          bench_ubuntu_gcc_*.csv
                         summary.csv, summary_tag_overhead.csv, summary_classes.csv
                         bench_environment.txt (controlled-run metadata)
                         graph/  (7 comparison PNGs)
    key_bin/             key and payload files used by the CLI tests
    test_results/        kat_gcc.txt, kat_clang.txt, ctest_*.txt,
                         gui_selftest.txt, environment.txt
    neg_results/         captured output of the negative tests + SUMMARY.md
  windows/
    benchmarks/          bench_windows_msvc_*.csv + summaries
                         summary.csv, summary_robust.csv,
                         summary_classes.csv, summary_tag_overhead.csv
                         pilot/pilot_ctr_1MB.csv (pre-run stability check)
                         bench_environment.txt (AC, load, power, affinity, priority)
                         power_scheme.txt (powercfg scheme captured before the run)
                         graph/  (7 comparison PNGs)
    key_bin/             key.bin used for the Windows run
    test_results/        kat_msvc.txt, ctest_msvc.txt, gui_selftest.txt,
                         non_ascii_path.txt, environment.txt,
                         kat_mingw_{gcc,clang}.txt, ctest_mingw_{gcc,clang}.txt
    manual_checks/       files produced by the non-ASCII path check
    neg_results/         17 negative-test logs, 17 positive round-trip logs
                         (SUMMARY.md has the results table)
  compare/
    cross_platform_summary.csv   per mode/size: both platforms + Windows/Ubuntu ratio
    graph/                       Windows-vs-Ubuntu charts and heatmaps
  package/
    lab01-windows/               staged Windows submission tree (bin, gui, output)
    lab01-windows.zip            archive of that tree; generated, git-ignored
```

Each benchmark CSV has one header plus one row per run:

```
run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,throughput_decrypt_MBps
```

## Matrix

| Dimension | Values |
|---|---|
| Modes | `ecb cbc ofb cfb ctr xts ccm gcm` |
| Payload sizes | 1 KB, 4 KB, 16 KB, 256 KB, 1 MB, 8 MB |
| Runs per CSV | 1000 |
| Platforms | Ubuntu GCC 15.2, Windows MSVC 19.51 x64 |
| CSV count | 48 Ubuntu + 48 Windows = 96 (96,000 encrypt/decrypt pairs) |

Ubuntu Clang 21.1.8 was built and tested but not benchmarked. MinGW GCC 16.2.0
and MinGW Clang 22.1.8 also build and pass KAT/`ctest`; they were not
benchmarked, so the matrix stays at 96 CSVs.

The Windows rows come from the controlled 25/10/2026 run (AC power, High
performance, logical CPU 2, high priority, 5 s warm-up/cooldown, 30 s pilot
warm-up). `summary_robust.csv` flags 5 of the 48 configurations as noisy in at
least one direction (43/48 stable both ways; median CV 0.071 encrypt / 0.068
decrypt). Use the medians or trimmed means for those rows and disclose the
flags. The report also uses a post-hoc 30-block analysis of the same Windows
rows; that analysis does not add new files to this directory.

## Regenerate

From `lab01/`:

```bat
python scripts\summarize_bench.py --dir output\ubuntu\benchmarks
python scripts\plot_compare.py --dir output\ubuntu\benchmarks --out-dir output\ubuntu\benchmarks\graph

python scripts\summarize_bench.py --dir output\windows\benchmarks
python scripts\plot_compare.py --dir output\windows\benchmarks --out-dir output\windows\benchmarks\graph

python scripts\plot_crossplatform.py
```

The cross-platform script defaults to `output/ubuntu/benchmarks` and
`output/windows/benchmarks` and writes to `output/compare/`.

## Reading the comparison

- Throughput ratio (Windows / Ubuntu) above 1 means Windows is faster.
- Latency ratio above 1 means Windows is slower.
- The two platforms ran on different hosts, so treat the ratios as indicative;
  every cross-platform figure says so in its footer.
