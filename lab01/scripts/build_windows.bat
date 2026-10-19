@echo off
setlocal
set "root=%~dp0.."
set "compiler=%~1"
if "%compiler%"=="" set "compiler=msvc"

set "extra="
if defined CRYPTOPP_ROOT set "extra=-DCRYPTOPP_ROOT=%CRYPTOPP_ROOT%"

if /i "%compiler%"=="msvc" goto msvc
if /i "%compiler%"=="gcc" goto mingw
if /i "%compiler%"=="clang" goto mingw
echo usage: %~nx0 [msvc^|gcc^|clang] >&2
exit /b 1

:msvc
cmake -S "%root%" -B "%root%\build_windows_msvc" -G "Visual Studio 17 2022" -A x64 %extra%
if errorlevel 1 exit /b 1
cmake --build "%root%\build_windows_msvc" --config Release
if errorlevel 1 exit /b 1
pushd "%root%\build_windows_msvc"
ctest -C Release --output-on-failure
set "result=%errorlevel%"
popd
exit /b %result%

:mingw
set "build=%root%\build_windows_%compiler%"
if /i "%compiler%"=="gcc" set "cxx=g++"
if /i "%compiler%"=="clang" set "cxx=clang++"
cmake -S "%root%" -B "%build%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=%cxx% %extra%
if errorlevel 1 exit /b 1
cmake --build "%build%"
if errorlevel 1 exit /b 1
pushd "%build%"
ctest --output-on-failure
set "result=%errorlevel%"
popd
exit /b %result%
