# aestool GUI (bonus)

Minimal cross-platform GUI for the Lab 1 crypto core, written in Python with
Tkinter. It does **not** reimplement any cryptography: every encrypt/decrypt,
IV generation and container parse goes through the compiled shared library
(`aescore.dll` on Windows, `libaescore.so` on Linux) using the C ABI declared in
`src/aesgui.h`. This satisfies the bonus requirement that the GUI calls the
compiled library rather than the CLI.

```
gui/aestool_gui.py   Tkinter window + ctypes binding
gui/build_gui.bat    Windows shared-library build (MSVC)
gui/build_gui.sh     Linux/macOS shared-library build
gui/run_gui.bat      Windows launcher
gui/run_gui.sh       Linux/macOS launcher
src/aesgui.h         Stable C ABI
src/aesgui.cpp       C ABI -> aescore C++ implementation
```

## Build the shared library

The GUI needs `BUILD_SHARED_LIBS=ON`; the normal static build keeps working.

### Windows

```bat
cd E:\NT219\lab01
gui\build_gui.bat
```

The script prefers the CMake bundled with Visual Studio, detects the installed
generator, and looks for Crypto++ in this order:

1. `CRYPTOPP_INCLUDE_DIR` + `CRYPTOPP_LIBRARY`
2. `CRYPTOPP_ROOT`
3. `D:\cryptolibrary` (this repository's hand-built checkout)

Override any of them before running:

```bat
set CRYPTOPP_ROOT=C:\vcpkg\installed\x64-windows
gui\build_gui.bat
```

Expected output: `build_gui\Release\aescore.dll` (Visual Studio generator) or
`build_gui\aescore.dll` (Ninja).

### Linux

```bash
sudo apt update
sudo apt install -y build-essential cmake libcryptopp-dev python3-tk

cd lab01
./gui/build_gui.sh
```

`python3-tk` is only needed to display the window; `--self-test` works without
it as long as `import tkinter` succeeds. Expected output: `build_gui/libaescore.so`.

If you link a self-built static Crypto++ into the shared library, build Crypto++
with `-fPIC`; distribution packages such as `libcryptopp-dev` already are.

## Run

```bat
gui\run_gui.bat
```

```bash
./gui/run_gui.sh
```

Or call the script directly:

```bash
python gui/aestool_gui.py
python3 gui/aestool_gui.py
```

Library discovery order: `--library PATH`, `AESGUI_LIBRARY`, then the usual
`build_gui`, `build_ubuntu_gcc_shared`, `build_windows_msvc`,
`build_ubuntu_gcc`, `build_ubuntu_clang` and `build` directories.

### Headless self-test

```bash
python3 gui/aestool_gui.py --self-test
```

The self-test exercises all eight modes through the shared library, verifies
round-trips, checks the record header, and confirms that a mode mismatch and a
wrong key are rejected. It does not open a window.

## Features

- All modes: ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM
- AES-128/192/256 keys (XTS uses 32/64-byte keys), entered as hex or loaded from
  a raw/`hex:` key file
- Random key and IV generation via Crypto++ (`AutoSeededRandomPool`); blank IV
  means "generate inside the library and store in the container"
- The IV field is cleared after each encryption, so a reused nonce cannot come
  from a stale form value; the IV actually used stays in the container and the
  status bar
- Optional AAD for CCM/GCM
- UTF-8 text or binary file input
- AESL1 container output as hex or base64; "Save output" writes raw bytes
- Decryption accepts an AESL1 container from a file, hex text or base64 text
- ECB warning/confirmation above 16 KiB and a permanent XTS confidentiality note
- Errors are shown in a dialog and in the status bar; failed AEAD decryption
  never produces plaintext

## C ABI

| Function | Purpose |
|---|---|
| `aesgui_encrypt` | Encrypt and return an AESL1 container (allocates a buffer) |
| `aesgui_decrypt` | Parse and decrypt an AESL1 container |
| `aesgui_free` | Free a buffer returned by the library |
| `aesgui_last_error` | Last error message on the calling thread |
| `aesgui_random_hex` | Generate `n` random bytes as hex |
| `aesgui_record_info` | Read mode, IV, tag length and AAD length from a record |
| `aesgui_mode_sizes` | Default IV length, tag length and AEAD flag for a mode |

Buffers are returned as `unsigned char*` plus a length; they may contain NUL
bytes. The caller must free them with `aesgui_free`. Error strings are
thread-local and valid until the next call on the same thread.

## Security notes and limitations

- Keys live in the Python process and in the Tk entry widget; the GUI does not
  zeroize them and does not provide a key store. This matches the CLI, not a
  production key-management design.
- The GUI decrypts only the self-describing AESL1 container. It is not a generic
  "raw ciphertext + separate tag" editor.
- Decryption always uses the IV/nonce and AAD stored in the AESL1 header; the IV
  and AAD entries are encryption parameters only. This prevents a stale value
  from an earlier operation from breaking decryption.
- The GUI does not call the CLI, so the CLI's local nonce-reuse registry is not
  involved. Random nonces come from the library; do not reuse an IV manually.
- Windows was built and self-tested with MSVC 19.51 x64; Linux was built and
  self-tested with GCC 15.2 (`libaescore.so`). Logs are under
  `output/windows/test_results/` and `output/ubuntu/test_results/`.
- Tkinter is a platform widget set, so the window decoration differs between
  Windows and Linux; the layout and behaviour are the same.
