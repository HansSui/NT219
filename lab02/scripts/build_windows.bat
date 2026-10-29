@echo off
setlocal EnableExtensions

rem Build lab02 aestool.
rem Usage: build_windows.bat [msvc|gcc|clang]
rem   msvc  -> run from "Developer Command Prompt for VS"
rem   gcc   -> MinGW g++ on PATH
rem   clang -> MinGW clang++ on PATH

set "root=%~dp0.."
set "compiler=%~1"
if "%compiler%"=="" set "compiler=msvc"

set "cmake=cmake"
if /i "%compiler%"=="msvc" goto msvc
if /i "%compiler%"=="gcc" goto mingw
if /i "%compiler%"=="clang" goto mingw

echo usage: %~nx0 [msvc^|gcc^|clang] >&2
exit /b 1

:msvc
if not defined VSINSTALLDIR (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VSINSTALLDIR=%%i\"
)
if defined VSINSTALLDIR if exist "%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "cmake=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

set "generator=Visual Studio 18 2026"
set "generators=%TEMP%\lab2_cmake_gen.txt"
"%cmake%" --help > "%generators%" 2>nul
findstr /c:"Visual Studio 18 2026" "%generators%" >nul || set "generator=Visual Studio 17 2022"
del "%generators%" >nul 2>nul

echo [msvc] Using generator: %generator%
"%cmake%" -S "%root%" -B "%root%\build\windows-msvc" -G "%generator%" -A x64
if errorlevel 1 exit /b 1
"%cmake%" --build "%root%\build\windows-msvc" --config Release --parallel
if errorlevel 1 exit /b 1
pushd "%root%\build\windows-msvc"
ctest -C Release --output-on-failure
set "result=%errorlevel%"
popd
exit /b %result%

:mingw
set "build=%root%\build\windows-%compiler%"
if /i "%compiler%"=="gcc" set "cxx=g++"
if /i "%compiler%"=="clang" set "cxx=clang++"
"%cmake%" -S "%root%" -B "%build%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=%cxx%
if errorlevel 1 exit /b 1
"%cmake%" --build "%build%" --parallel
if errorlevel 1 exit /b 1
pushd "%build%"
ctest --output-on-failure
set "result=%errorlevel%"
popd
exit /b %result%
