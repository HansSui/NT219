# Lab 2 output evidence

| Path | Contents | Status |
|---|---|---|
| `windows/test_results/kat_msvc.txt` | MSVC KAT | 9/9 pass, 2026-11-02 |
| `windows/test_results/ctest_msvc.txt` | MSVC CTest | 4/4 pass, pre-OpenSSL; rerun |
| `windows/test_results/kat_mingw_gcc.txt` | MinGW GCC KAT | 9/9 pass |
| `windows/test_results/ctest_mingw_gcc.txt` | MinGW GCC CTest | 4/4 pass, pre-OpenSSL; rerun |
| `windows/test_results/kat_mingw_clang.txt` | MinGW Clang KAT | 9/9 pass |
| `windows/test_results/ctest_mingw_clang.txt` | MinGW Clang CTest | 4/4 pass, pre-OpenSSL; rerun |
| `windows/test_results/unit_*.txt` | Catch2 unit tests | 351 assertions / 11 cases |
| `windows/test_results/crossval_*.txt` | prior Crypto++ parity tests | stale; rerun with OpenSSL |
| `windows/test_results/environment.txt` | prior Windows host/oracle versions | stale Crypto++ note; update after rerun |
| `windows/benchmarks/` | Windows benchmark CSVs, summary, plots, audit | 1000/100/30 rows, 2026-11-05; combined 3-series summary plot added; see `AUDIT.md` |
| `ubuntu/benchmarks/*.csv` | Ubuntu GCC benchmark rows | 1000/100/30, 2026-11-05 |
| `ubuntu/benchmarks/*.log` | system-tracking logs | old run; current script is console-only |
| `ubuntu/benchmarks/summary*.csv` | statistics | 2026-11-05 |
| `ubuntu/benchmarks/graphs/` | one combined throughput + latency summary plot (encrypt, decrypt, total) | all three sizes, 2026-11-05 |
| `ubuntu/benchmarks/openssl/` | OpenSSL throughput oracle CSVs + summary | 4 threads, 2026-11-05 |
| `ubuntu/test_results/` | Ubuntu GCC KAT/CTest/unit/crossval/environment logs | 9/9 KAT, 4/4 CTest, OpenSSL oracle, 2026-11-05 |

The Windows KAT and unit logs are oracle-independent and remain valid. The
Windows `ctest_*` and `crossval_*` logs include the pre-OpenSSL
cross-validation target, so rerun them before citing that evidence.

## Rerun commands

Ubuntu tests and KAT (already run 2026-11-05; commands for a future rerun):

```bash
cd lab02
scripts/build_linux.sh gcc
./build/ubuntu-gcc/aestool --kat kat/vectors.json \
  | tee output/ubuntu/test_results/kat_gcc.txt
(cd build/ubuntu-gcc && ctest --output-on-failure) \
  | tee output/ubuntu/test_results/ctest_gcc.txt
```

Ubuntu benchmark (already run 2026-11-05; commands for a future rerun):

```bash
THREADS=4 PIN_CPU=0-3 scripts/run_benchmarks.sh gcc
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
python3 scripts/plot_bench.py output/ubuntu/benchmarks/bench_ubuntu_gcc_ctr_*.csv \
  --out-dir output/ubuntu/benchmarks/graphs --platform ubuntu-gcc
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

OpenSSL throughput comparison (Ubuntu data exists; Windows not run):

```bash
mkdir -p output/ubuntu/benchmarks/openssl
for pair in "1048576 1000" "104857600 100" "1073741824 30"; do
  set -- $pair
  ./build/ubuntu-gcc/bench_openssl --size "$1" --runs "$2" --threads 4 \
    --out "output/ubuntu/benchmarks/openssl/bench_openssl_gcc_ctr_$1.csv"
done
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks/openssl
```

```bat
cd lab02
scripts\build_windows.bat msvc
mkdir output\windows\benchmarks\openssl 2>nul
build\windows-msvc\Release\bench_openssl.exe --size 1048576 --runs 1000 --threads 4 --out output\windows\benchmarks\openssl\bench_openssl_msvc_ctr_1048576.csv
build\windows-msvc\Release\bench_openssl.exe --size 104857600 --runs 100 --threads 4 --out output\windows\benchmarks\openssl\bench_openssl_msvc_ctr_104857600.csv
build\windows-msvc\Release\bench_openssl.exe --size 1073741824 --runs 30 --threads 4 --out output\windows\benchmarks\openssl\bench_openssl_msvc_ctr_1073741824.csv
python scripts\summarize_bench.py --dir output\windows\benchmarks\openssl
```

The Ubuntu data now uses the chosen 1000/100/30 matrix with 4 threads and
CPUs 0-3, but it has no environment log (the script is console-only) and the
1 MiB CV is 14.4%. The Windows data uses the same counts and also has no
run-time environment log.
