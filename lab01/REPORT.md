# Lab 1 Report - Symmetric Encryption with Crypto++

> Linux performance data, statistics and comparison plots are filled in.
> Windows builds and the Windows-vs-Linux comparison remain pending.
> Do not submit invented benchmark numbers; export this file to PDF/DOCX after
> the Windows measurements are added.

## 1. Objectives

- Implement AES-128/192/256 in ECB, CBC, OFB, CFB, CTR, XTS, CCM and GCM with
  Crypto++ as the only cryptographic library.
- Handle IV/nonce generation, persistence and length validation correctly.
- Prevent common misuse: ECB on large payloads, CTR/GCM/CCM nonce reuse and
  ignored AEAD tags.
- Validate against NIST SP 800-38A, NIST GCM and RFC 3610 CCM vectors.
- Measure throughput and latency for 1 KiB to 8 MiB payloads and report mean,
  median, standard deviation and a 95% confidence interval.
- Discuss mode-level and implementation-level security, including fail-closed
  behavior.

## 2. Implementation

### 2.1 Structure

`aescore` contains all cryptographic operations. `main.cpp` only parses the CLI,
reads/writes files, applies policy checks and formats output. The KAT runner and
benchmark call the same core functions as normal encryption/decryption, so a KAT
pass exercises the production code path.

### 2.2 Modes and padding

| Mode | Crypto++ type | Padding | Integrity |
|---|---|---|---|
| ECB | `ECB_Mode<AES>` | PKCS #7 | no |
| CBC | `CBC_Mode<AES>` | PKCS #7 | no |
| OFB | `OFB_Mode<AES>` | none | no |
| CFB | `CFB_Mode<AES>` | none | no |
| CTR | `CTR_Mode<AES>` | none | no |
| XTS | `XTS_Mode<AES>` | none (ciphertext stealing) | no |
| CCM | `CCM<AES, 8>` | n/a | 64-bit tag |
| GCM | `GCM<AES>` | n/a | 128-bit tag |

KAT vectors are block-aligned and use no padding. The core therefore has an
internal `pkcs7` switch used only by the KAT runner; the CLI always uses PKCS #7
for ECB/CBC.

### 2.3 IV and nonce lifecycle

| Mode | Length | Auto-generated | Stored |
|---|---|---|---|
| ECB | 0 | n/a | n/a |
| CBC/OFB/CFB/CTR | 16 | yes | header |
| XTS | 16 | yes | header |
| GCM | 12 | yes | header |
| CCM | 7..13 (default 12) | yes | header |

The IV is generated with `AutoSeededRandomPool`. A missing IV on encryption is
generated and written into the container header. A user-supplied IV is validated
against the mode before use. On decryption the header IV is used, with an
explicit `--iv`/`--nonce` override for negative testing.

### 2.4 Nonce reuse detection

For CTR, CCM and GCM the CLI computes `SHA-256(key)` and appends
`keyFingerprint nonceHex` to `.aestool_nonces` after a successful encryption.
Before encrypting, it scans that file for the same fingerprint+nonce pair and
refuses the operation if found. This also catches the same nonce used across
CTR, CCM and GCM. Only the fingerprint and public nonce are stored. The
registry is local to the working directory and is not locked; this is documented
as a limitation.

### 2.5 Fail-closed behavior

- Keys, IVs, tags and container headers are length-checked before any crypto call.
- AEAD decryption uses `AuthenticatedDecryptionFilter::THROW_EXCEPTION`; the
  plaintext buffer is only returned after `MessageEnd()` succeeds.
- Output files are written after successful decryption, so an authentication
  failure leaves no partial plaintext file.
- Unknown modes, odd hex, malformed headers, truncated containers and AAD on
  non-AEAD modes are rejected.
- Decryption requires `--mode` and rejects a mismatch with the container header,
  preventing a silent downgrade from an AEAD mode to a non-AEAD mode.

## 3. Correctness

### 3.1 Known-answer tests

Run:

```bash
./build_ubuntu_gcc/aestool --kat kat/vectors.json
```

Observed result:

```
PASS NIST SP 800-38A AES-128 ECB
PASS NIST SP 800-38A AES-128 CBC
PASS NIST SP 800-38A AES-128 CFB128
PASS NIST SP 800-38A AES-128 OFB
PASS NIST SP 800-38A AES-128 CTR
PASS NIST GCM AES-128 test case 3
PASS NIST GCM AES-128 test case 4 with AAD
PASS RFC 3610 CCM packet vector 1
PASS RFC 3610 CCM packet vector 2
9 passed, 0 failed
```

