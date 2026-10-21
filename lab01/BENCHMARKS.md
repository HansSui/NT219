# Batch benchmarks

`scripts/run_benchmarks.sh` (Linux) and `scripts/run_benchmarks.bat` (Windows)
run every mode at every required payload size, with **1000 runs per CSV** by
default:

- Modes: `ecb cbc ofb cfb ctr xts ccm gcm`
- Sizes: `1024 4096 16384 262144 1048576 8388608` bytes (1 KB ... 8 MB)
- Runs: `1000` per size
- Output: 48 CSV files per compiler/platform, each with 1000 rows

## Current matrix status

| Platform | Compiler | CSVs | Status |
|---|---|---:|---|
| Ubuntu | GCC 15.2 | 48 | measured |
| Ubuntu | Clang 21.1.8 | 48 | measured |
| Windows | MSVC 19.51 x64 | 48 | measured |
| Windows | MinGW GCC/Clang | 0 | scripts supported, not run |

The measured 144 CSVs and their summaries are committed under `output/`. The
report (`REPORT.md`) adds a post-hoc 30-block confidence-interval analysis on the
Windows rows: with 1,000 single-run samples the per-run CI95 reflects only
within-run timer noise, not the run-to-run drift visible at block level. Keep
that distinction when quoting CI95 from `summary.csv`.

The 8 MB case processes about 8 GB for encryption and 8 GB for decryption per
mode, so a full run takes minutes. Start with a smaller smoke run if needed.
Each completed CSV has one header plus 1000 run rows (1001 lines). ECB prints a
security warning for every invocation; the runner logs stderr to `bench.log`
instead of repeating the warning in the terminal.

## Linux

```bash
scripts/build_linux.sh gcc
scripts/run_benchmarks.sh gcc
scripts/run_benchmarks.sh clang
```

Output goes to `output/ubuntu/benchmarks/`:

```
bench_ubuntu_gcc_gcm_1KB.csv
bench_ubuntu_gcc_gcm_4KB.csv
...
bench_ubuntu_gcc_ecb_8MB.csv
```

Each CSV has:

```
run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,throughput_decrypt_MBps
```

The runner also writes statistics tables when Python is available:

```
output/ubuntu/benchmarks/summary.csv
output/ubuntu/benchmarks/summary_tag_overhead.csv
output/ubuntu/benchmarks/summary_classes.csv
```

`summary.csv` contains mean, median, standard deviation, CI95 and coefficient of
variation for every compiler/mode/size. `summary_tag_overhead.csv` compares GCM
and CCM against CTR. `summary_classes.csv` aggregates stream, block, AEAD and
non-AEAD classes.

Generate comparison plots after the run:

```bash
PLOT=1 scripts/run_benchmarks.sh gcc
# or, from existing CSVs:
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
python3 scripts/plot_compare.py --dir output/ubuntu/benchmarks --out-dir output/ubuntu/benchmarks/graph
```

This writes seven combined PNGs into `graph/`:

```
comparison_throughput_by_mode_1MB.png
comparison_throughput_vs_size.png
comparison_latency_vs_size.png
comparison_latency_box_16KB.png
comparison_stream_vs_block.png
comparison_aead_vs_nonaead.png
comparison_tag_overhead.png
```

Use `scripts/plot_bench.py` only if you want the per-CSV run-by-run plots
instead of the comparison charts.

## Windows

```bat
scripts\build_windows.bat msvc
scripts\run_benchmarks.bat msvc
```

Output goes to `output\windows\benchmarks\`. Use `gcc` or `clang` instead of
`msvc` for the MinGW builds.

## Cross-platform comparison (Windows vs Linux)

`scripts/plot_crossplatform.py` pools all runs per mode/size from a Linux and a
Windows directory and writes comparison charts plus heatmaps:

```bat
python scripts\plot_crossplatform.py ^
    --linux-dir output/ubuntu/benchmarks ^
    --windows-dir output/windows/benchmarks ^
    --out-dir output/compare/graph
```

`--windows-dir` defaults to `output/windows/benchmarks`. Relative paths are
resolved against the repository root, so the script can be started from any
directory.

It writes into `output/compare/graph/`:

```
cross_throughput_by_mode_1MB.png
cross_latency_by_mode_16KB.png
cross_throughput_vs_size.png
cross_latency_vs_size.png
heatmap_throughput_ubuntu.png
heatmap_throughput_windows.png
heatmap_throughput_ratio_windows_vs_ubuntu.png
heatmap_latency_ratio_windows_vs_ubuntu.png
```

and the underlying table into `output/compare/cross_platform_summary.csv`
(per mode/size: both platforms' throughput and latency plus the
Windows/Ubuntu ratio). Throughput ratios above 1 mean Windows is faster;
latency ratios above 1 mean Windows is slower. Runs from different hosts are
not a controlled experiment, which the figure footers state explicitly.

The default bar charts use the 1 MB payload (`--bar-size`) and the latency
chart uses 16 KB (`--latency-size`); both sizes are adjustable.

## Overrides

Both scripts accept environment variables:

| Variable | Default | Purpose |
|---|---|---|
| `RUNS` | `1000` | runs per CSV |
| `WARMUP` | `1` | warm-up seconds per invocation |
| `THREADS` | `1` | benchmark worker threads (`--threads`); keep 1 for report data |
| `PIN_CPUS` | unset | Linux `taskset` CPU list, e.g. `0` for the default single-thread run |
| `SIZES` | all six sizes | space-separated byte sizes |
| `MODES` | all eight modes | space-separated modes |
| `OUT` | `output/<platform>/benchmarks` | output directory |
| `KEY` | `output/<platform>/key_bin/key.bin` | 32-byte key file |
| `PLOT` | `0` | set to `1` to generate plots |

Examples:

```bash
# quick smoke test
RUNS=10 SIZES=1024 MODES=gcm scripts/run_benchmarks.sh gcc

# 30 repetitions for a shorter report run
RUNS=30 scripts/run_benchmarks.sh gcc
```

The script creates a random 32-byte key if the key file does not exist. ECB runs
automatically add `--allow-ecb`. Each benchmark operation uses a fresh
counter-derived IV, so the benchmark itself does not reuse nonces.

### Controlled CPU run

For report data, fix the governor and pin the benchmark process, then rerun the
whole matrix. Do not mix old and new CSVs.

```bash
# once per boot; needs root
sudo cpupower frequency-set -g performance

# default single-thread benchmark, pinned to logical CPU 0
PIN_CPUS=0 scripts/run_benchmarks.sh gcc
PIN_CPUS=0 scripts/run_benchmarks.sh clang
```

`PIN_CPUS` uses `taskset -c`, so `0-3` works for a multi-thread run. The script
writes `bench_environment.txt` next to the CSVs with timestamp, CPU, governor and
pinning. It warns if the governor is not `performance`; it does not change system
settings itself.

On Windows, run the batch inside an affinity mask and record the power plan
before starting:

```bat
powercfg /setactive SCHEME_MIN
powercfg /getactivescheme > output\windows\benchmarks\bench_environment.txt
start /affinity FF /wait cmd /c scripts\run_benchmarks.bat msvc
```

`FF` pins the process tree to eight logical CPUs; adjust for the host. The batch
itself still defaults to `THREADS=1`.

After the CSVs exist, run `scripts/summarize_bench.py` for the statistics tables
and `scripts/plot_compare.py` (or `PLOT=1`) for the seven combined comparison
graphs in `benchmarks/graph/`. `scripts/plot_bench.py` still produces one
throughput and one latency plot per CSV if needed.
