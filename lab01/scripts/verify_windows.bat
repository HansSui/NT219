@echo off
rem Refresh the Windows evidence for lab01 (TODO steps 1-6 and 8).
rem Usage:  scripts\verify_windows.bat
rem Optional:  set SKIP_MINGW=1 before calling to skip the MinGW half of step 8.
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0.." || exit /b 1

set "TOOL=%CD%\build\windows-msvc\Release\aestool.exe"
set "RES=%CD%\output\windows\test_results"
set "KEY=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
rem Keep redirected Python logs UTF-8 instead of the ANSI code page.
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

if not exist "%RES%" mkdir "%RES%"

echo ===== [1] MSVC build =====
call scripts\build_windows.bat msvc
if errorlevel 1 goto :fail
if not exist "%TOOL%" (
    echo missing executable: %TOOL%
    goto :fail
)

echo ===== [2] KAT =====
"%TOOL%" --kat kat\vectors.json > "%RES%\kat_msvc.txt" 2>&1
if errorlevel 1 goto :fail
type "%RES%\kat_msvc.txt"
findstr /c:"16 passed, 0 failed" "%RES%\kat_msvc.txt" >nul || goto :fail

echo ===== [3] ctest =====
pushd build\windows-msvc
ctest -C Release --output-on-failure > "%RES%\ctest_msvc.txt" 2>&1
set "RC=!errorlevel!"
popd
type "%RES%\ctest_msvc.txt"
if not "!RC!"=="0" goto :fail

echo ===== [4] GUI build and self-test =====
call gui\build_gui.bat
if errorlevel 1 goto :fail
python gui\aestool_gui.py --self-test > "%RES%\gui_selftest.txt" 2>&1
if errorlevel 1 goto :fail
type "%RES%\gui_selftest.txt"
findstr /c:"self-test OK" "%RES%\gui_selftest.txt" >nul || goto :fail

echo ===== [5] non-ASCII path round-trip =====
rem Build "thu muc" with a Vietnamese u-hook and dot-under-u without putting
rem non-ASCII bytes in this batch file (cmd decodes .bat files with the ANSI CP).
powershell -NoProfile -Command "$d = Join-Path 'output\windows\manual_checks' ([string][char]0x0074 + [char]0x0068 + [char]0x01B0 + ' ' + [char]0x006D + [char]0x1EE5 + [char]0x0063); New-Item -ItemType Directory -Force -Path $d | Out-Null"
set "VDIR="
for /d %%d in ("output\windows\manual_checks\t*") do set "VDIR=%%~fd"
if not defined VDIR (
    echo cannot locate the non-ASCII test directory
    goto :fail
)
"%TOOL%" encrypt --mode cbc --key-hex %KEY% --text "path test" --out "%VDIR%\ct.bin" >nul 2>&1
if errorlevel 1 goto :fail
"%TOOL%" decrypt --mode cbc --key-hex %KEY% --in "%VDIR%\ct.bin" --out "%VDIR%\out.txt" >nul 2>&1
if errorlevel 1 goto :fail
set "PLAIN="
for /f "usebackq delims=" %%p in ("%VDIR%\out.txt") do set "PLAIN=%%p"
if not "!PLAIN!"=="path test" (
    echo non-ASCII round-trip mismatch: "!PLAIN!"
    goto :fail
)
powershell -NoProfile -Command "$t = 'encrypt/decrypt via ' + $env:VDIR + [Environment]::NewLine + 'plaintext=path test' + [Environment]::NewLine + 'exit=0'; [IO.File]::WriteAllText('%RES%\non_ascii_path.txt', $t, (New-Object Text.UTF8Encoding($false)))"
echo non-ASCII round-trip OK