Each case is checked in both directions: encryption must equal the published
ciphertext/tag, then decryption must return the published plaintext.

### 3.2 Unit tests

```bash
cd build_ubuntu_gcc && ctest --output-on-failure
```

Coverage includes hex parsing, every mode, XTS partial final blocks, empty
plaintext, AEAD tag/AAD/key/IV tampering, invalid key/IV lengths, container
round-trip, malformed headers and both key-file formats. A CLI test also checks
round-trip, nonce-reuse rejection, the ECB size limit and short-IV rejection.

### 3.3 Negative tests

| Test | Command (abridged) | Expected | Observed |
|---|---|---|---|
| Wrong key, CBC | `decrypt --mode cbc --key key_wrong.bin` | garbage or padding failure | `invalid PKCS #7 block padding` |
| Wrong IV, CBC | `decrypt --mode cbc --iv iv_wrong.bin` | corrupted plaintext | corrupted output |
| Tampered GCM tag | flip a header tag byte | refusal | `authentication failed` |
| Short IV | `--iv short_iv.bin` (8 bytes) | rejection | `invalid IV/nonce length` |
| ECB over 16 KiB | `encrypt --mode ecb` without `--allow-ecb` | warning + refusal | warning + refusal |
| Malformed container | truncated file | rejection | `input is too short` |
| Odd hex | `--key-hex abc` | rejection | `odd number of hex digits` |

The raw capture files are under `output/ubuntu/neg_results/`.

## 4. Performance

### 4.1 Method

- Payload sizes: 1 KiB, 4 KiB, 16 KiB, 256 KiB, 1 MiB, 8 MiB.
- Warm-up: at least 1 second of encrypt+decrypt operations before measurement.
- Measured runs: 1000 per compiler/mode/size (48 CSVs per compiler, 96 total).
- Each run times one encryption and one decryption with
  `std::chrono::steady_clock`; the timed region is in memory and excludes file
  I/O, process startup and shell overhead.
- Statistics: mean, median, sample standard deviation and
  `CI95 = 1.96 * stddev / sqrt(n)`.
- Throughput: `size_in_MB / elapsed_seconds` (MB = 10^6 bytes).
- Host: Intel Core i5-1135G7, 8 logical CPUs, Linux 7.0.0-38-generic, GCC and
  Clang from the Ubuntu toolchain. The CPU exposes AES-NI, PCLMULQDQ, AVX2 and
  AVX-512F. The CPU governor was `powersave`, not `performance`; results
  therefore include frequency scaling.
- The machine was not isolated and the process was not CPU-pinned, so scheduler
  migration and background activity remain in the variance.
- Full statistics: `output/ubuntu/benchmarks/summary.csv`. Class and tag
  summaries: `summary_classes.csv`, `summary_tag_overhead.csv`.

### 4.2 Results

Representative rows from the full 96-CSV matrix. All values are microseconds for
latency and MB/s for throughput; `SD` and `CI95` are for encryption latency.

