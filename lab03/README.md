# Lab 3 — RSA-OAEP and hybrid encryption (`rsatool`)

Implements RSA-OAEP with SHA-256/MGF1-SHA-256, optional OAEP labels, and an
automatic AES-256-GCM hybrid envelope for messages above the RSA plaintext
limit. Keys are written as PEM, DER and a metadata JSON file.

**Library decision:** this lab uses **OpenSSL 4.0.2** for all primitives and key
formats. The build rejects any other version. OpenSSL CLI 4.0.2 is used only as
an independent cross-check; it is never called by `rsatool` at runtime. Rejected
alternative: Crypto++ 8.9.0, because the installed package has no PEM helpers and
would force hand-written PEM/ASN.1 plumbing. See `docs/memory/decisions.md`.

## Dependencies

| Dependency | Version used | Notes |
|---|---|---|
| CMake | ≥ 3.16 (tested 4.2.3) | same as Labs 1–2 |
| C++ compiler | C++17 (GCC 15.2, Clang 21.1.8, MSVC 19.51, MinGW GCC 16.2) | |
| OpenSSL | **4.0.2 (required)** | `OPENSSL_ROOT_DIR` / `D:\cryptolibrary` |
| Catch2 | 2.13.10 | vendored at `tests/catch.hpp` |
| Python | 3.8+ | benchmark summary/plots (`matplotlib` for plots) |

Ubuntu:

```bash
sudo apt install -y build-essential cmake python3-matplotlib
# OpenSSL 4.0.2 must be available separately; set OPENSSL_ROOT_DIR to its prefix.
```

The Linux build script auto-detects `$HOME/CryptoLib/lib/openssl/gcc` when it
contains 4.0.2. Otherwise pass it explicitly:

```bash
OPENSSL_ROOT_DIR=/path/to/openssl-4.0.2 scripts/build_linux.sh gcc
```

Windows (MSVC): install "Desktop development with C++" and OpenSSL **4.0.2**,
then set `OPENSSL_ROOT_DIR` (the scripts look in `D:\cryptolibrary`). MinGW:
install MinGW OpenSSL 4.0.2 and pass its prefix.

## Build and test

```bash
cd lab03
scripts/build_linux.sh gcc
# or: scripts/build_linux.sh clang
# use a specific 4.0.2 prefix if auto-detection misses it:
# OPENSSL_ROOT_DIR=/path/to/openssl-4.0.2 scripts/build_linux.sh gcc
```

```bat
cd lab03
scripts\build_windows.bat msvc
rem scripts\build_windows.bat gcc
```

`ctest` is run by both scripts:

```bash
cd build/ubuntu-gcc
ctest --output-on-failure
```

## CLI

```text
rsatool keygen  --bits 3072 --pub pub.pem --priv priv.pem
rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin --label optional
rsatool decrypt --in ct.bin --priv priv.pem --out msg.bin --label optional
rsatool bench   [--runs-rsa N] [--runs-small N] [--runs-large N]
                [--threads N] [--warmup SECONDS] [--out CSV]
rsatool --kat kat/oaep_vectors.json
```

`keygen` also writes `<pub>.der`, `<priv>.der` and `<priv>.json` unless
`--pub-der`, `--priv-der` and `--metadata` override those paths. The metadata
records `creation_time`, `modulus_bits`, `hash` and the four key paths.

Keys may be PEM or DER. Public keys are SubjectPublicKeyInfo; private keys are
PKCS#8 PEM and PKCS#1 DER. Keys below 3072 bits are rejected at load time.
Key generation `--bits` accepts 3072 and 4096 (and any larger value); 4096 is
for the performance comparison.

### Encoding

- File output defaults to raw binary; stdout defaults to hex.
- `--encode hex|base64|raw` overrides both.
- Decrypt encoded input with `--decode hex|base64|raw`.
- Odd-length hex, bad base64, and wrong padding fail closed.

### Direct vs hybrid

Direct OAEP is used when `mLen <= k - 2*hLen - 2`:

