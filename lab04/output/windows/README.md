# Windows evidence

No Windows run is committed. On the Windows host run from `lab04/`:

```bat
scripts\build_windows.bat msvc
rem scripts\build_windows.bat gcc
set "OPENSSL_ROOT_DIR=D:\cryptolibrary"
scripts\run_benchmarks.bat msvc
```

The build script writes `test_results/ctest_<compiler>.txt` and
`test_results/kat_<compiler>.txt`; the benchmark script writes the raw CSV,
summary CSV and `graphs/` here. Copy the run-time OpenSSL/CPU details into a
short `environment.txt` alongside them.
