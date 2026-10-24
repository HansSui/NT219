@echo off
rem Controlled Windows benchmark run for lab01.
rem
rem What this does:
rem   1. selects the High performance power plan;
rem   2. records the actual OS/CPU/power/affinity/priority settings;
rem   3. checks AC power, CPU load and stray aestool processes;
rem   4. runs a short CTR 1 MB pilot and aborts if it is noisy;
rem   5. runs the full matrix at high priority on one logical CPU;
rem   6. writes mean/median/trimmed-mean summaries and the plots.
rem
rem Usage:  scripts\bench_windows_controlled.bat
rem Optional overrides: RUNS WARMUP THREADS SIZES MODES OUT KEY AFFINITY
rem                     COOLDOWN FORCE SKIP_PILOT PILOT_ONLY SKIP_CROSS
rem
rem   Smoke test:
rem     set OUT=%TEMP%\benchtest & set MODES=ctr gcm ccm cbc & set SIZES=1024
rem     set RUNS=5 & set WARMUP=0 & set SKIP_PILOT=1 & set SKIP_CROSS=1
rem     set FORCE=1 & scripts\bench_windows_controlled.bat
rem
rem   Pilot only (does not touch the real CSVs):
rem     set PILOT_ONLY=1 & scripts\bench_windows_controlled.bat
rem
rem   Run anyway on a busy host or after a noisy pilot:
rem     set FORCE=1 & scripts\bench_windows_controlled.bat
setlocal EnableExtensions

cd /d "%~dp0.." || exit /b 1

rem High performance. powercfg's SCHEME_MIN alias is Power saver, not High performance.
set "SCHEME=8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"
rem start /affinity takes a hex mask: 4 = logical CPU 2.
if not defined AFFINITY set "AFFINITY=4"
set "AFFINITY_LABEL=0x%AFFINITY%"
if "%AFFINITY%"=="4" set "AFFINITY_LABEL=0x4 (CPU 2)"

if not defined OUT set "OUT=output\windows\benchmarks"
if not defined RUNS set "RUNS=1000"
if not defined WARMUP set "WARMUP=1"
if not defined THREADS set "THREADS=1"
if not defined COOLDOWN set "COOLDOWN=2"
if not defined PILOT_WARMUP set "PILOT_WARMUP=15"
if not defined SETTLE set "SETTLE=5"
if not defined MAX_CPU_LOAD set "MAX_CPU_LOAD=15"
if not defined TOOL set "TOOL=build\windows-msvc\Release\aestool.exe"
if not defined KEY set "KEY=output\windows\key_bin\key.bin"

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%TOOL%" (
    echo missing executable: %TOOL%
    echo build it first: scripts\build_windows.bat msvc
    exit /b 1
)

if not exist "%KEY%" (
    if not exist "output\windows\key_bin" mkdir "output\windows\key_bin"
    set "KEYPATH=%KEY%"
    powershell -NoProfile -Command "$b=New-Object byte[] 32; [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($b); [IO.File]::WriteAllBytes($env:KEYPATH,$b)"
    if errorlevel 1 (
        echo failed to generate key: %KEY%
        exit /b 1
    )
    echo generated key: %KEY%
)

echo ===== power plan =====
powercfg /setactive %SCHEME%
if errorlevel 1 echo warning: could not select the High performance plan; using the current plan
powercfg /getactivescheme > "%OUT%\power_scheme.txt"
type "%OUT%\power_scheme.txt"
echo.

echo ===== environment =====
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\bench_env.ps1 ^
    -OutFile "%OUT%\bench_environment.txt" ^
    -Affinity "%AFFINITY_LABEL%" ^
    -Runs %RUNS% -Warmup %WARMUP% -Cooldown %COOLDOWN% ^
    -PilotWarmup %PILOT_WARMUP% -Settle %SETTLE% ^
    -Threads %THREADS% -Priority high -Tool "%TOOL%"
if errorlevel 1 exit /b 1

echo ===== preflight =====
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\preflight_windows.ps1 -MaxCpuLoad %MAX_CPU_LOAD%
if errorlevel 1 (
    echo.
    echo preflight failed: host is not quiet or not on AC power.
    if not defined FORCE (
        echo Close background apps, plug in the charger, or set FORCE=1 to run anyway.
        exit /b 2
    )
    echo FORCE=1: continuing anyway.
)

echo ===== settle: %SETTLE%s =====
powershell -NoProfile -Command "Start-Sleep -Seconds %SETTLE%"

if not defined SKIP_PILOT (
    echo ===== pilot: CTR 1 MB, 300 runs, %PILOT_WARMUP%s warmup =====
    if not exist "%OUT%\pilot" mkdir "%OUT%\pilot"
    start "bench-pilot" /b /high /affinity %AFFINITY% /wait "%TOOL%" bench ^
        --mode ctr --key "%KEY%" --size 1048576 --runs 300 --warmup %PILOT_WARMUP% ^
        --out "%OUT%\pilot\pilot_ctr_1MB.csv"
    if errorlevel 1 (
        echo pilot failed
        exit /b 1
    )
    if exist "%OUT%\summary.csv" (
        python scripts\check_pilot.py "%OUT%\pilot\pilot_ctr_1MB.csv" --baseline "%OUT%\summary.csv"
    ) else (
        python scripts\check_pilot.py "%OUT%\pilot\pilot_ctr_1MB.csv"
    )
    set "PILOT_RC=%ERRORLEVEL%"
    if "%PILOT_RC%"=="1" (
        echo pilot check failed
        exit /b 1
    )
    if "%PILOT_RC%"=="2" (
        echo.
        echo WARNING: the host is still noisy; the full run would be unreliable.
        if not defined FORCE (
            echo Close background apps / wait for updates to finish, or set FORCE=1.
            exit /b 3
        )
        echo FORCE=1: continuing with a noisy pilot.
    )
)

if defined PILOT_ONLY (
    echo PILOT_ONLY=1: stopping after the pilot.
    exit /b 0
)

echo ===== full benchmark =====
set "COOLDOWN=%COOLDOWN%"
start "bench-full" /b /high /affinity %AFFINITY% /wait cmd /c scripts\run_benchmarks.bat msvc
if errorlevel 1 (
    echo benchmark failed; see "%OUT%\bench.log"
    exit /b 1
)

echo ===== summaries and plots =====
python scripts\summarize_bench.py --dir "%OUT%"
if errorlevel 1 exit /b 1
python scripts\plot_compare.py --dir "%OUT%" --out-dir "%OUT%\graph"
if errorlevel 1 exit /b 1

if defined SKIP_CROSS (
    echo cross-platform plots skipped ^(SKIP_CROSS=1^)
) else (
    if /i "%OUT%"=="output\windows\benchmarks" (
        python scripts\plot_crossplatform.py
        if errorlevel 1 exit /b 1
    ) else (
        echo cross-platform plots skipped: OUT is not the default Windows benchmark directory
    )
)

echo.
echo Done. Review "%OUT%\summary_robust.csv" before updating REPORT.md.
echo If enc/dec noisy counts are high, rerun on a quieter host.
exit /b 0
