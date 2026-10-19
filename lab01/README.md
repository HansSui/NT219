# Lab 1 - Symmetric Encryption with Crypto++

`aestool` is a CLI for AES-128/192/256 in ECB, CBC, OFB, CFB, CTR, XTS, CCM and GCM
modes. It is intentionally small: one crypto core (`aescore`), one CLI, NIST/RFC
known-answer tests, negative tests and a benchmark mode that writes CSV.

## Repository layout

```
CMakeLists.txt
src/
  aescore.hpp/.cpp      crypto core and container format
  cryptopp_all.hpp      single Crypto++ include point
  minijson.hpp          tiny JSON reader for KAT files
  main.cpp              CLI: encrypt, decrypt, bench, --kat
tests/
  catch.hpp             Catch2 v2.13.10 single header (vendored)
  test_core.cpp         unit tests
kat/vectors.json        NIST SP 800-38A, NIST GCM and RFC 3610 CCM vectors
scripts/
  build_linux.sh        GCC or Clang build + ctest
  build_windows.bat     MSVC, MinGW GCC or MinGW Clang build + ctest
  run_benchmarks.sh     all modes x all sizes x 1000 runs (Linux)
  run_benchmarks.bat    all modes x all sizes x 1000 runs (Windows)
  plot_compare.py       combined comparison charts from all benchmark CSVs
  summarize_bench.py    mean/median/SD/CI95 tables and class/tag summaries
  plot_bench.py         throughput/latency plots from one benchmark CSV
BENCHMARKS.md           batch benchmark instructions and output naming
```

## Dependencies

| Dependency | Version used | Notes |
|---|---|---|
| CMake | 3.16 or newer | tested with 4.2.3 |
| C++ compiler | C++17 | GCC 15.2, Clang 20, MSVC 2022 |
| Crypto++ | 8.9.0 | required; no OpenSSL/libsodium |
| Catch2 | 2.13.10 | vendored header, no download at build time |
| Python | 3.8+ | optional, for `plot_bench.py`, `plot_compare.py` and `summarize_bench.py` |
| matplotlib | any recent | optional, only for plots |

## Install

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake libcryptopp-dev python3-matplotlib
```

If Crypto++ is built in a custom prefix, configure with
`-DCRYPTOPP_ROOT=/path/to/cryptopp`.

### Windows

Use vcpkg:

```bat
vcpkg install cryptopp:x64-windows
cmake -S . -B build_windows_msvc -G "Visual Studio 17 2022" -A x64 ^
      -DCRYPTOPP_ROOT=C:\path\to\vcpkg\installed\x64-windows
```

Or MSYS2 MinGW64:

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-cryptopp
```

## Build

Out-of-source build (the required form):

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Compiler-specific helper scripts:

```bash
scripts/build_linux.sh gcc      # build_ubuntu_gcc/aestool
scripts/build_linux.sh clang    # build_ubuntu_clang/aestool
```

```bat
scripts\build_windows.bat msvc
scripts\build_windows.bat gcc
scripts\build_windows.bat clang
```

Shared-library (bonus) build:

```bash
cmake -S . -B build_ubuntu_gcc_shared -DBUILD_SHARED_LIBS=ON
cmake --build build_ubuntu_gcc_shared
```

## CLI

```
aestool encrypt --mode MODE [--in FILE | --text STRING] [--out FILE]
                (--key FILE | --key-hex HEX) [--iv FILE|HEX] [--nonce FILE|HEX]
                [--aead] [--aad FILE | --aad-text STRING]
                [--encode hex|base64|raw] [--allow-ecb] [--verbose]

aestool decrypt --mode MODE [--in FILE | --text HEX] [--out FILE]
                (--key FILE | --key-hex HEX) [--iv FILE|HEX] [--nonce FILE|HEX]
                [--aead] [--aad FILE | --aad-text STRING]
                [--encode hex|base64|raw] [--allow-ecb] [--verbose]

aestool bench --mode MODE (--key FILE | --key-hex HEX) [--size BYTES]
              [--runs N] [--warmup SECONDS] [--out CSV] [--allow-ecb]

aestool --kat vectors.json
```

Modes: `ecb cbc ofb cfb ctr xts ccm gcm`.

### Examples

Encrypt and decrypt a file with AES-256-GCM:

```bash
aestool encrypt --mode gcm --aead --key key.bin --in msg.txt --out ct.bin
aestool decrypt --mode gcm --aead --key key.bin --in ct.bin --out msg.txt
```

AES-256-CBC with an explicit IV, plaintext supplied as UTF-8 text:

```bash
aestool encrypt --mode cbc --key key.bin --iv 000102030405060708090a0b0c0d0e0f \
                --text "hello" --encode base64
```

AES-256-CTR with an explicit nonce, AAD and hex output:

```bash
aestool encrypt --mode ctr \
        --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f \
        --nonce 00112233445566778899aabbccddeeff --text "hello" --encode hex
```

Known-answer tests:

```bash
./build_ubuntu_gcc/aestool --kat kat/vectors.json
# PASS ... for every vector, then: 9 passed, 0 failed
```

Benchmark AES-256-GCM at 16 KiB and write one CSV row per run:

```bash
./build_ubuntu_gcc/aestool bench --mode gcm --key key.bin --size 16384 \
    --runs 1000 --out output/ubuntu/benchmarks/bench_gcc_gcm_16KB.csv
```

Run the full matrix (8 modes x 6 sizes x 1000 runs):

```bash
scripts/run_benchmarks.sh gcc
scripts/run_benchmarks.sh clang
# Windows: scripts\run_benchmarks.bat msvc
```

