# Lab Requirements & Toolchain Dependencies: Ubuntu & Windows

> **Course:** NT219 — Information Security / Cryptography Laboratory  
> **Scope:** Comprehensive dependency specification for Compilers (**GCC, G++, Clang, Clang++, MSVC**), Cryptographic Libraries (**Crypto++, OpenSSL**), and Supporting Toolchains across **Ubuntu Linux** and **Microsoft Windows**, extracted from all project Markdown documents, specifications, and reports (`docs/lab01-brief.txt`, `docs/course-objectives.txt`, `lab01/README.md`, `lab01/REPORT.md`, `LAB1_LINUX_TODO.md`, `LAB1_WINDOWS_TODO.md`, etc.).

---

## 1. Quick Reference: System & Dependency Matrix

| Category | Component / Tool | Ubuntu Linux | Microsoft Windows (MSVC) | Microsoft Windows (MinGW64) |
|---|---|---|---|---|
| **OS Verified** | Operating System | Ubuntu 26.04.1 LTS (Kernel 7.0.0-38-generic x86_64) | Microsoft Windows 11 Home (Build 10.0.26300, x64) | Microsoft Windows 11 Home (MSYS2 MinGW 64-bit) |
| **C++ Compiler** | **GCC / G++** | GCC 15.2.0 (`g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`) | *N/A (MSVC native environment)* | GCC 16.2.0 (`mingw-w64-x86_64-gcc`) |
| **C++ Compiler** | **Clang / Clang++** | Clang 21.1.8 (`Ubuntu clang version 21.1.8`) | *N/A (or ClangCL via VS)* | Clang 22.1.8 (`mingw-w64-x86_64-clang`) |
| **C++ Compiler** | **MSVC** | *N/A (Proprietary to Windows)* | MSVC 19.51.36260.0 (Visual Studio 18 / 2026, x64) / VS 2022 | *N/A* |
| **Crypto Primitive** | **Crypto++** | 8.9.0 (`libcryptopp-dev` / static `/usr/local`) | 8.9.0 (Static `cryptlib.lib` with `/MT` CRT) | 8.9.0 (Compiled from source with `PREFIX=/c/cryptopp-mingw`) |
| **Crypto Library** | **OpenSSL** | **Prohibited for Lab 1 AES primitives**; Available via `libssl-dev` for general labs | **Prohibited for Lab 1 AES primitives**; Located at `D:\cryptolibrary` for general labs | **Prohibited for Lab 1 AES primitives**; MSYS2 `mingw-w64-x86_64-openssl` |
| **Build System** | **CMake** | CMake ≥ 3.16 (Verified: 4.2.3) | CMake ≥ 3.16 (Verified: 4.3.1 bundled with VS) | CMake ≥ 3.16 (`mingw-w64-x86_64-cmake`) |
| **Test Suite** | **Catch2 & CTest** | Catch2 v2.13.10 (vendored header) + CTest | Catch2 v2.13.10 (vendored header) + CTest | Catch2 v2.13.10 (vendored header) + CTest |
| **GUI & Plots** | **Python / GUI** | Python 3.8+, `python3-tk`, `python3-matplotlib` | Python 3.8+ (standard Tkinter), `pip install matplotlib` | Python 3.8+ |

---

## 2. Compilers & Toolchains Breakdown

### 2.1 GCC / G++ (GNU Compiler Collection)

#### Ubuntu Linux
* **Role:** Primary production compiler for Linux builds, unit testing, and benchmark data collection.
* **Verified Version:** `g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0` (C++17 standard).
* **Package Management (APT):**
  ```bash
  sudo apt update
  sudo apt install -y build-essential gcc g++
  ```
* **Required Flags & Options:**
  * Standard: `-std=c++17` (`CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_EXTENSIONS OFF`).
  * Warnings: `-Wall -Wextra`.
  * Optimization: `-O2` (Release build).
  * Shared Library requirement: `-fPIC` (Position Independent Code) when compiling `libaescore.so` for GUI usage.
* **Build Targets:**
  * Static CLI: `build/ubuntu-gcc/aestool` (`scripts/build_linux.sh gcc`).
  * Shared Library (GUI): `build/ubuntu-gcc-shared/libaescore.so` (`cmake -S . -B build/ubuntu-gcc-shared -DBUILD_SHARED_LIBS=ON`).