| Platform | Compiler | Mode | Size | Enc mean | Enc median | Enc SD | Enc CI95 | Dec mean | Dec SD | Enc MB/s | Dec MB/s |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Ubuntu | GCC | CTR | 1 KiB | 2.13 | 1.48 | 7.93 | 0.49 | 1.81 | 0.52 | 601 | 605 |
| Ubuntu | GCC | CTR | 16 KiB | 6.27 | 5.32 | 1.88 | 0.12 | 6.24 | 1.84 | 2770 | 2790 |
| Ubuntu | GCC | CTR | 1 MiB | 936.04 | 838.18 | 246.17 | 15.26 | 668.11 | 200.33 | 1168 | 1648 |
| Ubuntu | GCC | CTR | 8 MiB | 8880.36 | 8484.47 | 1499.83 | 92.96 | 6168.10 | 1220.45 | 967 | 1405 |
| Ubuntu | GCC | CBC | 16 KiB | 14.66 | 14.96 | 4.35 | 0.27 | 4.06 | 0.80 | 1135 | 4110 |
| Ubuntu | GCC | CBC | 1 MiB | 1780.36 | 1739.01 | 173.29 | 10.74 | 275.78 | 85.31 | 594 | 3996 |
| Ubuntu | GCC | CBC | 8 MiB | 15406.17 | 15281.16 | 559.02 | 34.65 | 3083.59 | 483.37 | 545 | 2776 |
| Ubuntu | GCC | GCM | 1 KiB | 1.97 | 1.93 | 0.29 | 0.02 | 2.54 | 0.37 | 525 | 408 |
| Ubuntu | GCC | GCM | 16 KiB | 7.76 | 6.97 | 2.96 | 0.18 | 8.28 | 4.78 | 2227 | 2105 |
| Ubuntu | GCC | GCM | 1 MiB | 1449.36 | 1390.22 | 218.53 | 13.54 | 795.57 | 121.73 | 736 | 1341 |
| Ubuntu | GCC | GCM | 8 MiB | 13505.28 | 13059.37 | 1624.78 | 100.70 | 7636.01 | 1196.81 | 629 | 1122 |
| Ubuntu | GCC | CCM | 16 KiB | 19.16 | 18.73 | 2.20 | 0.14 | 19.43 | 1.57 | 860 | 846 |
| Ubuntu | GCC | CCM | 1 MiB | 2114.03 | 2073.60 | 150.13 | 9.30 | 1536.70 | 96.21 | 498 | 685 |
| Ubuntu | GCC | CCM | 8 MiB | 20300.21 | 18380.52 | 3075.91 | 190.65 | 14551.26 | 2178.56 | 422 | 588 |
| Ubuntu | Clang | GCM | 16 KiB | 7.52 | 6.99 | 1.92 | 0.12 | 7.89 | 1.65 | 2246 | 2133 |
| Ubuntu | Clang | GCM | 1 MiB | 1449.32 | 1338.79 | 299.32 | 18.55 | 795.68 | 156.61 | 742 | 1350 |
| Ubuntu | Clang | GCM | 8 MiB | 12193.54 | 11886.46 | 991.68 | 61.47 | 6835.97 | 782.83 | 692 | 1241 |
| Ubuntu | Clang | CTR | 1 MiB | 1103.17 | 1010.39 | 288.91 | 17.91 | 776.39 | 207.95 | 1006 | 1431 |
| Ubuntu | Clang | CCM | 1 MiB | 2102.82 | 2039.13 | 198.63 | 12.31 | 1529.14 | 130.81 | 502 | 690 |

The full table (96 rows) is in `output/ubuntu/benchmarks/summary.csv`; it includes
every mode, size, compiler, mean, median, SD, CI95 and coefficient of variation.

Windows rows are intentionally absent because no Windows benchmark has been run
yet. See Section 4.7.

Generate the full matrix (8 modes x 6 sizes x 1000 runs):

```bash
scripts/run_benchmarks.sh gcc
scripts/run_benchmarks.sh clang
```

Generate the tables and plots from existing CSVs:

```bash
python3 scripts/summarize_bench.py --dir output/ubuntu/benchmarks
python3 scripts/plot_compare.py --dir output/ubuntu/benchmarks \
    --out-dir output/ubuntu/benchmarks/graph
```

### 4.3 Comparison tables and plots

The following artifacts are generated from the 96 Linux CSVs:

| Artifact | Content |
|---|---|
| `summary.csv` | mean, median, SD, CI95 and CV for every compiler/mode/size |
| `summary_tag_overhead.csv` | GCM/CCM latency and throughput relative to CTR |
| `summary_classes.csv` | stream, block, XTS, AEAD and non-AEAD aggregates |
| `comparison_throughput_by_mode_1MB.png` | GCC vs Clang, all modes, 1 MiB |
| `comparison_throughput_vs_size.png` | throughput from 1 KiB to 8 MiB |
| `comparison_latency_vs_size.png` | latency from 1 KiB to 8 MiB |
| `comparison_latency_box_16KB.png` | run distribution per mode at 16 KiB |
| `comparison_stream_vs_block.png` | ECB, CBC, stream modes and XTS |
| `comparison_aead_vs_nonaead.png` | AEAD vs non-AEAD throughput |
| `comparison_tag_overhead.png` | GCM/CCM overhead relative to CTR |

