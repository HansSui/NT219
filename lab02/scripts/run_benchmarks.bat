@echo off
setlocal EnableExtensions

set "root=%~dp0.."
set "compiler=%~1"
if "%compiler%"=="" set "compiler=msvc"

set "bin=%root%\build\windows-%compiler%\Release\aestool.exe"
if not exist "%bin%" set "bin=%root%\build\windows-%compiler%\aestool.exe"
if not exist "%bin%" (
    echo Binary not found at %bin%. Running build_windows.bat first...
    call "%root%\scripts\build_windows.bat" %compiler%
)

set "out_dir=%root%\output\windows\benchmarks"
if not exist "%out_dir%" mkdir "%out_dir%"

rem Chosen matrix: 1 MiB/1000, 100 MiB/100, 1 GiB/30.
rem This keeps N >= 30 for the large cases; the blueprint's nominal 500/50
rem is available via RUNS_100MB=500 RUNS_1GB=50.
rem Override with RUNS, RUNS_1MB, RUNS_100MB or RUNS_1GB.
if not defined RUNS_1MB set "RUNS_1MB=1000"
if not defined RUNS_100MB set "RUNS_100MB=100"
if not defined RUNS_1GB set "RUNS_1GB=30"
if defined RUNS (
    set "RUNS_1MB=%RUNS%"
    set "RUNS_100MB=%RUNS%"
    set "RUNS_1GB=%RUNS%"
)
if not defined WARMUP set "WARMUP=1.0"
rem Benchmark worker threads. Override with THREADS=1 for a single-thread run.
if not defined THREADS set "THREADS=4"

rem ---- Noise reduction ----
rem PIN_MASK: hex CPU affinity mask (no 0x). F = logical CPUs 0-3, 4 = CPU 2.
rem For THREADS=1 the default avoids CPU 0. For THREADS>1 the default uses four
rem logical CPUs so the worker threads actually run in parallel; override it
rem explicitly, or set PIN_MASK=0 to disable pinning.
rem PRIORITY: high | abovenormal | normal
if not defined PIN_MASK (
    if %THREADS% GTR 1 (set "PIN_MASK=F") else (set "PIN_MASK=4")
)
if not defined PRIORITY set "PRIORITY=high"

set "start_opts=/wait /b /%PRIORITY%"
if not "%PIN_MASK%"=="0" set "start_opts=%start_opts% /affinity %PIN_MASK%"

rem Switch to High performance power plan, restore it afterwards.
set "old_scheme="
for /f "tokens=4" %%G in ('powercfg /getactivescheme 2^>nul') do set "old_scheme=%%G"
powercfg /setactive SCHEME_MIN >nul 2>&1

echo Running Lab 2 AES-128 CTR benchmarks using %compiler%...
echo Matrix: 1 MiB/%RUNS_1MB%, 100 MiB/%RUNS_100MB%, 1 GiB/%RUNS_1GB%, warm-up %WARMUP%s
echo Priority: %PRIORITY%, affinity mask: %PIN_MASK%, threads: %THREADS%
if %THREADS% GTR 1 if "%PIN_MASK%"=="4" echo WARNING: PIN_MASK=4 selects one logical CPU; %THREADS% threads will share it. Use PIN_MASK=0 or a 4-CPU mask such as PIN_MASK=F.

set "rc=0"
call :run_one 1048576 %RUNS_1MB%
if errorlevel 1 set "rc=1" & goto :done
call :run_one 104857600 %RUNS_100MB%
if errorlevel 1 set "rc=1" & goto :done
call :run_one 1073741824 %RUNS_1GB%
if errorlevel 1 set "rc=1" & goto :done

echo Windows benchmarks completed. Results stored in %out_dir%

:done
if defined old_scheme powercfg /setactive %old_scheme% >nul 2>&1
exit /b %rc%

:run_one
set "size=%~1"
set "runs=%~2"
set "csv=%out_dir%\bench_windows_%compiler%_ctr_%size%.csv"
echo Benchmarking size %size% bytes, %runs% runs -^> %csv%
start "" %start_opts% "%bin%" bench --mode ctr --size %size% --runs %runs% --warmup %WARMUP% --threads %THREADS% --verbose --out "%csv%"
if errorlevel 1 (
    echo FAIL: benchmark size=%size%
    exit /b 1
)
exit /b 0