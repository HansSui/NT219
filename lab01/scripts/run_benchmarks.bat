@echo off
setlocal EnableDelayedExpansion
set "root=%~dp0.."
set "compiler=%~1"
if "%compiler%"=="" set "compiler=gcc"

if /i "%compiler%"=="msvc" (
    set "tool=%root%\build_windows_msvc\Release\aestool.exe"
) else (
    set "tool=%root%\build_windows_%compiler%\aestool.exe"
)
if not exist "%tool%" (
    echo missing executable: %tool%
    exit /b 1
)

set "runs=%RUNS%"
if "%runs%"=="" set "runs=1000"
set "warmup=%WARMUP%"
if "%warmup%"=="" set "warmup=1"
set "out=%OUT%"
if "%out%"=="" set "out=%root%\output\windows\benchmarks"
set "key=%KEY%"
if "%key%"=="" set "key=%root%\output\windows\key_bin\key.bin"

if not exist "%out%" mkdir "%out%"
if not exist "%key%" (
    if not exist "%root%\output\windows\key_bin" mkdir "%root%\output\windows\key_bin"
    set "KEYPATH=%key%"
    powershell -NoProfile -Command "$b=New-Object byte[] 32; [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($b); [IO.File]::WriteAllBytes($env:KEYPATH,$b)"
    if errorlevel 1 exit /b 1
)

for %%M in (ecb cbc ofb cfb ctr xts ccm gcm) do (
    for %%S in (1024 4096 16384 262144 1048576 8388608) do (
        set "label=%%S"
        if "%%S"=="1024" set "label=1KB"
        if "%%S"=="4096" set "label=4KB"
        if "%%S"=="16384" set "label=16KB"
        if "%%S"=="262144" set "label=256KB"
        if "%%S"=="1048576" set "label=1MB"
        if "%%S"=="8388608" set "label=8MB"

        set "extra="
        if /i "%%M"=="ecb" set "extra=--allow-ecb"
        echo benchmarking %%M !label! (%runs% runs)
        "%tool%" bench --mode %%M --key "%key%" --size %%S --runs %runs% --warmup %warmup% --out "%out%\bench_windows_%compiler%_%%M_!label!.csv" !extra! >nul 2>>"%out%\bench.log"
        if errorlevel 1 (
            echo failed: %out%\bench_windows_%compiler%_%%M_!label!.csv ^(see %out%\bench.log^)
            exit /b 1
        )
        echo   wrote bench_windows_%compiler%_%%M_!label!.csv
    )
)

if "%PLOT%"=="1" (
    python "%root%\scripts\plot_compare.py" --dir "%out%" --out-dir "%out%\graph"
)

where python >nul 2>nul
if not errorlevel 1 (
    python "%root%\scripts\summarize_bench.py" --dir "%out%"
)

echo done: %out%
endlocal
