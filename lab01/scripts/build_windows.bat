@echo off
setlocal EnableExtensions

rem Build lab01 aestool.
rem Usage: build_windows.bat [msvc|gcc|clang]
rem   msvc  -> run from a "Developer Command Prompt for VS 2026" (or 2022)
rem   gcc   -> MinGW g++ on PATH
rem   clang -> MinGW clang++ on PATH
rem
rem Optional environment:
rem   CRYPTOPP_ROOT  Crypto++ prefix (default D:\cryptolibrary when present)

set "root=%~dp0.."
set "compiler=%~1"
if "%compiler%"=="" set "compiler=msvc"

rem --- Crypto++ location -------------------------------------------------
rem Expected layout for the local install:
rem   %CRYPTOPP_ROOT%\include\cryptopp\*.h
rem   %CRYPTOPP_ROOT%\libs\cryptopp\msvc\cryptlib.lib   (MSVC static lib)
if not defined CRYPTOPP_ROOT if exist "D:\cryptolibrary\include\cryptopp\aes.h" set "CRYPTOPP_ROOT=D:\cryptolibrary"

set "crypto_args="
if defined CRYPTOPP_ROOT set "crypto_args=-DCRYPTOPP_ROOT="%CRYPTOPP_ROOT%""

rem Default CMake. The MSVC branch below prefers the CMake bundled with Visual
rem Studio; the MinGW branches use this one from PATH.
set "cmake=cmake"

if /i "%compiler%"=="msvc" goto msvc
if /i "%compiler%"=="gcc" goto mingw
if /i "%compiler%"=="clang" goto mingw
echo usage: %~nx0 [msvc^|gcc^|clang] >&2
exit /b 1

:msvc
rem Prefer the CMake bundled with Visual Studio: CMake 4.x knows the
rem "Visual Studio 18 2026" generator, older stand-alone CMake does not.
if not defined VSINSTALLDIR (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VSINSTALLDIR=%%i\"
)
set "cmake=cmake"
if defined VSINSTALLDIR if exist "%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "cmake=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

rem Pick the newest Visual Studio generator this CMake supports.
set "generators=%TEMP%\aestool_cmake_generators.txt"
"%cmake%" --help > "%generators%" 2>nul
set "generator="
findstr /c:"Visual Studio 18 2026" "%generators%" >nul && set "generator=Visual Studio 18 2026"
if not defined generator (
    findstr /c:"Visual Studio 17 2022" "%generators%" >nul
    if not errorlevel 1 set "generator=Visual Studio 17 2022"
)
del "%generators%" >nul 2>nul
if not defined generator (
    echo No Visual Studio generator found. Open a Developer Command Prompt for VS and retry. >&2
    exit /b 1
)

rem The local Crypto++ keeps its MSVC static library outside lib\.
if defined CRYPTOPP_ROOT if exist "%CRYPTOPP_ROOT%\include\cryptopp\aes.h" set "crypto_args=%crypto_args% -DCRYPTOPP_INCLUDE_DIR="%CRYPTOPP_ROOT%/include""
if defined CRYPTOPP_ROOT if exist "%CRYPTOPP_ROOT%\libs\cryptopp\msvc\cryptlib.lib" set "crypto_args=%crypto_args% -DCRYPTOPP_LIBRARY="%CRYPTOPP_ROOT%/libs/cryptopp/msvc/cryptlib.lib""

echo [msvc] cmake:     %cmake%
echo [msvc] generator: %generator%
if defined CRYPTOPP_ROOT echo [msvc] cryptopp:  %CRYPTOPP_ROOT%

"%cmake%" -S "%root%" -B "%root%\build\windows-msvc" -G "%generator%" -A x64 %crypto_args%
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
"%cmake%" -S "%root%" -B "%build%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=%cxx% %crypto_args%
if errorlevel 1 exit /b 1
"%cmake%" --build "%build%" --parallel
if errorlevel 1 exit /b 1
pushd "%build%"
ctest --output-on-failure
set "result=%errorlevel%"
popd
exit /b %result%