| Modulus | `k` | Max direct plaintext |
|---|---:|---:|
| 3072 | 384 | 318 bytes |
| 4096 | 512 | 446 bytes |

Above the limit the CLI switches automatically to AES-256-GCM with a random
256-bit key and 96-bit IV. The AES key is wrapped with RSA-OAEP using the same
optional label. No flag is needed for the switch.

For direct ciphertext there is no header, so the `--label` used at encryption
must be supplied again at decryption. For a hybrid envelope the label is stored
in the authenticated header; an explicitly supplied `--label` must match it.

## Envelope format

```text
RSAENV1\n
{"version":1,"mode":"RSA-OAEP-AES-GCM","rsa_modulus":3072,"hash":"SHA-256",
 "label":"...","wrapped_key":"<base64>","iv":"<base64>",
 "payload_len":N,"tag":"<base64>"}\n
<raw AES-256-GCM ciphertext>
```

The header is one line of canonical JSON (the example wraps for display only).
`RSAENV1` makes the format self-describing. The canonical header **without the
`tag` field** is the GCM AAD, so changing any header value (mode, modulus, hash,
label, wrapped key, IV, payload length) makes tag verification fail. The tag is
verified before any plaintext is written.

## Validation and failure behavior

All of these fail closed and write no output file:

- altered RSA ciphertext, altered AES-GCM ciphertext, altered tag;
- wrong private key, wrong OAEP label, tampered envelope header;
- malformed ciphertext, unsupported key size, bad hex/base64 encoding;
- truncated envelope, bad version/mode/hash, inconsistent `payload_len`.

The `decrypt` command reports only `error: decryption failed`, so an attacker
cannot learn which check failed. `encrypt` and `keygen` report their input
errors because they do not process attacker-controlled ciphertext.

## Tests and KAT

`ctest` runs five tests: OAEP unit tests, hybrid unit tests, negative/edge
tests, the JSON KAT runner and the CLI test. The KAT file contains two
independent RSA-OAEP-SHA256 vectors generated with OpenSSL `pkeyutl` (empty
label and non-empty label); `tests/cli_test.sh` additionally cross-checks PEM
format and OAEP interop with the OpenSSL CLI when it is installed.

```bash
./build/ubuntu-gcc/rsatool --kat kat/oaep_vectors.json
```

## Benchmarks

```bash
cd lab03
PIN_CPU=0-3 THREADS=4 RUNS_RSA=1000 RUNS_SMALL=1000 RUNS_LARGE=100 \
  WARMUP=1 scripts/run_benchmarks.sh gcc
```

The script defaults to the same values: four CPU-pinned worker threads, 1000
runs for RSA and for AES/hybrid 1 KiB and 1 MiB, and 100 runs for the 100 MiB
cases. The matrix covers RSA 3072/4096,
AES-256-GCM and hybrid encryption/decryption. The command writes per-run CSV
rows; the Python helper writes mean, median, standard deviation, 95% CI and
three plots. Latency is per operation under four-way concurrency; aggregate
throughput is four times the per-op MB/s. See `BENCHMARKS.md` for the table and
method.

## Known limitations

- The whole input is held in memory; there is no streaming envelope writer.
- Private keys are written unencrypted PEM/DER (`0600` is not enforced by the
  tool). This is test-key handling only.
- Direct RSA provides no forward secrecy; hybrid mode still uses long-term RSA
  for the key wrap. Production TLS should use ECDHE/ML-KEM.
- OpenSSL is the only implementation; there is no second crypto library for a
  byte-for-byte oracle. OpenSSL CLI checks cover formats and OAEP interop.
- Benchmarks are single-host, pinned to CPUs 0–3 and run with four worker
  threads; the CSV and environment table are the evidence.

## References

- RFC 8017 (PKCS #1 v2.2): RSAES-OAEP.
- NIST SP 800-38D: AES-GCM.
- NIST FIPS 197 and SP 800-57 for key sizes and AES.
- OpenSSL 4.0.2 EVP/PKEY documentation and CLI `pkeyutl`.
