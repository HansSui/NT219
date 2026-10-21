# Lab 1 — Windows verification TODO

These checks cannot be completed from the Linux host. Run them on the Windows
machine, then copy the updated logs back into `lab01/output/windows/`.

Current archived Windows evidence is older than the current code:

- `kat_msvc.txt` is **9/9**; the suite now has **16** vectors.
- `ctest_msvc.txt` is **1/1**; `ctest` now also registers the portable CLI test.
- The archived binaries predate `--threads`, `u8path` path handling and the
  CLI test. Rebuild before submitting.

## 0. Prerequisites

- Visual Studio 2026 or 2022 with the x64 C++ toolchain.
- CMake (the Visual Studio bundled CMake is fine).
- Crypto++ static library, normally `D:\cryptolibrary`, or set:
  ```bat
  set CRYPTOPP_ROOT=D:\cryptolibrary
  ```
- Python 3 with Tkinter for the GUI self-test.

All commands below start from the lab directory:

```bat
cd /d E:\NT219\lab01
```

Adjust the drive/path to your machine.

## 1. Build with MSVC

Open **x64 Native Tools Command Prompt for VS 2026/2022**, then:

```bat
scripts\build_windows.bat msvc
```

Expected: Release build succeeds. `build_windows_msvc\Release\aestool.exe`
exists.

## 2. Re-run KAT and save the log

```bat
build_windows_msvc\Release\aestool.exe --kat kat\vectors.json > output\windows\test_results\kat_msvc.txt
type output\windows\test_results\kat_msvc.txt
```

Expected last line:

```text
16 passed, 0 failed
```

## 3. Re-run ctest and save the log

```bat
cd build_windows_msvc
ctest -C Release --output-on-failure > ..\output\windows\test_results\ctest_msvc.txt 2>&1
type ..\output\windows\test_results\ctest_msvc.txt
cd ..
```

Expected: **2/2 passed** — `aescore` and `cli`. The `cli` test runs
`tests\cli_test.cmake` and covers round-trip through a non-ASCII path,
nonce reuse, ECB size limit, short IV, odd hex and `--threads`.

If `cli` fails, run it directly to see the actual error:

```bat
cmake -DTOOL=build_windows_msvc\Release\aestool.exe -DWORK_DIR=build_windows_msvc\cli_test -P tests\cli_test.cmake
```

Expected:

```text
-- cli_test.cmake: all cases passed
```

## 4. Re-run the GUI self-test

```bat
gui\build_gui.bat
python gui\aestool_gui.py --self-test > output\windows\test_results\gui_selftest.txt
type output\windows\test_results\gui_selftest.txt
```

Expected: `self-test OK`, eight modes, round-trip and wrong-key rejection passed.

If the log becomes UTF-16 (a PowerShell `>` redirect can do that), rewrite it as
UTF-8:

```powershell
python gui\aestool_gui.py --self-test | Out-File -Encoding utf8 output\windows\test_results\gui_selftest.txt
```

## 5. Check UTF-8 paths (the new `u8path` code)

The `cli` ctest already covers this, but a quick manual check is useful:

```bat
set "KEY=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
mkdir "thư mục"
build_windows_msvc\Release\aestool.exe encrypt --mode cbc --key-hex %KEY% --text "path test" --out "thư mục\ct.bin"
build_windows_msvc\Release\aestool.exe decrypt --mode cbc --key-hex %KEY% --in "thư mục\ct.bin" --out "thư mục\out.txt"
type "thư mục\out.txt"
```

Expected output: `path test`.

## 6. Negative behaviour (optional re-check)

The archived 17 cases are in `output\windows\neg_results\`. Crypto behaviour did
not change, so a full rerun is optional. If you want fresh logs, re-check the
important fail-closed cases:

```bat
rem AEAD wrong key -> exit 1, no output
build_windows_msvc\Release\aestool.exe encrypt --mode gcm --aead --key-hex %KEY% --nonce 00112233445566778899aabb --text data --out gcm.bin
build_windows_msvc\Release\aestool.exe decrypt --mode gcm --aead --key-hex 101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f --in gcm.bin --out bad.out

