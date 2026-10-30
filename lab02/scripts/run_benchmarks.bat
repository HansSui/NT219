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

echo Running Lab 2 benchmarks for AES-128 CTR mode using %compiler%...
set "sizes=1048576 104857600"

for %%s in (%sizes%) do (
    set "csv=%out_dir%\bench_windows_%compiler%_ctr_%%s.csv"
    echo Benchmarking size %%s bytes -^> %csv%
    "%bin%" bench --size %%s --runs 100 --out "%out_dir%\bench_windows_%compiler%_ctr_%%s.csv"
)

echo Windows benchmarks completed. Results stored in %out_dir%
