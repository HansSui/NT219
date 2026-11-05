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

rem Crypto++ is oracle-only, but CMake requires it when ENABLE_CROSSVAL=ON.
rem Expected layout: %CRYPTOPP_ROOT%\include\cryptopp\*.h
rem                  %CRYPTOPP_ROOT%\libs\cryptopp\{msvc,gcc,clang}\*
if not defined CRYPTOPP_ROOT if exist "D:\cryptolibrary\include\cryptopp\aes.h" set "CRYPTOPP_ROOT=D:\cryptolibrary"

set "crypto_args="
if defined CRYPTOPP_ROOT set "crypto_args=-DCRYPTOPP_ROOT="%CRYPTOPP_ROOT%""

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

if defined CRYPTOPP_ROOT if exist "%CRYPTOPP_ROOT%\include\cryptopp\aes.h" set "crypto_args=%crypto_args% -DCRYPTOPP_INCLUDE_DIR="%CRYPTOPP_ROOT%/include""
if defined CRYPTOPP_ROOT if exist "%CRYPTOPP_ROOT%\libs\cryptopp\msvc\cryptlib.lib" set "crypto_args=%crypto_args% -DCRYPTOPP_LIBRARY="%CRYPTOPP_ROOT%/libs/cryptopp/msvc/cryptlib.lib""

echo [msvc] Using generator: %generator%
if defined CRYPTOPP_ROOT echo [msvc] Crypto++:     %CRYPTOPP_ROOT%
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
rem The local clang/libcryptopp.a is not ABI-compatible with MinGW clang in
rem this tree. The GCC-built archive links and passes, so use it for both
rem MinGW compilers; override CRYPTOPP_LIBRARY if your install differs.
set "mingw_lib_dir=gcc"
set "mingw_crypto_args=%crypto_args%"
if defined CRYPTOPP_ROOT if exist "%CRYPTOPP_ROOT%\libs\cryptopp\%mingw_lib_dir%\libcryptopp.a" set "mingw_crypto_args=%mingw_crypto_args% -DCRYPTOPP_LIBRARY="%CRYPTOPP_ROOT%/libs/cryptopp/%mingw_lib_dir%/libcryptopp.a""
if defined CRYPTOPP_ROOT echo [%compiler%] Crypto++: %CRYPTOPP_ROOT%
"%cmake%" -S "%root%" -B "%build%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=%cxx% %mingw_crypto_args%
if errorlevel 1 exit /b 1
"%cmake%" --build "%build%" --parallel
if errorlevel 1 exit /b 1
pushd "%build%"
ctest --output-on-failure
set "result=%errorlevel%"
popd
exit /b %result%
