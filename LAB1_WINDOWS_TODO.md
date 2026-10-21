# Lab 1 — Windows verification TODO

Run these steps on Windows; the Linux host cannot verify MSVC. Start from the
lab directory:

```bat
cd /d E:\NT219\lab01
```

The archived Windows evidence is stale: `kat_msvc.txt` is 9/9 and
`ctest_msvc.txt` is 1/1. Rebuild and replace both logs; do not reuse the old
ones.

Prerequisites: Visual Studio 2022/2026 with the x64 C++ toolchain, CMake,
Crypto++ (set `CRYPTOPP_ROOT`, normally `D:\cryptolibrary`) and Python 3 with
Tkinter for the GUI.

## 1. Build with MSVC

- [ ] Release build succeeds.

```bat
scripts\build_windows.bat msvc
```

Expected: `build\windows-msvc\Release\aestool.exe` exists.

## 2. KAT

- [ ] All 16 vectors pass.

```bat
build\windows-msvc\Release\aestool.exe --kat kat\vectors.json > output\windows\test_results\kat_msvc.txt
type output\windows\test_results\kat_msvc.txt
```

Expected last line: `16 passed, 0 failed`.

## 3. ctest

- [ ] Both tests pass.

```bat
cd build\windows-msvc
ctest -C Release --output-on-failure > ..\..\output\windows\test_results\ctest_msvc.txt 2>&1
type ..\..\output\windows\test_results\ctest_msvc.txt
cd ..\..
```

Expected: `2/2` passed, covering `aescore` and `cli`. The CLI test covers a
non-ASCII path, nonce reuse, the ECB size limit, short IV, odd hex and
`--threads`.

If `cli` fails, run it directly:

```bat
cmake -DTOOL=build\windows-msvc\Release\aestool.exe -DWORK_DIR=build\windows-msvc\cli_test -P tests\cli_test.cmake
```

Expected: `-- cli_test.cmake: all cases passed`.

## 4. GUI self-test

- [ ] Shared library builds and the self-test passes.

```bat
gui\build_gui.bat
python gui\aestool_gui.py --self-test > output\windows\test_results\gui_selftest.txt
type output\windows\test_results\gui_selftest.txt
```

Expected: `self-test OK`, all eight modes, round-trip and wrong-key rejection
passed.

If the log is UTF-16, rewrite it as UTF-8:

```powershell
python gui\aestool_gui.py --self-test | Out-File -Encoding utf8 output\windows\test_results\gui_selftest.txt
```

## 5. Non-ASCII path check

- [ ] Round-trip through a Vietnamese directory succeeds.

```bat
set "KEY=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
mkdir "thư mục"
build\windows-msvc\Release\aestool.exe encrypt --mode cbc --key-hex %KEY% --text "path test" --out "thư mục\ct.bin"
build\windows-msvc\Release\aestool.exe decrypt --mode cbc --key-hex %KEY% --in "thư mục\ct.bin" --out "thư mục\out.txt"
type "thư mục\out.txt"
```

Expected output: `path test`.

## 6. Optional negative re-check

The archived 17 cases are in `output\windows\neg_results\`; a full rerun is
optional. To refresh the important fail-closed cases:

```bat
set "KEY=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"

rem AEAD wrong key -> exit 1, no output
build\windows-msvc\Release\aestool.exe encrypt --mode gcm --aead --key-hex %KEY% --nonce 00112233445566778899aabb --text data --out gcm.bin
build\windows-msvc\Release\aestool.exe decrypt --mode gcm --aead --key-hex 101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f --in gcm.bin --out bad.out

rem ECB >16 KiB -> warning + exit 1
powershell -NoProfile -Command "Set-Content -Path large.bin -Value ('A' * 16385) -NoNewline -Encoding ascii"
build\windows-msvc\Release\aestool.exe encrypt --mode ecb --key-hex %KEY% --in large.bin --out ecb.bin

rem nonce reuse -> exit 1 on the second CTR use
build\windows-msvc\Release\aestool.exe encrypt --mode ctr --key-hex %KEY% --iv 00112233445566778899aabbccddeeff --text first --out first.bin
build\windows-msvc\Release\aestool.exe encrypt --mode ctr --key-hex %KEY% --iv 00112233445566778899aabbccddeeff --text second --out second.bin
```

Delete `.aestool_nonces` before repeating the nonce-reuse pair, otherwise the
first command is already rejected as reuse.

## 7. Optional controlled benchmark rerun

- [ ] Windows CSVs regenerated on a controlled host.

Default `--threads` is 1, so the methodology is unchanged. Use a fixed power
plan and CPU affinity:

```bat
powercfg /setactive SCHEME_MIN
powercfg /getactivescheme > output\windows\benchmarks\bench_environment.txt
start /affinity FF /wait cmd /c scripts\run_benchmarks.bat msvc
python scripts\summarize_bench.py --dir output\windows\benchmarks
python scripts\plot_compare.py --dir output\windows\benchmarks --out-dir output\windows\benchmarks\graph
```

`SCHEME_MIN` is the High performance alias; use the GUID from `powercfg /list`
if needed. `FF` pins eight logical CPUs; adjust the mask for the host.

This overwrites the 48 Windows CSVs and summaries. Do not mix old and new CSVs
in one summary. Regenerate the cross-platform figures only after the Windows
rerun, from `lab01`:

```bat
python scripts\plot_crossplatform.py
```

## 8. Optional MinGW GCC/Clang

- [ ] MinGW builds pass ctest 2/2.

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

If skipped, keep the report note "MinGW not verified".

## 9. Update evidence and report

- [ ] Windows logs replaced.
- [ ] Report numbers refreshed.

1. Replace `output\windows\test_results\kat_msvc.txt` (16/16).
2. Replace `output\windows\test_results\ctest_msvc.txt` (2/2).
3. Replace `output\windows\test_results\gui_selftest.txt` (UTF-8).
4. If benchmarks were rerun, replace the CSVs/summaries/graphs and then run
   `python scripts\plot_crossplatform.py`.
5. Update the Windows tables in `REPORT.md` (KAT 16/16, ctest 2/2) and remove
   the stale "Windows CLI not rerun" limitation. Re-check the Performance score
   if the new CSV numbers change.
6. Re-export the report to PDF/DOCX.

## 10. Package

- [ ] Final archive contains the current binaries and evidence.

- CLI: `build\windows-msvc\Release\aestool.exe` (Crypto++ is static; no extra
  DLL needed).
- GUI bonus: `build\windows-msvc-shared\Release\aescore.dll` plus the `gui\`
  scripts.
- Evidence: the refreshed files under `output\windows\`.
- Do not commit the `build\windows-msvc` or `build\windows-msvc-shared` trees.
