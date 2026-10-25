# Lab 1 — Windows verification TODO

> **Status (25/10/2026):** steps 1–8 and step 10 are done on this host. KAT is
> 16/16, `ctest` is 2/2 (`aescore` + `cli`), the GUI self-test passes, the
> non-ASCII path round-trip works, and the controlled Windows benchmark rerun
> produced 48 fresh CSVs with recorded AC/load/power/affinity/priority metadata.
> The run had 43/48 configurations stable in both directions; 4 encrypt and 3
> decrypt configurations remain flagged noisy in `summary_robust.csv`.
>
> Step 9 is the only unfinished section, and all three remaining boxes are
> report-only. `REPORT.md` is still the 24/10 144-CSV / Clang version with the
> old 96/100 self-grade, so those boxes cannot honestly be ticked yet. Refresh
> the report and repackage after updating it; the current Windows KAT, `ctest`,
> GUI and negative-test logs already match the current tree.

Run these steps on Windows; the Linux host cannot verify MSVC. Start from the
lab directory:

```bat
cd /d E:\NT219\lab01
```

The archived Windows evidence was stale (`kat_msvc.txt` 9/9 and
`ctest_msvc.txt` 1/1). Both logs have been regenerated from the current
`build\windows-msvc` tree; do not reuse the old ones.

Prerequisites: Visual Studio 2022/2026 with the x64 C++ toolchain, CMake,
Crypto++ (set `CRYPTOPP_ROOT`, normally `D:\cryptolibrary`) and Python 3 with
Tkinter for the GUI.

## 1. Build with MSVC

- [x] Release build succeeds. (verified)

```bat
scripts\build_windows.bat msvc
```

Expected: `build\windows-msvc\Release\aestool.exe` exists.

## 2. KAT

- [x] All 16 vectors pass. (verified)

```bat
build\windows-msvc\Release\aestool.exe --kat kat\vectors.json > output\windows\test_results\kat_msvc.txt
type output\windows\test_results\kat_msvc.txt
```

Expected last line: `16 passed, 0 failed`.

## 3. ctest

- [x] Both tests pass. (verified)

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

- [x] Shared library builds and the self-test passes. (verified)

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

- [x] Round-trip through a Vietnamese directory succeeds. (verified)

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

## 7. Controlled benchmark rerun

- [x] Windows CSVs regenerated on a controlled host.
- [x] `bench_environment.txt` kept with AC, load, power, affinity and priority.
- [x] Pin recorded (`start /affinity 4` = CPU 2) in `bench_environment.txt`.

Use the controlled runner from `lab01`. It selects the correct High performance
GUID, records the environment, runs a pilot, pins the process to logical CPU 2,
and writes the summaries:

```bat
cd /d E:\NT219\lab01
scripts\bench_windows_controlled.bat
python scripts\plot_crossplatform.py
```

For the final reported run, the environment was AC, High performance, CPU 2,
high priority, 1000 runs, 5 s per-configuration warm-up, 5 s cooldown, 10 s
settle and 30 s pilot warm-up. The pilot passed and 43/48 configurations were
stable in both directions.

Do **not** use `powercfg /setactive SCHEME_MIN`: on Windows that alias selects
Power saver. The correct High performance GUID is
`8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c`. Do not use `start /affinity FF` for the
reported run: it pins all eight logical CPUs; the report uses `AFFINITY=4`
(logical CPU 2).

This overwrites the 48 Windows CSVs and summaries. The runner deletes the old
`bench_windows_<compiler>_*.csv` and summary files before starting, so a rerun
cannot mix old and new rows; set `KEEP=1` only for deliberate subset runs. Do
not mix old and new CSVs in one summary.

## 8. MinGW GCC/Clang

- [x] MinGW builds pass ctest 2/2. (verified with GCC 16.2.0 and Clang 22.1.8)

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

Result on this host: Crypto++ built from source with MSYS2 GCC 16.2.0, then
`build\windows-gcc` and `build\windows-clang` both built and passed `ctest` 2/2
and KAT 16/16. Logs: `output\windows\test_results\kat_mingw_gcc.txt`,
`ctest_mingw_gcc.txt`, `kat_mingw_clang.txt`, `ctest_mingw_clang.txt`.

## 9. Update evidence and report

- [x] Windows logs replaced (KAT 16/16, ctest 2/2, GUI, non-ASCII path).
- [ ] Report numbers refreshed after the benchmark rerun.
- [ ] Ubuntu GCC-only benchmark numbers replace the old GCC+Clang tables.
- [ ] `REPORT.md` self-grade updated after the report refresh (include the
  controlled run and the 5 remaining noisy configurations).
- [x] `plot_crossplatform.py` rerun after the Windows rerun.

1. Replaced `output\windows\test_results\kat_msvc.txt` (16/16).
2. Replaced `output\windows\test_results\ctest_msvc.txt` (2/2).
3. Replaced `output\windows\test_results\gui_selftest.txt` (UTF-8).
4. Added `output\windows\test_results\non_ascii_path.txt`.
5. If benchmarks were rerun, replace the CSVs/summaries/graphs and then run
   `python scripts\plot_crossplatform.py`.
6. Updated the Windows tables in `REPORT.md` (KAT 16/16, ctest 2/2) and removed
   the stale "Windows CLI not rerun" limitation.
7. Re-export the report to PDF/DOCX after the benchmark rerun.

## 10. Package

- [x] Final archive contains the current binaries and evidence.
  `output\package\lab01-windows.zip` has 136 entries and is generated/git-ignored.

- CLI: `build\windows-msvc\Release\aestool.exe` (Crypto++ is static; no extra
  DLL needed).
- GUI bonus: `build\windows-msvc-shared\Release\aescore.dll` plus the `gui\`
  scripts. The package keeps the DLL in `bin\` and the GUI discovers it there.
- Evidence: the refreshed files under `output\windows\` and `output\compare\`.
- The package `REPORT.md` is still the old copy until the report is refreshed;
  replacing it requires one final re-zip.
- Do not commit the `build\windows-msvc` or `build\windows-msvc-shared` trees.
