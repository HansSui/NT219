# Lab 4 — Hashing, PKI and practical attacks (`hashtool`)

`hashtool` is a C++17 CLI for SHA-2, SHA-3 and SHAKE hashing, X.509 field
extraction and one-link signature verification, plus a SHA-256 length-extension
demo. Hash primitives come from OpenSSL EVP; no hash is reimplemented except the
small SHA-256 used to demonstrate state continuation.

**Library decision:** OpenSSL ≥ 3.0 is required (tested with the course 4.0.2
build and Ubuntu's 3.5.5). Rejected alternative: Crypto++, which would force
hand-written PEM/ASN.1 plumbing for the certificate task.

## Dependencies

| Dependency | Version used | Notes |
|---|---|---|
| CMake | ≥ 3.16 (tested 4.2.3) | |
| C++ compiler | C++17 (GCC 15.2, Clang 21.1.8) | MSVC/MinGW scripts included but not run here |
| OpenSSL | ≥ 3.0 (Ubuntu evidence 4.0.2) | `OPENSSL_ROOT_DIR` selects a non-system install |
| Catch2 | 2.13.10 | vendored at `tests/catch.hpp` |
| Python | 3.8+ | benchmark summary/plots (`matplotlib` for plots) |
| HashClash | static release v1.2b | only for the collision demos |

Ubuntu:

```bash
sudo apt install -y build-essential cmake python3-matplotlib
# The course OpenSSL 4.0.2 prefix:
export OPENSSL_ROOT_DIR="$HOME/CryptoLib/lib/openssl/gcc"
```

Windows: install "Desktop development with C++" and OpenSSL 3.x/4.x; the build
script looks in `D:\cryptolibrary`. Run from an x64 Native Tools prompt for MSVC,
or with MinGW on `PATH`:

```bat
cd lab04
scripts\build_windows.bat msvc
rem scripts\build_windows.bat gcc
```

## Build and test

```bash
cd lab04
OPENSSL_ROOT_DIR="$HOME/CryptoLib/lib/openssl/gcc" scripts/build_linux.sh gcc
# or: scripts/build_linux.sh clang
```

Both scripts configure `build/ubuntu-<compiler>`, build, and run `ctest`. Manual
run:

```bash
cd build/ubuntu-gcc
ctest --output-on-failure
```

The suite has five tests: `hashes`, `x509`, `lenext`, the 31-vector KAT, and a
CLI/negative test (`tests/cli_test.sh`, `tests/cli_test.cmake` on Windows).

## CLI

```text
hashtool --algo sha256 --in file.bin
hashtool --algo sha512 --in large.iso --stream
hashtool --algo shake128 --outlen 128 --in file.bin
hashtool --algo shake256 --outlen 64 --in file.bin

hashtool cert --in cert.pem [--issuer issuer.pem | --issuer pubkey.pem]
              [--require-verified] [--out report.txt]

hashtool lenext --mac HEX --key-len N (--in FILE | --text STR)
                (--append FILE | --append-text STR)
                [--out forged.bin] [--encode hex|base64|raw]

hashtool bench [--algos LIST] [--sizes MIB[,MIB...]] [--io stream,mmap]
               [--runs N] [--warmup SECONDS] [--out CSV] [--work-dir DIR]

hashtool --kat kat/hash_vectors.json
hashtool help
```

Supported algorithms:

| Family | Names | Output |
|---|---|---|
| SHA-2 | `sha224` `sha256` `sha384` `sha512` | fixed |
| SHA-3 | `sha3-224` `sha3-256` `sha3-384` `sha3-512` | fixed |
| SHAKE | `shake128` `shake256` | `--outlen` bits, multiple of 8, 1…8,388,608 |

Names are case-insensitive and accept `_` for `-`. `md5` and `sha1` are
deliberately rejected. `--outlen` on a fixed-size hash is an error; SHAKE
without `--outlen` is an error.

Input is binary-safe through `--in`; `--text` is for UTF-8 literals. Exactly one
is required. File hashing is always streamed in 64 KiB chunks, so multi-GB files
do not fit in memory; `--stream` is accepted as the explicit form. Benchmarks
adds an optional `mmap` path.

Output: stdout is hex, file output is raw binary; `--encode hex|base64|raw`
overrides either. A digest written with `--out digest.hex --encode hex` is
interchangeable with the console form.

## X.509 behaviour

`cert` extracts subject, issuer, SPKI algorithm and parameters (RSA modulus
bits/exponent or EC curve), signature algorithm, validity, key usage and SANs.
It also prints the inner TBS signature algorithm and an
`algorithm_consistency` result.

- Without `--issuer`: structural parsing and algorithm consistency only. The
  report says `UNVERIFIED` and `NOT TRUSTED`; the command exits 0 so inspection
  still works. Unknown signature OIDs and RSA/EC/EdDSA key-type mismatches are
  rejected. MD5/SHA-1 signature OIDs are parsed for inspection (the collision
  demo needs this), not policy-approved.
- With `--issuer` (PEM/DER certificate or public key): the issuer name must
  match when a certificate is supplied, and `X509_verify` must accept the
  signature. Any failure exits non-zero and prints nothing else.
- `--require-verified` fails closed unless an issuer key was supplied and the
  signature verified.
- Chain building, root trust, revocation, hostname matching and expiry policy
  are **not** evaluated. A successful verify means one link only, and the
  report says so.

## Demos

| Directory | Contents |
|---|---|
| `demos/md5_collision/` | two valid PNGs with identical pixels and identical MD5, generated with `md5_fastcoll` |
| `demos/length_extension/` | original/forged message, both MACs, padding diagram, mitigation analysis |
| `demos/chosen_prefix_cert/` | sandbox fallback: two MD5/RSA certificates sharing one MD5 signature; **identical-prefix, not full chosen-prefix** |
| `demos/tls/` | nginx/Apache hardenings, trust-chain explanation, evidence collector |

Each has a `run_demo.sh` except the TLS directory, which needs a host you own.
`collect_evidence.sh` writes the TLS 1.2/1.3 logs, HTTP headers and certificate
chain; it does not modify the server.

## Benchmarks

```bash
scripts/run_benchmarks.sh gcc
```

Defaults: `sha256,sha512,sha3-256,sha3-512`, sizes `1,100,1024` MiB, `stream` and
`mmap`, 1 s warm-up, and 1000/100/30 runs by size. Outputs land in
`output/ubuntu/benchmarks/`: raw CSV, grouped summary (mean, median, SD, 95% CI)
and four plots. `BENCHMARKS.md` records the measured table and analysis. For an
interactive view of the same CSV (table, ranking, plots), open
`analysis/benchmark_analysis.ipynb`; it reuses `scripts/plot_bench.py`.

## Limitations

- No actual Windows build/test or TLS deployment evidence is committed here;
  the scripts and configuration snippets are provided for the host owner to run.
- X.509 trust is deliberately one-link only (see above).
- The chosen-prefix +10 bonus is not claimed. The committed demo is an
  identical-prefix MD5 certificate collision; a real chosen-prefix attack costs
  about 2^39 MD5 work and was not run on this CPU host.
- The length-extension tool handles SHA-256 only; MD5/SHA-1 variants are out of
  scope.
- `bench` writes its scratch files under the system temp directory (or
  `--work-dir`); a 1 GiB case needs that much free space.

## Evidence

- `output/ubuntu/test_results/` — CTest, KAT, CLI and environment logs.
- `output/ubuntu/benchmarks/` — CSV, summary and plots.
- `output/windows/` — populated only by `scripts/build_windows.bat` on Windows.
- `REPORT.md` — Vietnamese lab report; `BENCHMARKS.md` — measurements and
  analysis.
