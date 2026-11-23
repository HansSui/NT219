@echo off
setlocal EnableExtensions
set ROOT=%~dp0..
set COMPILER=%1
if "%COMPILER%"=="" set COMPILER=msvc
if /I "%COMPILER%"=="msvc" (
    set TOOL=%ROOT%\build\windows-msvc\Release\hashtool.exe
) else (
    set TOOL=%ROOT%\build\windows-%COMPILER%\hashtool.exe
)
set OUTDIR=%ROOT%\output\windows\benchmarks
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
if not defined ALGOS set ALGOS=sha256,sha512,sha3-256,sha3-512
if not defined SIZES set SIZES=1,100,1024
if not defined IO set IO=stream,mmap
if not defined WARMUP set WARMUP=1

set RUNS_ARG=
if defined RUNS set RUNS_ARG=--runs %RUNS%

"%TOOL%" bench --algos "%ALGOS%" --sizes "%SIZES%" --io "%IO%" --warmup %WARMUP% %RUNS_ARG% --out "%OUTDIR%\bench_windows_%COMPILER%.csv"
if errorlevel 1 exit /b 1
python "%ROOT%\scripts\summarize_bench.py" --input "%OUTDIR%\bench_windows_%COMPILER%.csv" --out "%OUTDIR%\summary_windows_%COMPILER%.csv"
if errorlevel 1 exit /b 1
python "%ROOT%\scripts\plot_bench.py" --input "%OUTDIR%\summary_windows_%COMPILER%.csv" --out-dir "%OUTDIR%\graphs" --platform "Windows %COMPILER%"
if errorlevel 1 exit /b 1
echo benchmarks written to %OUTDIR%
