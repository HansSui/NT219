# Architecture

Last updated: 2026-11-19

## Repo

```
NT219/
  docs/                  briefs, blueprint, audit notes, memory
  lab01/                 AES modes CLI (Crypto++)
  lab02/                 pure C++ AES core + CTR CLI
  lab03/                 RSA-OAEP + hybrid AES-GCM CLI (OpenSSL 4.0.x)
  lab04/                 hashing, X.509 and attack demos CLI (OpenSSL ≥ 3.0)
```

Each lab is standalone: no cross-lab library, no shared build target. This is
deliberate (see [decisions](decisions.md#2026-11-02-standalone-labs)) so the
grader can build any lab alone and a bug cannot spread across submissions.

## Lab shape

```
labNN/
  CMakeLists.txt         build + tests + install
  README.md              deps, build, CLI, format, limitations
  BENCHMARKS.md          method and matrix
  REPORT.md              lab report section
  src/                   core library + CLI
  tests/                 Catch2 unit tests, KAT, CLI test
  kat/                   JSON vectors
  scripts/               build, benchmark, summarize, plot
  output/                curated logs, CSVs, plots
```

Dependency direction inside a lab: `main.cpp` → core library → crypto library.
Tests link the same core library, never duplicate crypto logic. The CLI owns
file I/O and argument parsing; the core owns crypto and format validation.

## Lab 3 modules

| Module | Responsibility |
|---|---|
| `rsa` (`src/rsa.*`) | RSA keygen, OAEP-SHA256 encrypt/decrypt, label, size checks |
| `keyio` (inside `rsa.*`) | PEM/DER encode/load, metadata JSON |
| `hybrid` (`src/hybrid.*`) | AES-256-GCM, envelope encrypt/decrypt |
| `envelope` (inside `hybrid.*`) | versioned JSON header serialize/parse, AAD binding |
| `util` (`src/util.hpp`) | hex/base64, JSON escape, file I/O |
| `main` | CLI, encoding, `--kat`, `bench` |

The envelope is `RSAENV1\n` + one-line canonical JSON header + raw AES-GCM
ciphertext. The header (minus the tag) is the GCM AAD, so any field tamper fails
authentication. Direct RSA-OAEP ciphertext has no header and is exactly one
modulus long.

## Lab 4 modules

| Module | Responsibility |
|---|---|
| `hashcore` (`src/hash_core.*`) | EVP SHA-2/SHA-3/SHAKE, name/outlen validation, stream/mmap, KAT |
| `sha256ext` (`src/sha256_ext.*`) | minimal SHA-256 and length-extension state continuation |
| `x509` (`src/x509_inspect.*`) | PEM/DER parse, field extraction, one-link signature verify |
| `io` (`src/io.hpp`) | read-only mmap (`MapViewOfFile` / `mmap`) |
| `util` / `minijson` | hex/base64/file helpers, KAT JSON parser |
| `main` | CLI: `hash`, `cert`, `lenext`, `bench`, `--kat` |

`hashtool` never builds a certificate chain: `cert --issuer` verifies one link
and reports chain/root trust as not evaluated. The SHA-256 implementation is
not the hash backend; it exists only so `lenext` can continue a published MAC
state. Demos are offline files under `demos/` and are never called by the tool.
