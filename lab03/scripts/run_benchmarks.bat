@echo off
setlocal EnableExtensions

rem Run the Lab 3 RSA/hybrid benchmark matrix on Windows and write CSV, summary
rem and plots. Mirrors scripts/run_benchmarks.sh.
rem Usage: run_benchmarks.bat [msvc|gcc|clang]
rem Override with RUNS_RSA, RUNS_SMALL, RUNS_LARGE, WARMUP, THREADS, PIN_MASK,
rem PRIORITY or OUT_DIR.

set "root=%~dp0.."
set "compiler=%~1"
if "%compiler%"=="" set "compiler=msvc"

set "bin=%root%\build\windows-%compiler%\Release\rsatool.exe"
if not exist "%bin%" set "bin=%root%\build\windows-%compiler%\rsatool.exe"
if not exist "%bin%" (
    echo Binary not found at %bin%. Running build_windows.bat first...
    call "%root%\scripts\build_windows.bat" %compiler%
    if errorlevel 1 exit /b 1
)

if not defined OUT_DIR set "OUT_DIR=%root%\output\windows\benchmarks"
set "out_dir=%OUT_DIR%"
if not exist "%out_dir%" mkdir "%out_dir%"

rem Chosen matrix: RSA 1000, AES/hybrid <=1 MiB 1000, 100 MiB 100.
if not defined RUNS_RSA set "RUNS_RSA=1000"
if not defined RUNS_SMALL set "RUNS_SMALL=1000"
if not defined RUNS_LARGE set "RUNS_LARGE=100"
if not defined WARMUP set "WARMUP=1.0"
if not defined THREADS set "THREADS=4"

rem PIN_MASK: hex CPU affinity mask (no 0x). F = logical CPUs 0-3, 4 = CPU 2.
rem Set PIN_MASK=0 to disable pinning.
if not defined PIN_MASK (
    if %THREADS% GTR 1 (set "PIN_MASK=F") else (set "PIN_MASK=4")
)
if not defined PRIORITY set "PRIORITY=high"

set "start_opts=/wait /b /%PRIORITY%"
if not "%PIN_MASK%"=="0" set "start_opts=%start_opts% /affinity %PIN_MASK%"

rem Switch to the High performance power plan, restore it afterwards.
set "old_scheme="
for /f "tokens=4" %%G in ('powercfg /getactivescheme 2^>nul') do set "old_scheme=%%G"
powercfg /setactive SCHEME_MIN >nul 2>&1

set "csv=%out_dir%\bench_windows_%compiler%_rsa_hybrid.csv"
set "summary=%out_dir%\summary_%compiler%.csv"

echo Running Lab 3 RSA/hybrid benchmarks using %compiler%...
echo Matrix: RSA %RUNS_RSA%, ^<=1 MiB %RUNS_SMALL%, 100 MiB %RUNS_LARGE%, warm-up %WARMUP%s
echo Priority: %PRIORITY%, affinity mask: %PIN_MASK%, threads: %THREADS%
if %THREADS% GTR 1 if "%PIN_MASK%"=="4" echo WARNING: PIN_MASK=4 selects one logical CPU; %THREADS% threads will share it. Use PIN_MASK=0 or a 4-CPU mask such as PIN_MASK=F.

start "" %start_opts% "%bin%" bench --runs-rsa %RUNS_RSA% --runs-small %RUNS_SMALL% --runs-large %RUNS_LARGE% --threads %THREADS% --warmup %WARMUP% --out "%csv%" --verbose
set "rc=%errorlevel%"
if not "%rc%"=="0" (
    echo FAIL: benchmark run
    goto done
)

python "%root%\scripts\summarize_bench.py" --input "%csv%" --out "%summary%"
if errorlevel 1 set "rc=1" & goto done
python "%root%\scripts\plot_bench.py" --input "%csv%" --out-dir "%out_dir%\graph" --platform "windows-%compiler%"
if errorlevel 1 set "rc=1" & goto done

echo Windows benchmarks completed. Results stored in %out_dir%

:done
if defined old_scheme powercfg /setactive %old_scheme% >nul 2>&1
exit /b %rc%