Class definitions: block = ECB + CBC, stream = OFB + CFB + CTR, AEAD = GCM +
CCM, non-AEAD = ECB + CBC + OFB + CFB + CTR + XTS. Class aggregates weight every
run equally, so they show the average behaviour of the modes in that class.

### 4.4 Stream vs block modes

GCC/Clang mean encryption throughput (MB/s):

| Size | ECB | CBC | Stream group (OFB/CFB/CTR) | XTS |
|---|---:|---:|---:|---:|
| 1 KiB | 1098 | 532 | 599 | 456 |
| 16 KiB | 4232 | 1096 | 1714 | 1155 |
| 1 MiB | 963 | 508 | 782 | 767 |
| 8 MiB | 820 | 521 | 720 | 696 |

At 16 KiB, the stream group reaches about 1714 MB/s encryption and 2065 MB/s
decryption, while CBC alone reaches 1096 MB/s and 4110 MB/s. At 1 MiB the stream
group reaches about 782 MB/s encryption, compared with 508 MB/s for CBC; at
8 MiB the values are 720 MB/s and 521 MB/s. CTR is the fastest stream mode
(3132 MB/s at 16 KiB, 1087 MB/s at 1 MiB, 1012 MB/s at 8 MiB), while OFB and CFB
are closer to CBC because their feedback chains limit parallelism.

The block-class average is inflated by ECB, which reaches 4232 MB/s at 16 KiB
and 963 MB/s at 1 MiB. ECB is fast but insecure; it should not be used as
evidence that block modes are generally faster. Comparing the secure block mode
(CBC) with the stream group, stream modes are faster for encryption, especially
at 1 MiB and 8 MiB, because they avoid padding and CBC's serial encryption
chain. CBC decryption is much faster than CBC encryption because blocks can be
decrypted in parallel once the IV and previous ciphertext blocks are known.

### 4.5 AEAD vs non-AEAD

AEAD is consistently slower than the non-AEAD group:

| Size | AEAD enc MB/s | Non-AEAD enc MB/s | AEAD dec MB/s | Non-AEAD dec MB/s |
|---|---:|---:|---:|---:|
| 1 KiB | 503 | 647 | 398 | 719 |
| 16 KiB | 1545 | 1938 | 1479 | 2631 |
| 1 MiB | 620 | 764 | 1016 | 2024 |
| 8 MiB | 550 | 699 | 897 | 1708 |

At 1 MiB, AEAD loses about 19% encryption throughput but about 50% decryption
throughput. GCM is faster than CCM in this build: at 1 MiB, GCM reaches 736 MB/s
encryption and 1341 MB/s decryption, while CCM reaches 498 MB/s and 685 MB/s.
GCM benefits from PCLMUL/GHASH acceleration; CCM's CBC-MAC is serial and its
Crypto++ implementation is slower here. AEAD decryption also has to verify the
tag before releasing plaintext, which adds work on top of the CTR core.

The non-AEAD group includes ECB, which inflates its average. If ECB is excluded,
the gap narrows: at 16 KiB the AEAD and non-AEAD averages become comparable,
while at 1 MiB and 8 MiB AEAD remains slower because authentication is real work,
not free.

### 4.6 Tag overhead (GCM/CCM)

`summary_tag_overhead.csv` subtracts CTR latency from GCM/CCM latency at the same
size and compiler. GCC values:

| Size | Mode | Enc overhead (us) | Enc latency | Dec overhead (us) | Dec latency | Enc throughput loss | Dec throughput loss |
|---|---:|---:|---:|---:|---:|---:|---:|
| 16 KiB | GCM | 1.48 | +23.6% | 2.03 | +32.6% | 19.6% | 24.6% |
| 16 KiB | CCM | 12.89 | +205.4% | 13.19 | +211.3% | 68.9% | 69.7% |
| 1 MiB | GCM | 513.32 | +54.8% | 127.47 | +19.1% | 37.0% | 18.6% |
| 1 MiB | CCM | 1177.99 | +125.8% | 868.60 | +130.0% | 57.3% | 58.5% |
| 8 MiB | GCM | 4624.92 | +52.1% | 1467.91 | +23.8% | 35.0% | 20.1% |
| 8 MiB | CCM | 11419.85 | +128.6% | 8383.15 | +135.9% | 56.4% | 58.1% |

