# Lab 2 output evidence

| Path | Contents | Status |
|---|---|---|
| `windows/test_results/kat_msvc.txt` | MSVC KAT | 9/9 pass, 2026-11-02 |
| `windows/test_results/ctest_msvc.txt` | MSVC CTest | 4/4 pass |
| `windows/test_results/kat_mingw_gcc.txt` | MinGW GCC KAT | 9/9 pass |
| `windows/test_results/ctest_mingw_gcc.txt` | MinGW GCC CTest | 4/4 pass |
| `windows/test_results/kat_mingw_clang.txt` | MinGW Clang KAT | 9/9 pass |
| `windows/test_results/ctest_mingw_clang.txt` | MinGW Clang CTest | 4/4 pass |
| `windows/test_results/unit_*.txt` | Catch2 unit tests | 351 assertions / 11 cases |
| `windows/test_results/crossval_*.txt` | Crypto++ parity tests | 22 assertions / 3 cases |
| `windows/test_results/environment.txt` | Windows host and compiler versions | current |
| `windows/benchmarks/` | Windows benchmark CSVs, summary, plots, audit | 1000/100/30 rows, 2026-11-05; see `AUDIT.md` |
| `ubuntu/benchmarks/*.csv` | Ubuntu GCC benchmark rows | old run; see README |
| `ubuntu/benchmarks/*.log` | system-tracking logs | old run |
| `ubuntu/benchmarks/summary*.csv` | statistics | old run |
| `ubuntu/benchmarks/graphs/` | 1 MiB and 100 MiB plots | 1 GiB plot missing |
| `ubuntu/test_results/` | expected Ubuntu logs | not created in this pass |

## Rerun commands

Ubuntu tests and KAT:

```bash
cd lab02
scripts/build_linux.sh gcc
./build/ubuntu-gcc/aestool --kat kat/vectors.json \
  | tee output/ubuntu/test_results/kat_gcc.txt
(cd build/ubuntu-gcc && ctest --output-on-failure) \
  | tee output/ubuntu/test_results/ctest_gcc.txt
```

Ubuntu benchmark (do not mix with the old CSVs without noting the protocol):

```bash
THREADS=4 PIN_CPU=0-3 scripts/run_benchmarks.sh gcc
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
for f in output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_*.csv; do
  python3 scripts/plot_bench.py "$f" \
    --out-dir output/ubuntu/benchmarks/graphs --platform gcc
done
```

Windows tests and benchmark (already run; commands for a future rerun):

```bat
cd lab02
scripts\build_windows.bat msvc
cd build\windows-msvc
ctest -C Release --output-on-failure
cd ..\..
scripts\run_benchmarks.bat msvc
python scripts\summarize_bench.py --dir output\windows\benchmarks
```

The committed Ubuntu benchmark data is observational: it used the old
5-iteration warm-up, `powersave`, one thread, and no CPU pinning. Do not
present it as final protocol data until the rerun above is done. The Windows
data is current and uses the chosen 1000/100/30 counts, but it has no
run-time environment log.
