@echo off
setlocal EnableExtensions

rem Build lab03 rsatool.
rem Usage: build_windows.bat [msvc|gcc|clang]
rem   msvc  -> run from "x64 Native Tools Command Prompt for VS"
rem   gcc   -> MinGW g++ on PATH
rem   clang -> MinGW clang++ on PATH

set "root=%~dp0.."
set "compiler=%~1"
if "%compiler%"=="" set "compiler=msvc"

rem OpenSSL 4.0.x or newer is required.
if not defined OPENSSL_ROOT_DIR if /i "%compiler%"=="gcc" if exist "D:\cryptolibrary\libs\openssl\gcc\include\openssl\evp.h" set "OPENSSL_ROOT_DIR=D:\cryptolibrary\libs\openssl\gcc"
if not defined OPENSSL_ROOT_DIR if /i "%compiler%"=="clang" if exist "D:\cryptolibrary\libs\openssl\gcc\include\openssl\evp.h" set "OPENSSL_ROOT_DIR=D:\cryptolibrary\libs\openssl\gcc"
if not defined OPENSSL_ROOT_DIR if /i "%compiler%"=="msvc" if exist "D:\cryptolibrary\libs\openssl\msvc\include\openssl\evp.h" set "OPENSSL_ROOT_DIR=D:\cryptolibrary\libs\openssl\msvc"
if not defined OPENSSL_ROOT_DIR if exist "D:\cryptolibrary\include\openssl\evp.h" set "OPENSSL_ROOT_DIR=D:\cryptolibrary"
set "crypto_args="
if defined OPENSSL_ROOT_DIR set "crypto_args=-DOPENSSL_ROOT_DIR="%OPENSSL_ROOT_DIR%""
if defined OPENSSL_ROOT_DIR if exist "%OPENSSL_ROOT_DIR%\bin" set "PATH=%OPENSSL_ROOT_DIR%\bin;%PATH%"
rem Also cover the course layout D:\cryptolibrary\libs\openssl\<compiler>\bin.
if defined OPENSSL_ROOT_DIR if exist "%OPENSSL_ROOT_DIR%\libs\openssl\gcc\bin" set "PATH=%OPENSSL_ROOT_DIR%\libs\openssl\gcc\bin;%PATH%"
if defined OPENSSL_ROOT_DIR if exist "%OPENSSL_ROOT_DIR%\libs\openssl\clang\bin" set "PATH=%OPENSSL_ROOT_DIR%\libs\openssl\clang\bin;%PATH%"
if defined OPENSSL_ROOT_DIR if exist "%OPENSSL_ROOT_DIR%\libs\openssl\msvc\bin" set "PATH=%OPENSSL_ROOT_DIR%\libs\openssl\msvc\bin;%PATH%"

set "cmake=cmake"
if /i "%compiler%"=="msvc" goto msvc
if /i "%compiler%"=="gcc" goto mingw
if /i "%compiler%"=="clang" goto mingw
echo usage: %~nx0 [msvc^|gcc^|clang] >&2
exit /b 1

:msvc
set "build=%root%\build\windows-msvc"
if not defined VSINSTALLDIR (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VSINSTALLDIR=%%i\"
)
if defined VSINSTALLDIR if exist "%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "cmake=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "generator=Visual Studio 18 2026"
set "generators=%TEMP%\lab3_cmake_gen.txt"
"%cmake%" --help > "%generators%" 2>nul
findstr /c:"Visual Studio 18 2026" "%generators%" >nul || set "generator=Visual Studio 17 2022"
del "%generators%" >nul 2>nul
echo [msvc] Using generator: %generator%
"%cmake%" -S "%root%" -B "%build%" -G "%generator%" -A x64 %crypto_args%
if errorlevel 1 exit /b 1
"%cmake%" --build "%build%" --config Release --parallel
if errorlevel 1 exit /b 1
pushd "%build%"
ctest -C Release --output-on-failure
set "result=%errorlevel%"
popd
call :save_evidence "%build%" "msvc" "%build%\Release\rsatool.exe"
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
call :save_evidence "%build%" "%compiler%" "%build%\rsatool.exe"
exit /b %result%

:save_evidence
rem %1 = build dir, %2 = compiler label, %3 = rsatool path
if not exist "%root%\output\windows\test_results" mkdir "%root%\output\windows\test_results"
if exist "%~1\Testing\Temporary\LastTest.log" copy /y "%~1\Testing\Temporary\LastTest.log" "%root%\output\windows\test_results\ctest_%~2.txt" >nul
if exist "%~3" "%~3" --kat "%root%\kat\oaep_vectors.json" > "%root%\output\windows\test_results\kat_%~2.txt" 2>&1
exit /b 0