GCM's tag overhead is roughly one third of throughput at large sizes; CCM's is
more than half. This is not just the 8-byte versus 16-byte tag: CCM's
authentication path is much slower in this build. At 1 KiB the absolute
overheads are below 2 us and are dominated by setup and timer noise, so the
percentage values at that size are not stable; a negative GCM encryption
overhead of -0.16 us was observed and should be read as zero within noise.

### 4.7 Windows vs Linux

**Status: not measured.** All 96 CSVs are Ubuntu only. There are no Windows
benchmark results, so no Windows-vs-Linux claim is made in this report. The
Windows build itself also remains unverified on this host.

To complete this comparison, run the same matrix on Windows with the same CPU,
payload sizes, 1000 runs, 1 s warm-up and Crypto++ version, using MSVC and MinGW
GCC/Clang. Record the Windows build number, power plan, CPU model, RAM, compiler
versions and Crypto++ build flags. Then compare `summary.csv` values for the same
compiler/mode/size. The expected differences are scheduler behaviour, timer
resolution, allocator implementation, code generation and power management, but
these are hypotheses until the Windows measurements exist.

### 4.8 OS scheduling and system-call overhead

The benchmark is in-memory: it calls the Crypto++ encrypt/decrypt API directly
and times only that region. It does **not** include `open`, `read`, `write`,
`fsync` or process-startup syscalls. `std::chrono::steady_clock` normally uses
`clock_gettime`, which is served through the vDSO on Linux and is not a real
syscall. Therefore these numbers measure library work, allocation, memory
traffic and scheduler interference, not file-I/O syscall overhead.

Small payloads are dominated by fixed per-call cost. At 1 KiB, mean encryption
latency is about 0.9-2.6 us depending on mode, and throughput is only
400-1100 MB/s. At 8 MiB, latency is about 8-20 ms and throughput is
422-1056 MB/s; the larger payload amortises setup but becomes memory-bound.

Scheduling noise is visible in the coefficient of variation at 1 MiB: CTR/GCC
has CV 0.263, GCM/GCC 0.151, CBC/GCC 0.097 and CCM/GCC 0.071. The median is
often below the mean (for example CTR/GCC 838 us median versus 936 us mean),
which indicates right-skewed outliers from frequency changes or scheduler
preemption. The host was running the `powersave` governor and was not CPU-pinned,
so these results include DVFS and migration effects. For a final comparison, set
the governor to `performance`, pin the process with `taskset`, keep the machine
idle, and report both mean and median. To measure syscall overhead specifically,
add a separate end-to-end CLI benchmark that includes file I/O; the current
`bench` command deliberately excludes it.

## 5. Security engineering discussion

### 5.1 Mode-level security

**ECB.** Identical plaintext blocks produce identical ciphertext blocks, so
structure and repetition leak. The tool warns and blocks payloads over 16 KiB
unless explicitly overridden. ECB must not be used for real messages.

**CBC.** A random IV hides repeated plaintexts, but CBC provides no integrity.
Bit-flipping in one ciphertext block corrupts the corresponding plaintext block
and flips predictable bits in the next. Padding errors can enable a padding
oracle if an attacker can distinguish them; therefore the CLI reports a generic
failure and never returns partial plaintext.

**OFB/CFB/CTR.** These turn AES into a stream cipher. Reusing an IV/nonce with
the same key reuses the keystream; XORing two ciphertexts then reveals the XOR of
the plaintexts. CTR is the clearest example of catastrophic nonce reuse, so the
tool rejects repeated key+nonce pairs. OFB/CFB have the same IV-reuse risk even
though the lab's mandatory registry applies to CTR/CCM/GCM.

**GCM.** GCM is CTR plus GHASH authentication. It requires a unique nonce for
every message under a key; nonce reuse breaks both confidentiality and
authentication. The 96-bit random nonce gives a low collision probability for a
bounded number of messages, but a counter-based nonce is preferable at scale.
GCM verifies the tag over ciphertext, AAD and lengths before releasing
plaintext.

**CCM.** CCM is CBC-MAC plus CTR. It also requires unique nonces and
pre-specified message/AAD lengths. The tool uses a 64-bit tag and 12-byte default
nonce (L=3, 16 MiB maximum message). The KAT path accepts the RFC 3610 13-byte
nonce.

**XTS.** XTS is a tweakable block mode for storage. It hides plaintext patterns
within a sector but provides no integrity: an attacker can still make controlled
changes. It is not a replacement for an AEAD mode. Crypto++ requires at least one
full 16-byte block, so inputs shorter than 16 bytes are rejected.