rem ECB >16 KiB -> warning + exit 1
powershell -NoProfile -Command "Set-Content -Path large.bin -Value ('A' * 16385) -NoNewline -Encoding ascii"
build_windows_msvc\Release\aestool.exe encrypt --mode ecb --key-hex %KEY% --in large.bin --out ecb.bin

rem nonce reuse -> exit 1 on the second CTR use
build_windows_msvc\Release\aestool.exe encrypt --mode ctr --key-hex %KEY% --iv 00112233445566778899aabbccddeeff --text first --out first.bin
build_windows_msvc\Release\aestool.exe encrypt --mode ctr --key-hex %KEY% --iv 00112233445566778899aabbccddeeff --text second --out second.bin
```

Save the command output and exit codes if you replace the existing logs. Delete
`.aestool_nonces` before repeating the nonce-reuse pair, or the first command will
already be rejected as a reuse.

## 7. Benchmarks (only if you need numbers from the current binary)

Default `--threads` is 1, so the benchmark methodology is unchanged. Re-running
is optional unless the report should use timings from the rebuilt binary. For a
controlled run, set a fixed power plan and pin the process tree:

```bat
powercfg /setactive SCHEME_MIN
powercfg /getactivescheme > output\windows\benchmarks\bench_environment.txt
start /affinity FF /wait cmd /c scripts\run_benchmarks.bat msvc
python scripts\summarize_bench.py --dir output\windows\benchmarks
python scripts\plot_compare.py --dir output\windows\benchmarks --out-dir output\windows\benchmarks\graph
```

`SCHEME_MIN` is the High performance alias; use the GUID from
`powercfg /list` if your machine uses another scheme. `FF` pins eight logical
CPUs; adjust the affinity mask for the host. Then regenerate the cross-platform
tables/plots from the repository root:

```bat
python scripts\plot_crossplatform.py
```

This overwrites the 48 Windows CSVs and summaries. Do not mix old and new CSVs in
one summary. If only the old CSVs are used, keep the existing "uncontrolled
host" limitation in the report.

## 8. Optional: MinGW GCC/Clang (closes the last cross-platform gap)

Use an **MSYS2 MinGW 64-bit** shell. Build Crypto++ for MinGW first:

```bash
pacman -S --needed git make mingw-w64-x86_64-gcc mingw-w64-x86_64-clang mingw-w64-x86_64-cmake
git clone --depth 1 https://github.com/weidai11/cryptopp.git
cd cryptopp
make -f GNUmakefile -j"$(nproc)"
make -f GNUmakefile install PREFIX=/c/cryptopp-mingw
```

Then from `cmd.exe` with `C:\msys64\mingw64\bin` on `PATH`:

```bat
cd /d E:\NT219\lab01
set CRYPTOPP_ROOT=C:\cryptopp-mingw
scripts\build_windows.bat gcc
scripts\build_windows.bat clang
```

Expected: both builds pass `ctest` 2/2. Save the logs if you want to claim MinGW
in the report. If this is too much work, leave the report note "MinGW not
verified" as is.

## 9. Update evidence and deliverables

After the MSVC run:

1. Replace `output\windows\test_results\kat_msvc.txt` (expect 16/16).
2. Replace `output\windows\test_results\ctest_msvc.txt` (expect 2/2).
3. Replace `output\windows\test_results\gui_selftest.txt` (UTF-8).
4. If you reran negatives/benchmarks, replace those logs/CSVs and regenerate
   summaries/plots.
5. Copy `build_windows_msvc\Release\aestool.exe` into the final submission
   archive. Crypto++ is static; no extra DLL is needed for the CLI.
6. For the GUI bonus, include `build_gui\Release\aescore.dll` and the `gui\`
   scripts if the archive is meant to run the GUI.
7. Update the stale Windows numbers in `REPORT.md`:
   - KAT: 9/9 → 16/16.
   - `ctest`: 1/1 → 2/2.
   - Self-grade cross-platform note: remove "test CLI Windows chưa chạy lại"
     after it passes.
8. Re-export the report to PDF/DOCX after those edits.

Do not commit `build_windows_msvc/`; only the logs/CSVs under `output/` are
evidence.