* **Validation Evidence:** KAT 16/16 vectors passed (`output/ubuntu/test_results/kat_gcc.txt`), `ctest` 2/2 passed (`ctest_gcc.txt`), full 48-CSV benchmark matrix recorded under CPU pinning (`PIN_CPUS=2`, `performance` governor).

#### Microsoft Windows (MinGW-w64)
* **Role:** Portability verification and cross-compiler consistency testing on Windows.
* **Verified Version:** GCC 16.2.0 (MSYS2 64-bit environment).
* **Package Management (MSYS2 Pacman):**
  ```bash
  pacman -S --needed mingw-w64-x86_64-gcc make
  ```
* **Build Targets & Execution:**
  * MSYS2 MinGW 64-bit Terminal or `cmd.exe` with `C:\msys64\mingw64\bin` on `PATH`.
  * CLI Executable: `build\windows-gcc\aestool.exe` (`scripts\build_windows.bat gcc`).
* **Validation Evidence:** KAT 16/16 passed (`output/windows/test_results/kat_mingw_gcc.txt`), `ctest` 2/2 passed (`ctest_mingw_gcc.txt`).

---

### 2.2 Clang / Clang++ (LLVM Toolchain)

#### Ubuntu Linux
* **Role:** Secondary Linux compiler for static checking, alternative code generation, and sanity verification.
* **Verified Version:** `Ubuntu clang version 21.1.8 (6ubuntu1)` (Target: `x86_64-pc-linux-gnu`).
* **Package Management (APT):**
  ```bash
  sudo apt update
  sudo apt install -y clang
  ```
* **Required Flags & Options:**
  * Standard: `-std=c++17`.
  * Warnings: `-Wall -Wextra`.
  * CMake Invocation: `-DCMAKE_CXX_COMPILER=clang++`.
* **Build Targets:**
  * Static CLI: `build/ubuntu-clang/aestool` (`scripts/build_linux.sh clang`).
* **Validation Evidence:** KAT 16/16 vectors passed (`output/ubuntu/test_results/kat_clang.txt`), `ctest` 2/2 passed (`ctest_clang.txt`). *(Note: Benchmarks were intentionally restricted to GCC in the final report to avoid mixed compiler noise).*

#### Microsoft Windows (MinGW-w64 Clang)
* **Role:** Verification of LLVM/Clang on Windows under the MinGW environment.
* **Verified Version:** Clang 22.1.8 (MSYS2 64-bit environment).
* **Package Management (MSYS2 Pacman):**
  ```bash
  pacman -S --needed mingw-w64-x86_64-clang
  ```
* **Build Targets & Execution:**
  * CLI Executable: `build\windows-clang\aestool.exe` (`scripts\build_windows.bat clang`).
* **Validation Evidence:** KAT 16/16 passed (`output/windows/test_results/kat_mingw_clang.txt`), `ctest` 2/2 passed (`ctest_mingw_clang.txt`).

---

### 2.3 MSVC (Microsoft Visual C++ Compiler)

#### Microsoft Windows (Native Toolchain)
* **Role:** Primary Windows compiler for official benchmarks, production executable, negative tests, and shared library (`aescore.dll`) used by the Tkinter GUI.
* **Verified Version:** MSVC `19.51.36260.0` (Visual Studio 18 / 2026, x64); compatible with Visual Studio 17 / 2022.
* **Installation Prerequisites:**
  * Visual Studio Installer -> Workload: **"Desktop development with C++"** (`Microsoft.VisualStudio.Component.VC.Tools.x86.x64`).
  * CMake tools for Windows (`Microsoft.VisualStudio.Component.VC.CMake.Project`).
  * Windows 10/11 SDK.
  * Environment: Run from **"x64 Native Tools Command Prompt for VS 2026"** (or `vcvars64.bat`).
