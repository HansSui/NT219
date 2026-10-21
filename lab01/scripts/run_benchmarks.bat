@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Run the full benchmark matrix: every mode x every required size.
rem Usage: run_benchmarks.bat [msvc|gcc|clang]
rem Optional environment: RUNS WARMUP THREADS SIZES MODES PLATFORM OUT KEY PLOT

set "root=%~dp0.."
set "compiler=%~1"
if "%compiler%"=="" set "compiler=msvc"

if /i "%compiler%"=="msvc" (
    set "tool=%root%\build\windows-msvc\Release\aestool.exe"
) else (
    set "tool=%root%\build\windows-%compiler%\aestool.exe"
)
if not exist "%tool%" (
    echo missing executable: %tool%
    echo build it first: scripts\build_windows.bat %compiler%
    exit /b 1
)

set "platform=%PLATFORM%"
if "%platform%"=="" set "platform=windows"
set "runs=%RUNS%"
if "%runs%"=="" set "runs=1000"
set "warmup=%WARMUP%"
if "%warmup%"=="" set "warmup=1"
set "threads=%THREADS%"
if "%threads%"=="" set "threads=1"
set "thread_arg="
if not "%threads%"=="1" set "thread_arg=--threads %threads%"
set "out=%OUT%"
if "%out%"=="" set "out=%root%\output\%platform%\benchmarks"
set "key=%KEY%"
if "%key%"=="" set "key=%root%\output\%platform%\key_bin\key.bin"
set "modes=%MODES%"
if "%modes%"=="" set "modes=ecb cbc ofb cfb ctr xts ccm gcm"
set "sizes=%SIZES%"
if "%sizes%"=="" set "sizes=1024 4096 16384 262144 1048576 8388608"

if not exist "%out%" mkdir "%out%"
if exist "%out%\bench.log" del "%out%\bench.log" >nul 2>nul
if not exist "%out%" (
    echo cannot create output directory: %out% >&2
    exit /b 1
)
if not exist "%key%" (
    if not exist "%root%\output\%platform%\key_bin" mkdir "%root%\output\%platform%\key_bin"
    set "KEYPATH=%key%"
    powershell -NoProfile -Command "$b=New-Object byte[] 32; [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($b); [IO.File]::WriteAllBytes($env:KEYPATH,$b)"
    if errorlevel 1 (
        echo failed to generate key: %key% >&2
        exit /b 1
    )
    echo generated key: %key%
)

echo tool:    %tool%
echo runs:    %runs% per mode/size
echo warmup:  %warmup%s
echo threads: %threads%
echo modes:   %modes%
echo sizes:   %sizes%
echo out:     %out%

for %%M in (%modes%) do (
    for %%S in (%sizes%) do (
        set "label=%%S"
        if "%%S"=="1024" set "label=1KB"
        if "%%S"=="4096" set "label=4KB"
        if "%%S"=="16384" set "label=16KB"
        if "%%S"=="262144" set "label=256KB"
        if "%%S"=="1048576" set "label=1MB"
        if "%%S"=="8388608" set "label=8MB"
        if "!label!"=="%%S" set "label=%%SB"

        set "extra="
        if /i "%%M"=="ecb" set "extra=--allow-ecb"

        echo benchmarking %%M !label! - %runs% runs
        "%tool%" bench --mode %%M --key "%key%" --size %%S --runs %runs% --warmup %warmup% %thread_arg% --out "%out%\bench_%platform%_%compiler%_%%M_!label!.csv" !extra! >nul 2>>"%out%\bench.log"
        if errorlevel 1 (
            echo failed: %out%\bench_%platform%_%compiler%_%%M_!label!.csv ^(see %out%\bench.log^)
            exit /b 1
        )
        echo   wrote bench_%platform%_%compiler%_%%M_!label!.csv
    )
)

if "%PLOT%"=="1" (
    python "%root%\scripts\plot_compare.py" --dir "%out%" --out-dir "%out%\graph"
    if errorlevel 1 echo plot generation failed ^(is matplotlib installed?^)
)

where python >nul 2>nul
if not errorlevel 1 (
    python "%root%\scripts\summarize_bench.py" --dir "%out%"
)

echo done: %out%
endlocal