echo ===== [6] negative re-check =====
set "NEGTMP=%CD%\output\windows\manual_checks\neg_recheck"
set "NEGLOG=%CD%\output\windows\neg_results\neg_recheck.txt"
if not exist "output\windows\neg_results" mkdir "output\windows\neg_results"
if exist "%NEGTMP%" rmdir /s /q "%NEGTMP%"
mkdir "%NEGTMP%"
pushd "%NEGTMP%"
(
    echo ^$ aestool encrypt --mode gcm --aead ... --out gcm.bin
    "%TOOL%" encrypt --mode gcm --aead --key-hex %KEY% --nonce 00112233445566778899aabb --text data --out gcm.bin
    echo encrypt_exit=!errorlevel!
    echo ^$ aestool decrypt --mode gcm --aead --key-hex ^<wrong key^> --in gcm.bin --out bad.out
    "%TOOL%" decrypt --mode gcm --aead --key-hex 101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f --in gcm.bin --out bad.out
    echo decrypt_exit=!errorlevel!
    if exist bad.out (echo bad.out=present) else (echo bad.out=absent)

    echo ^$ aestool encrypt --mode ecb --in large.bin ^(16385 bytes^)
    powershell -NoProfile -Command "Set-Content -Path large.bin -Value ('A' * 16385) -NoNewline -Encoding ascii"
    "%TOOL%" encrypt --mode ecb --key-hex %KEY% --in large.bin --out ecb.bin
    echo ecb_exit=!errorlevel!
    if exist ecb.bin (echo ecb.bin=present) else (echo ecb.bin=absent)

    echo ^$ aestool encrypt --mode ctr --iv ... --text first
    "%TOOL%" encrypt --mode ctr --key-hex %KEY% --iv 00112233445566778899aabbccddeeff --text first --out first.bin
    echo ctr_first_exit=!errorlevel!
    echo ^$ aestool encrypt --mode ctr --iv ... --text second
    "%TOOL%" encrypt --mode ctr --key-hex %KEY% --iv 00112233445566778899aabbccddeeff --text second --out second.bin
    echo ctr_second_exit=!errorlevel!
    if exist second.bin (echo second.bin=present) else (echo second.bin=absent)
) > "%NEGLOG%" 2>&1
popd
rmdir /s /q "%NEGTMP%"
type "%NEGLOG%"
findstr /c:"decrypt_exit=1" "%NEGLOG%" >nul || goto :fail
findstr /c:"bad.out=absent" "%NEGLOG%" >nul || goto :fail
findstr /c:"ecb_exit=1" "%NEGLOG%" >nul || goto :fail
findstr /c:"ecb.bin=absent" "%NEGLOG%" >nul || goto :fail
findstr /c:"ctr_first_exit=0" "%NEGLOG%" >nul || goto :fail
findstr /c:"ctr_second_exit=1" "%NEGLOG%" >nul || goto :fail
findstr /c:"second.bin=absent" "%NEGLOG%" >nul || goto :fail

if defined SKIP_MINGW (
    echo ===== [8] MinGW skipped ^(SKIP_MINGW=1^) =====
    goto :done
)

echo ===== [8] MinGW GCC and Clang =====
if not defined MINGW_BIN if exist "D:\mys64\mingw64\bin\g++.exe" set "MINGW_BIN=D:\mys64\mingw64\bin"
if not defined MINGW_BIN if exist "C:\msys64\mingw64\bin\g++.exe" set "MINGW_BIN=C:\msys64\mingw64\bin"
if not defined MINGW_BIN (
    echo MinGW g++ not found. Set MINGW_BIN to the folder with g++.exe, or use SKIP_MINGW=1.
    goto :fail
)
set "PATH=%MINGW_BIN%;%PATH%"
if not defined CRYPTOPP_ROOT set "CRYPTOPP_ROOT=C:\cryptopp-mingw"

call scripts\build_windows.bat gcc
if errorlevel 1 goto :fail
build\windows-gcc\aestool.exe --kat kat\vectors.json > "%RES%\kat_mingw_gcc.txt" 2>&1
if errorlevel 1 goto :fail
pushd build\windows-gcc
ctest --output-on-failure > "%RES%\ctest_mingw_gcc.txt" 2>&1
set "RC=!errorlevel!"
popd
if not "!RC!"=="0" goto :fail
type "%RES%\kat_mingw_gcc.txt"
type "%RES%\ctest_mingw_gcc.txt"

call scripts\build_windows.bat clang
if errorlevel 1 goto :fail
build\windows-clang\aestool.exe --kat kat\vectors.json > "%RES%\kat_mingw_clang.txt" 2>&1
if errorlevel 1 goto :fail
pushd build\windows-clang
ctest --output-on-failure > "%RES%\ctest_mingw_clang.txt" 2>&1
set "RC=!errorlevel!"
popd
if not "!RC!"=="0" goto :fail
type "%RES%\kat_mingw_clang.txt"
type "%RES%\ctest_mingw_clang.txt"

:done
echo.
echo All checks passed. Evidence refreshed under output\windows\.
echo Next: run the controlled benchmark block, then update REPORT.md.
exit /b 0

:fail
echo.
echo FAILED. Inspect the last log under %RES% and rerun.
exit /b 1
