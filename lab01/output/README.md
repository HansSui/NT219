# Benchmark artifacts

Everything under `output/` is measurement evidence produced by `aestool bench`
and by the negative-test runs. Raw CSVs and summaries are kept as evidence;
`graph/` folders are produced by the plotting scripts.

## Layout

```
output/
  ubuntu/
    benchmarks/          bench_ubuntu_gcc_*.csv, bench_ubuntu_clang_*.csv
                         summary.csv, summary_tag_overhead.csv, summary_classes.csv
                         graph/  (7 comparison PNGs)
    key_bin/             key and payload files used by the CLI tests
    neg_results/         captured output of the negative tests
  windows/
    benchmarks/          bench_windows_msvc_*.csv + summaries
                         benchmark matrix for the MSVC build
                         graph/  (7 comparison PNGs)
    key_bin/             key.bin used for the Windows run
    test_results/        kat_msvc.txt, ctest_msvc.txt, gui_selftest.txt,
                         environment.txt
    neg_results/         17 negative-test logs, 17 positive round-trip logs
                         (SUMMARY.md has the results table)
  compare/
    cross_platform_summary.csv   per mode/size: both platforms + Windows/Ubuntu ratio
    graph/                       Windows-vs-Ubuntu charts and heatmaps
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
| Platforms | Ubuntu (GCC 15.2 + Clang 21.1.8), Windows (MSVC 19.51, x64) |
| CSV count | 96 Ubuntu + 48 Windows = 144 (144,000 encrypt/decrypt pairs) |

The report also uses a post-hoc 30-block analysis of the same Windows rows to
show that the per-run CI95 understates run-to-run uncertainty; that analysis
does not add new files to this directory.

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