* **Required Flags & CRT Linkage Settings:**
  * Standard: `/std:c++17`.
  * Warnings: `/W4`.
  * Optimizations: `/O2 /Ob2` (in Release mode).
  * **Critical Runtime Linkage (`/MT` vs `/MD`):**
    * The static Crypto++ library (`cryptlib.lib`) is customarily compiled with `/MT` (Static C Runtime).
    * Linking `/MT` with CMake's default dynamic `/MD` causes linker conflict **`LNK2038: mismatch detected for 'RuntimeLibrary': value 'MT_StaticRelease' doesn't match value 'MD_DynamicRelease'`**.
    * `lab01/CMakeLists.txt` sets:
      ```cmake
      if(MSVC AND CRYPTOPP_STATIC_CRT AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
          set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
      endif()
      ```
    * For vcpkg or dynamic CRT builds, pass `-DCRYPTOPP_STATIC_CRT=OFF`.
* **Build Targets:**
  * CLI Executable: `build\windows-msvc\Release\aestool.exe` (`scripts\build_windows.bat msvc`).
  * Shared Library (GUI): `build\windows-msvc-shared\Release\aescore.dll` (`gui\build_gui.bat`).
* **Validation Evidence:** KAT 16/16 passed (`output/windows/test_results/kat_msvc.txt`), `ctest` 2/2 passed (`ctest_msvc.txt`), 17 negative edge cases verified (`output/windows/neg_results/`), full 48-CSV controlled benchmark suite (CPU pinned to logical core 2 via `start /affinity 4`, High Performance power profile, high process priority).

#### Ubuntu Linux
* **Status:** **N/A** (MSVC is proprietary to Windows and cannot run natively on Linux. Cross-compilation via wine/clang-cl is not used in the lab).

---

## 3. Cryptographic Libraries: Crypto++ & OpenSSL

### 3.1 Crypto++ (CryptoPP / cryptlib)

#### Project Role & Laboratory Requirement
* **Mandatory Foundation:** All cryptographic primitives for Lab 1 (AES-128/192/256 in ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM, random generation via `AutoSeededRandomPool`, zeroization via `SecureWipeBuffer`, byte buffers via `SecByteBlock`) **must strictly use Crypto++**.
* **Verified Library Version:** Crypto++ **8.9.0** (`CRYPTOPP_VERSION 890`).

#### Ubuntu Linux Setup
1. **System Package (Recommended):**
   ```bash
   sudo apt update
   sudo apt install -y libcryptopp-dev
   ```
   * Header Directory: `/usr/include/cryptopp/`
   * Static Library: `/usr/lib/x86_64-linux-gnu/libcryptopp.a` (or `/usr/local/lib/libcryptopp.a`)
   * Distribution package is pre-compiled with `-fPIC`, allowing seamless linking into `libaescore.so`.
2. **Build from Source (Custom Prefix):**
   ```bash
   git clone --depth 1 https://github.com/weidai11/cryptopp.git
   cd cryptopp
   CXXFLAGS="-fPIC -O3" make -j"$(nproc)"
   sudo make install PREFIX=/usr/local
   ```
3. **CMake Configuration Option:**
   ```bash
   cmake -S . -B build -DCRYPTOPP_ROOT=/usr/local
   ```

#### Microsoft Windows Setup
1. **Prebuilt / Hand-built MSVC Tree (Current Lab Setup):**
   * Expected layout on local disk:
     * Headers: `D:\cryptolibrary\include\cryptopp\*.h`
     * Static Library: `D:\cryptolibrary\libs\cryptopp\msvc\cryptlib.lib`
   * CMake configuration:
     ```bat
     cmake -S . -B build\windows-msvc -G "Visual Studio 18 2026" -A x64 ^
           -DCRYPTOPP_INCLUDE_DIR=D:/cryptolibrary/include ^
           -DCRYPTOPP_LIBRARY=D:/cryptolibrary/libs/cryptopp/msvc/cryptlib.lib
     ```
2. **vcpkg Alternative:**
   ```bat
   vcpkg install cryptopp:x64-windows
   cmake -S . -B build\windows-msvc -G "Visual Studio 18 2026" -A x64 ^
         -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
         -DCRYPTOPP_STATIC_CRT=OFF
   ```
3. **MinGW64 (MSYS2) Build:**
   * MSYS2 does not supply a prebuilt `cryptopp` package in the official repositories. It must be built from source:
     ```bash
     pacman -S --needed git make mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
     git clone --depth 1 https://github.com/weidai11/cryptopp.git
     cd cryptopp
     make -f GNUmakefile -j"$(nproc)"
     make -f GNUmakefile install PREFIX=/c/cryptopp-mingw
     ```
   * Configure project with:
     ```bat
     set CRYPTOPP_ROOT=C:\cryptopp-mingw
     scripts\build_windows.bat gcc
     ```

---

### 3.2 OpenSSL

#### Policy & Role in Course vs Lab 1
* **Lab 1 Constraint:** **STRICTLY PROHIBITED FOR CIPHER PRIMITIVES.**
  * *From `docs/lab01-brief.txt` & `lab01/README.md`:*
    > "⚠ Constraint: Crypto++ library must be used for cryptographic primitives. No use of OpenSSL, libsodium, or other crypto libraries."
  * Implementing AES or AEAD modes in Lab 1 using OpenSSL `EVP_*` will violate the core assignment rubric.
* **Course-wide Scope (Labs 1–6):**
  * Mentioned in `docs/course-objectives.txt` as an approved cryptographic RNG provider (`OpenSSL RAND_bytes`) and for future labs involving RSA, ECDSA, digital signatures, X.509 certificates, and TLS handshakes.
  * Included in early setup guides (`docs/guides/report-guide-vi.txt` and `docs/guides/ubuntu-run-guide.txt`) for complete course environment preparation.

#### Ubuntu Linux Setup (For Course-wide Readiness)
* **Package Management (APT):**
  ```bash
  sudo apt install -y libssl-dev openssl
  ```
  * Header Directory: `/usr/include/openssl/`
  * Libraries: `/usr/lib/x86_64-linux-gnu/libssl.so`, `/usr/lib/x86_64-linux-gnu/libcrypto.so`
  * CLI Tool: `/usr/bin/openssl`

#### Microsoft Windows Setup (For Course-wide Readiness)
* **Local Central Installation (`D:\cryptolibrary`):**
  * OpenSSL on Windows is deployed centrally alongside Crypto++ inside the `D:\cryptolibrary` directory.
  * Headers: `D:\cryptolibrary\include\openssl\` (or `D:\cryptolibrary\include\`)
  * Libraries: `D:\cryptolibrary\libs\openssl\` or `D:\cryptolibrary\lib\` (`libssl.lib`, `libcrypto.lib` / dynamic binaries)
  * CMake Configuration:
    ```bat
    set OPENSSL_ROOT_DIR=D:\cryptolibrary
    rem In CMake: -DOPENSSL_ROOT_DIR="D:/cryptolibrary"
    ```
* **Alternative / Fallback Options:**
  1. **MSYS2 MinGW 64-bit:** `pacman -S --needed mingw-w64-x86_64-openssl`
  2. **vcpkg:** `vcpkg install openssl:x64-windows`
  3. **Win64 OpenSSL Installer:** Shining Light Productions OpenSSL installer (default `C:\Program Files\OpenSSL-Win64\`)

---

## 4. Supporting Build, Testing, & Analysis Dependencies

### 4.1 CMake Build System
* **Minimum Required Version:** CMake **3.16** (`cmake_minimum_required(VERSION 3.16)`).
* **Ubuntu Linux:**
  * Package: `sudo apt install -y cmake`
  * Verified version: CMake 4.2.3.
* **Microsoft Windows:**
  * Bundled with Visual Studio: CMake 4.3.1 (located in `%VSINSTALLDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`).
  * MSYS2: `pacman -S mingw-w64-x86_64-cmake`.

### 4.2 Unit Testing Framework (Catch2 & CTest)
* **Catch2 Version:** Catch2 **v2.13.10** single-header format.
* **Vendoring:** Directly stored at `tests/catch.hpp` inside the repository.
  * **Zero build-time network dependency:** Compiles fully offline without `FetchContent` or internet downloads.
* **CTest:** Automated runner bundled with CMake.
  * Ubuntu: `(cd build/ubuntu-gcc && ctest --output-on-failure)` (passes 2/2 tests: `aescore` unit test suite + `cli` shell test).
  * Windows: `ctest -C Release --output-on-failure` (passes 2/2 tests: `aescore` unit test suite + `cli` CMake test script `tests/cli_test.cmake`).

### 4.3 Python Stack: GUI & Performance Data Analysis
* **Python Runtime:** Python ≥ **3.8** (Verified with Python 3.10–3.14 on Linux/Windows).
* **Tkinter (GUI Extension):**
  * Calls shared library (`libaescore.so` on Linux, `aescore.dll` on Windows) via standard `ctypes`.
  * Ubuntu: Requires explicit package `sudo apt install -y python3-tk`.
  * Windows: Installed by default with the standard python.org Windows installer ("tcl/tk and IDLE" feature).
* **Matplotlib (Visualization):**
  * Used by `scripts/plot_compare.py`, `scripts/plot_crossplatform.py`, and `scripts/plot_bench.py` to generate the 8 required PNG charts.
  * Ubuntu: `sudo apt install -y python3-matplotlib`
  * Windows: `pip install matplotlib`

### 4.4 Report Generation Tools (Optional)
* **Pandoc & XeLaTeX:** For exporting `lab01/REPORT.md` to `REPORT.pdf` or `REPORT.docx`.
  * Ubuntu:
    ```bash
    sudo apt install -y pandoc texlive-xetex fonts-dejavu
    pandoc lab01/REPORT.md -o lab01/REPORT.pdf --pdf-engine=xelatex \
        -V mainfont="DejaVu Serif" -V geometry:margin=1in
    ```

---

## 5. Unified Command Matrix

### 5.1 One-Shot Environment Setup

```bash
# ==============================================================================
# UBUNTU / DEBIAN LINUX SETUP
# ==============================================================================
sudo apt update
sudo apt install -y \
    build-essential \
    gcc \
    g++ \
    clang \
    cmake \
    libcryptopp-dev \
    libssl-dev \
    python3 \
    python3-tk \
    python3-matplotlib \
    pandoc
```

```bat
@echo off
rem ==============================================================================
rem WINDOWS MSVC SETUP
rem ==============================================================================
rem 1. Install Visual Studio 2022 or 2026 with "Desktop development with C++"
rem 2. Install Python 3.8+ from python.org (ensure "Add to PATH" and "Tcl/Tk" are checked)
rem 3. Install Python matplotlib:
pip install matplotlib

rem 4. Ensure Crypto++ 8.9.0 and OpenSSL are located at D:\cryptolibrary
rem    (Crypto++ static library at D:\cryptolibrary\libs\cryptopp\msvc\cryptlib.lib)
rem    (OpenSSL headers & libs at D:\cryptolibrary)
rem    Alternatively, install via vcpkg:
rem    vcpkg install cryptopp:x64-windows openssl:x64-windows
```

```bash
# ==============================================================================
# WINDOWS MSYS2 MINGW64 SETUP (MinGW GCC & Clang)
# ==============================================================================
pacman -S --needed \
    git \
    make \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-clang \
    mingw-w64-x86_64-openssl

# Build Crypto++ from source for MinGW:
git clone --depth 1 https://github.com/weidai11/cryptopp.git
cd cryptopp
make -f GNUmakefile -j"$(nproc)"
make -f GNUmakefile install PREFIX=/c/cryptopp-mingw
```

---

### 5.2 Build & Test Verification Commands

| Action | Ubuntu Linux | Windows MSVC (VS Native Prompt) | Windows MinGW (MSYS2 / CMD) |
|---|---|---|---|
| **Build CLI** | `scripts/build_linux.sh gcc`<br>`scripts/build_linux.sh clang` | `scripts\build_windows.bat msvc` | `scripts\build_windows.bat gcc`<br>`scripts\build_windows.bat clang` |
| **Build Shared Lib** | `./gui/build_gui.sh` | `gui\build_gui.bat` | `cmake -S . -B build/windows-gcc-shared -DBUILD_SHARED_LIBS=ON` |
| **Run KAT** | `./build/ubuntu-gcc/aestool --kat kat/vectors.json` | `build\windows-msvc\Release\aestool.exe --kat kat\vectors.json` | `build\windows-gcc\aestool.exe --kat kat\vectors.json` |
| **Run CTest** | `(cd build/ubuntu-gcc && ctest --output-on-failure)` | `cd build\windows-msvc && ctest -C Release --output-on-failure` | `cd build\windows-gcc && ctest --output-on-failure` |
| **GUI Self-Test** | `python3 gui/aestool_gui.py --self-test` | `python gui\aestool_gui.py --self-test` | `python gui\aestool_gui.py --self-test` |
| **Run Benchmarks** | `scripts/run_benchmarks.sh gcc` | `scripts\bench_windows_controlled.bat` | `scripts\run_benchmarks.bat gcc` |
| **Plot Summaries** | `python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks`<br>`python3 scripts/plot_compare.py --dir output/ubuntu/benchmarks --out-dir output/ubuntu/benchmarks/graph` | `python scripts\summarize_bench.py --dir output\windows\benchmarks`<br>`python scripts\plot_compare.py --dir output\windows\benchmarks --out-dir output\windows\benchmarks\graph` | *N/A (Uses Python scripts on generated CSVs)* |