See `BENCHMARKS.md` for output names, overrides and a quicker smoke run.

Generate the combined comparison charts and statistics tables:

```bash
python3 scripts/plot_compare.py --dir output/ubuntu/benchmarks \
    --out-dir output/ubuntu/benchmarks/graph
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
```

This writes throughput-by-mode, throughput-vs-size, latency-vs-size, latency
box, stream-vs-block, AEAD-vs-non-AEAD and tag-overhead plots, plus
`summary.csv`, `summary_tag_overhead.csv` and `summary_classes.csv`.
`scripts/plot_bench.py` is still available for one CSV at a time.

### Key files

`--key FILE` accepts either raw binary key bytes or text beginning with `hex:`,
for example `hex:2b7e151628aed2a6abf7158809cf4f3c`. `--key-hex` takes the hex
string directly. Valid sizes are 16/24/32 bytes for AES and 32/64 bytes for XTS
(two AES-128 or two AES-256 keys).

### IV / nonce handling

| Mode | IV/nonce | Default when omitted |
|---|---|---|
| ECB | none | n/a |
| CBC, OFB, CFB, CTR | 16 bytes | random and stored in the header |
| XTS | 16-byte tweak | random and stored in the header |
| GCM | 12 bytes | random and stored in the header |
| CCM | 7..13 bytes (default 12) | random 12 bytes and stored in the header |

An invalid length is rejected. `--iv` and `--nonce` accept a file path or a hex
string; the file form is tried first. On decryption the stored IV is used unless
`--iv`/`--nonce` overrides it (useful for negative tests).

## Ciphertext container

AES encryption writes a self-describing binary container. Decryption needs the key,
the mode and the file; the mode must match the container header, which prevents a
header downgrade to a non-AEAD mode. No sidecar is required.

```
offset  size  field
0       5     magic "AESL1"
5       1     version = 1
6       1     mode id (0=ECB ... 7=GCM)
7       1     IV length
8       1     tag length
9       4     AAD length, little-endian
13      N     IV/nonce
13+N    T     authentication tag (AEAD only)
...     A     AAD
...     -     ciphertext
```

The header is parsed before decryption. A truncated or inconsistent header fails
closed. With `--out`, the raw container is written by default; `--encode hex|base64`
writes an encoded representation. On screen, ciphertext defaults to hex and
plaintext defaults to raw bytes.

## Security design

- Randomness comes from `CryptoPP::AutoSeededRandomPool`; no `rand()` or
  `std::random` is used.
- ECB always prints a warning and refuses payloads over 16 KiB unless
  `--allow-ecb` is supplied.
- CTR, CCM and GCM reject a key+nonce pair that this tool has already used.
  The registry is `.aestool_nonces` in the current working directory and stores
  only a SHA-256 key fingerprint and the nonce, never the key. It is a local,
  unlocked file: it catches accidental CLI reuse, not coordinated multi-process
  misuse.
- GCM and CCM are authenticated. The tag is verified during decryption and the
  plaintext is released only after verification succeeds. A bad tag, wrong key,
  wrong IV or wrong AAD throws and writes no output.
- Malformed keys, wrong IV lengths, malformed headers, odd hex and unknown modes
  are errors. Decryption failures do not produce partial plaintext files.
- GCM uses a 96-bit random nonce. XTS provides confidentiality only, not
  integrity.

### Limitations

- Non-AEAD modes provide no integrity. A tampered CBC/CTR/OFB/CFB/XTS ciphertext
  either decrypts to corrupted bytes or, for CBC, may fail padding validation.
- XTS provides no authentication and is intended for storage encryption, not
  for messages. It requires at least 16 bytes of input.
- The default CCM nonce is 12 bytes, i.e. L=3, which limits one message to 16 MiB.
  The core accepts 7..13-byte CCM nonces, so the KAT vectors with 13-byte nonces
  work, but the CLI auto-generates 12-byte nonces.
- The nonce reuse registry is per working directory and has no file locking.
- `--threads` is accepted for CLI compatibility but currently ignored.
- There is no password-based key derivation or key storage/rotation. Keys are
  read from files or hex.
- The container header is not independently authenticated. For AEAD, changing
  the IV, AAD or tag makes tag verification fail; for non-AEAD modes there is no
  such protection.
- CBC encryption uses PKCS #7 padding. A decryption oracle that exposes padding
  errors would be vulnerable to a padding-oracle attack; this CLI reports only a
  generic failure.

## Tests

```bash
cd build_ubuntu_gcc && ctest --output-on-failure
```

The unit suite covers hex handling, all modes, XTS ciphertext stealing, AEAD
round-trips, tag/AAD/key/IV tampering, invalid lengths, container parsing and key
file formats. A small CLI test checks round-trip, nonce-reuse rejection, the ECB
size limit and short-IV rejection. KAT coverage: NIST SP 800-38A
ECB/CBC/CFB/OFB/CTR, NIST GCM test cases 3 and 4, RFC 3610 CCM vectors 1 and 2.

KAT JSON shape (all crypto fields are hex, `aad` and `tag` may be empty):

```json
{"vectors": [{"name": "...", "mode": "gcm", "key": "...", "iv": "...",
              "plaintext": "...", "ciphertext": "...", "aad": "...", "tag": "..."}]}
```

## Report and deliverables

`REPORT.md` is the working report skeleton: objectives, implementation, test
results, security analysis, benchmark method and self-grade checklist. Fill the
measured tables after running the benchmark commands, then export to PDF/DOCX.
