@echo off
setlocal
cd /d "%~dp0.."

rem Prefer the CMake bundled with Visual Studio; the standalone CMake on PATH may
rem not know the newest Visual Studio generator yet.
set "CMAKE_EXE=cmake"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_DIR=%%i"
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion`) do set "VS_VERSION=%%i"
)
if defined VS_DIR if exist "%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
if not defined CMAKE_GENERATOR if defined VS_VERSION for /f "tokens=1 delims=." %%v in ("%VS_VERSION%") do set "VS_MAJOR=%%v"
if not defined CMAKE_GENERATOR if "%VS_MAJOR%"=="18" set "CMAKE_GENERATOR=Visual Studio 18 2026"
if not defined CMAKE_GENERATOR if "%VS_MAJOR%"=="17" set "CMAKE_GENERATOR=Visual Studio 17 2022"
if not defined CMAKE_GENERATOR set "CMAKE_GENERATOR=Visual Studio 17 2022"

rem Crypto++ discovery: explicit *_INCLUDE_DIR/*_LIBRARY wins, then CRYPTOPP_ROOT,
rem then the hand-built checkout used by this repository.
set "CRYPTOPP_ARGS="
if defined CRYPTOPP_INCLUDE_DIR if defined CRYPTOPP_LIBRARY (
    set "CRYPTOPP_ARGS=-DCRYPTOPP_INCLUDE_DIR=%CRYPTOPP_INCLUDE_DIR% -DCRYPTOPP_LIBRARY=%CRYPTOPP_LIBRARY%"
)
if not defined CRYPTOPP_ARGS if defined CRYPTOPP_ROOT (
    set "CRYPTOPP_ARGS=-DCRYPTOPP_ROOT=%CRYPTOPP_ROOT%"
)
if not defined CRYPTOPP_ARGS if exist "D:\cryptolibrary\include\cryptopp\aes.h" (
    set "CRYPTOPP_ARGS=-DCRYPTOPP_INCLUDE_DIR=D:\cryptolibrary\include -DCRYPTOPP_LIBRARY=D:\cryptolibrary\libs\cryptopp\msvc\cryptlib.lib"
)
if not defined CRYPTOPP_ARGS (
    echo Crypto++ was not found.
    echo Set CRYPTOPP_ROOT, or set CRYPTOPP_INCLUDE_DIR and CRYPTOPP_LIBRARY.
    exit /b 1
)

set "ARCH_ARG=-A x64"
echo %CMAKE_GENERATOR% | findstr /b /c:"Visual Studio" >nul || set "ARCH_ARG="

"%CMAKE_EXE%" -S . -B build_gui -G "%CMAKE_GENERATOR%" %ARCH_ARG% ^
      -DBUILD_SHARED_LIBS=ON ^
      %CRYPTOPP_ARGS% || exit /b 1
"%CMAKE_EXE%" --build build_gui --config Release || exit /b 1

echo.
echo Built the shared library. Expected path:
echo     build_gui\Release\aescore.dll
echo Run the GUI with:
echo     gui\run_gui.bat