### 5.2 Implementation-level security

- RNG: `AutoSeededRandomPool` only.
- Key material: never written to the nonce registry; only SHA-256 fingerprints.
- IV storage: embedded in the ciphertext header, validated on read.
- Tag verification: decryption throws before output is written.
- Failure handling: fail closed; no plaintext on authentication failure.
- Key storage: raw key files are trusted input. File permissions and OS key
  stores are outside this tool's scope.
- Header: for AEAD, changes to IV/AAD/tag cause tag failure. Non-AEAD modes have
  no header integrity.

### 5.3 Threat model

In scope: a network or storage attacker who can read and modify ciphertext,
truncate files, or supply a different key/IV; malformed CLI input; accidental
nonce reuse in normal CLI use.

Out of scope: an attacker who can read the key file or process memory; side
channels; physical attacks; a malicious Crypto++ build; coordinated multi-process
nonce reuse (the registry is local and unlocked).

### 5.4 Misuse scenarios and mitigations

| Misuse | Consequence | Mitigation |
|---|---|---|
| ECB on structured data | pattern leakage | warning, 16 KiB block, `--allow-ecb` |
| Fixed IV in CBC/CTR | keystream/pattern leakage | random IV, stored in header |
| CTR/GCM/CCM nonce reuse | confidentiality and integrity loss | local nonce registry, rejection |
| Ignoring GCM tag | forged ciphertext accepted | `THROW_EXCEPTION`, no output on failure |
| Using XTS as AEAD | undetected modification | documented limitation |
| Truncated/malformed header | crashes or wrong mode | strict length/version/magic checks |

## 6. Cross-platform build

| Platform | Compiler | Status |
|---|---|---|
| Ubuntu | GCC 15.2 | build + ctest pass |
| Ubuntu | Clang 21.1.8 | build + ctest pass |
| Windows | MSVC 2022 | build script provided; not verified; no performance data |
| Windows | MinGW GCC | build script provided; not verified; no performance data |
| Windows | MinGW Clang | build script provided; not verified; no performance data |

Record compiler versions, Crypto++ version, CPU model, RAM and OS build in the
final report. This run used Crypto++ 8.9.0. Build commands are in `README.md`.

## 7. Lessons learned

- AEAD is not just "encrypt then append a tag"; the tag must be verified before
  any plaintext is exposed.
- Nonce management is a state problem, not a formatting problem. A CLI needs a
  registry or an unambiguous nonce source.
- Padding behavior must be explicit when comparing against NIST vectors: the
  published CBC/ECB vectors do not use PKCS #7.
- Benchmark results are dominated by different effects at different sizes;
  report the distribution, not only the mean.
- Cross-platform crypto code is mostly portable, but library discovery, DLL
  exports and file paths need per-platform handling.

## 8. Self-grade checklist

- [ ] Correctness and KATs (25): 9/9 KAT, unit tests, negative tests.
- [ ] Security hygiene (15): RNG, IV validation, nonce reuse rejection, ECB
      policy, fail-closed AEAD.
- [ ] UX and I/O design (10): uniform CLI, file/text input, hex/base64/raw,
      clear errors.
- [x] Performance methodology, Linux (20): 1000 runs, warm-up, mean/median/SD/
      CI95, class comparisons, tag overhead, scheduling/variance analysis.
- [ ] Windows vs Linux performance comparison: Windows measurements not yet run.
- [ ] Cross-platform build (10): CMake, GCC/Clang/MSVC scripts, ctest; Windows
      builds still need verification.
- [ ] Report quality (20): objectives, implementation, tests, security analysis,
      cross-platform comparison, lessons learned.
- [ ] Bonus (+5): `BUILD_SHARED_LIBS=ON` produces `aescore` shared library; no GUI
      is included.

## 9. Academic integrity

Statement of originality: *I/we wrote this implementation and report, and have
not copied another student's work or an external implementation.*

AI/tool assistance disclosure: *[describe any AI assistant or code generator
used, and what it was used for; state that the author reviewed and tested the
result.]*

External references: Crypto++ documentation, NIST SP 800-38A, NIST SP 800-38C,
NIST SP 800-38D, NIST GCM test vectors, RFC 3610, IEEE P1619/XTS.
