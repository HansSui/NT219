# Lab 1 - Missing / Lacking

**Audit date:** 2026-11-02
**Fixes applied:** 2026-11-02 (L1-03, L1-04, L1-05 partial, L1-09, L1-10)
**Sources:** `docs/lab1.txt`, `docs/lab requirement.md`, `docs/GradingSystem.txt`,
`docs/blueprint/labs/lab1.md`, `lab01/`
**Method:** rebuilt and ran Lab 1 on Ubuntu with GCC 15.2 and Clang 21.1.8;
reviewed the committed Windows MSVC/MinGW logs and scripts. Windows was not
re-run in this audit.

Status legend: **OK** = present and verified, **Partial** = present but
incomplete, **Missing** = not present, **Not verified** = no evidence here.

## Verdict

The Lab 1 implementation is substantially complete on both platforms. The
remaining gaps are submission packaging, the Windows KAT rerun, Ubuntu
negative-test evidence, and minor CLI polish, not a broken AES implementation.

Sidecar nonce reuse, automatic XTS tweaks, broken documentation links, and the
unsupported Clang benchmark claims found in the first pass are fixed. Ubuntu is
still weaker than Windows on negative-test evidence. Windows build, KAT,
benchmark, GUI, and non-ASCII-path logs are present.

## Verified state after fixes

| Check | Ubuntu | Windows |
|---|---|---|
| Build and `ctest` | Pass, 2/2 (GCC and Clang) | Pass, 2/2 (MSVC and MinGW logs) |
| CLI KAT | 20/20 pass (GCC and Clang) | 16/16 committed log; 20-vector rerun pending |
| Negative tests | 12 cases | 17 cases |
| Benchmark matrix | GCC 48 CSVs; Clang tests/KAT only | MSVC 48 CSVs |
| GUI self-test | Pass | Pass |
| Non-ASCII path | CLI/unit coverage exists | Dedicated log exists |

Ubuntu commands used:

```bash
cd lab01
scripts/build_linux.sh gcc
./build/ubuntu-gcc/aestool --kat kat/vectors.json
(cd build/ubuntu-gcc && ctest --output-on-failure)
```

## Gaps

| ID | Gap | Requirement | Ubuntu | Windows | Severity |
|---|---|---|---|---|---|
| L1-01 | Final report/archive not produced | C-13, C-14 | Partial | Partial | High (submission) |
| L1-02 | Windows/Linux binaries not packaged | C-13 | Missing | Missing | Medium (ambiguous) |
| L1-03 | Sidecar header was not checked for nonce reuse | C-06 | Fixed | Fixed (code); rerun pending | Closed |
| L1-04 | XTS auto-generated an IV when omitted | C-06 | Fixed | Fixed (code); rerun pending | Closed |
| L1-05 | AES-192/256 and XTS KAT coverage is thin | C-08 | Partial (20 vectors) | 16/16 committed; rerun pending | Medium |
| L1-06 | Ubuntu negative-test evidence is thinner than Windows | C-09 | Partial | OK | Medium |
| L1-07 | Nonce registry is per-directory and concurrency is untested | C-06 | Partial | Partial | Low/Medium |
| L1-08 | Uniform CLI is only partially implemented | C-03, C-04 | Fixed | Fixed (code); rerun pending | Closed |
| L1-09 | Deleted checklist files leave broken links | C-02 | Fixed | Fixed | Closed |
| L1-10 | Report claims Clang benchmark data that is not in the tree | C-11, C-14 | Fixed | N/A | Closed |

### L1-01 - Final report and archive are not produced

`lab01/REPORT.md` exists and is detailed, but:

- Student, student ID, and class fields are blank.
- No PDF/DOCX export exists.
- No single submission archive exists (source, binaries, report, self-grade,
  integrity statement).

Requirement: C-13 and C-14; Lab 1 rubric gives 20 points to report quality.

Fix: fill the three identity fields, export the report, and package the archive.
Do not duplicate the report into another format unless the instructor asks.

### L1-02 - Binaries are not in the submission

`lab01/build/` is not part of the submitted artifact. The course checklist says
to include Windows and Linux binaries; `GradingSystem.txt` also accepts clear
build instructions. This conflict should be resolved with the TA.

Fix: if binaries are required, add one Windows `aestool.exe` and one Linux
`aestool` to the archive, plus the build instructions already in the README.
If instructions are enough, state that explicitly in the report.

### L1-03 - Sidecar nonce reuse was not detected from the sidecar header

**Fixed.** `checkAndRecordNonce()` used to parse only an existing `AESL1`
container. With `--sidecar`, the ciphertext is raw and the IV lives in the JSON
sidecar, so a copied sidecar/output pair in a fresh directory bypassed the
direct check. The function now accepts the sidecar path, parses it before
writing, and rejects the same `(mode, iv)`. A malformed existing sidecar fails
closed. Both `tests/cli_test.sh` and `tests/cli_test.cmake` now copy a sidecar
to a new working directory and expect rejection.

Requirement: C-06 and Lab 1 section 1.5.

Remaining: rerun the updated CLI test on Windows, because only the Ubuntu
GCC/Clang runs were refreshed here.

### L1-04 - XTS got a random IV when `--iv` was omitted

**Fixed.** `runEncrypt()` used to generate a random tweak for XTS, although the
brief requires an explicit data-unit number. It now rejects XTS without
`--iv`/`--nonce`, and both CLI tests cover the failure. `lab01/README.md` lists
XTS as `required (--iv); never auto-generated`.

Requirement: C-06; Lab 1 section 1.5.

Remaining: rerun the updated CLI test on Windows.

### L1-05 - KAT coverage is thin for AES-192/256 and XTS

**Partially fixed.** `lab01/kat/vectors.json` now has 20 vectors. AES-192/256
CBC, CFB128, OFB, and CTR are full four-block NIST SP 800-38A vectors; ECB stays
a first-block smoke test. Ubuntu GCC/Clang pass 20/20. The committed Windows
logs still show the old 16-vector run, so Windows 20/20 evidence is pending.

Still missing: AES-256 GCM and CCM KATs, and a larger XTS matrix. The single
XTS-AES-128 vector is unchanged.

Requirement: C-08 and Lab 1 section 2.4.

Fix remaining: add AES-256 GCM/CCM and more XTS vectors, then rerun KAT on
Windows and replace the committed logs.

### L1-06 - Ubuntu negative evidence is thinner than Windows

Windows has 17 negative cases. Ubuntu has 12. Ubuntu is missing CLI evidence
for at least:

- CCM bad tag
- GCM wrong AAD
- mode/header mismatch
- AEAD mode without `--aead`
- AAD supplied to a non-AEAD mode
- ECB over 16 KiB with `--allow-ecb`
- nonce-lock contention

The CCM bad-tag path is covered by unit tests, but the per-case CLI evidence is
not. Requirement: C-09 and Lab 1 section 2.3.

Fix: run the same case list on Linux and regenerate
`lab01/output/ubuntu/neg_results/SUMMARY.md`. No new test framework needed.

### L1-07 - Nonce registry scope and concurrency are not fully evidenced

The registry is `.aestool_nonces` in the current working directory. It stores
`SHA-256(key) + IV` in plaintext and is not authenticated. A manual eight-process
same-nonce check on Ubuntu produced exactly one output and seven rejections, but
there is no committed repeatable stress test. The Windows check is described
below. The registry remains per-directory by design.

Requirement: C-06. This is a lab tool, so a per-directory registry is a
reasonable simplification if documented.

`shortcut: registry is per-working-directory and unauthenticated; upgrade to a
per-key state store or OS keystore if nonce reuse must be prevented across
directories.`

Fix: document the scope and add one concurrent test if the README keeps the
atomic-lock claim. The Windows section below includes a manual eight-process
check for the same directory and nonce; run it when refreshing Windows evidence.

### L1-08 - Uniform CLI is only partially implemented

**Fixed on Ubuntu.** The three gaps were:

- `--threads N` was accepted for `encrypt`/`decrypt` but only printed a note.
- `--encode base64` output could not be fed back through `--text` on decrypt.
- `--sidecar` on decrypt required a path even though the usage showed it as
  optional.

Changes:

- `encrypt`/`decrypt` now reject `--threads` with `--threads is only valid with
  bench`; `bench` keeps its worker-thread support.
- Added `--decode hex|base64|raw` for decrypt input, plus `aes::fromBase64()`.
  `--text` still auto-detects hex when `--decode` is absent.
- `--sidecar` without a path now defaults to `OUT.json` on encrypt and `IN.json`
  on decrypt (or `sidecar.json` with `--text`).
- CLI tests cover all three on Ubuntu and in the portable CMake test used by
  Windows.

Requirement: C-03/C-04.

Remaining: run the updated CMake/CLI tests on Windows; see the Windows section
below.

### L1-09 - Broken documentation links

**Fixed.** `lab01/README.md` and the root `README.md` referenced
`LAB1_WINDOWS_TODO.md` and `LAB1_LINUX_TODO.md`, which are deleted in the
working tree. The links are removed and the root layout now points at the
current `docs/` tree and `lab02/`. The deleted checklists remain deleted; their
useful content is covered by the lab READMEs and `scripts/`.

Requirement: C-02. Low impact.

### L1-10 - Report claims Clang benchmark data that is not in the tree

**Fixed.** `lab01/REPORT.md` used to say there were 144 CSVs: 96 Ubuntu
(GCC + Clang) and 48 Windows. The committed tree has 48 Ubuntu GCC CSVs and 48
Windows MSVC CSVs; there are no Clang benchmark CSVs. The report now states 96
CSVs, removes the Clang performance columns and observations, keeps Clang as a
correctness-only build, and says the 96,000 measured pairs come from 48 Ubuntu
GCC + 48 Windows MSVC runs. `lab01/BENCHMARKS.md` already documented the same
limit.

Clang `ctest` and KAT logs do exist, so Clang correctness remains evidenced;
only the Clang **performance** claim was unsupported.

Requirement: C-11 and C-14; report quality is 20 points.

Remaining: export the corrected report and fill the identity fields (L1-01).

## Per-environment actions

### Ubuntu

1. Re-run the 17-case negative suite and save the summary.
2. Export the corrected report to PDF/DOCX and fill student fields.
3. Package the archive or state that instructions are the deliverable.
4. Add AES-256 GCM/CCM and more XTS KAT vectors (L1-05).
5. Document the per-directory nonce registry and the `--threads`/base64 limits,
   or implement them (L1-07, L1-08).

### Windows

1. Re-run `aestool --kat kat/vectors.json` and replace the 16-vector logs with
   the 20/20 output.
2. Re-run `ctest` and the 17-case negative suite with the updated code.
3. Rebuild the MSVC/MinGW binaries if the archive includes binaries.
4. Keep the existing Windows CSVs; no benchmark rerun is needed.

### Windows security verification (after pulling these changes)

The updated portable CMake test covers the new security and UX paths, so start
with a normal rebuild and `ctest`:

```bat
cd lab01
scripts\build_windows.bat msvc
set "TOOL=%CD%\build\windows-msvc\Release\aestool.exe"
set "KEY=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
pushd build\windows-msvc
ctest -C Release --output-on-failure
popd
```

`cli_test.cmake` now checks: copied-sidecar nonce rejection, missing-XTS-IV
rejection, `--threads` rejection on `encrypt`, base64 `--decode` round-trip, and
the default `--sidecar` path.

Then refresh the committed Windows evidence:

```bat
scripts\verify_windows.bat
```

It rebuilds MSVC, expects `20 passed, 0 failed`, runs `ctest`, and reruns the
negative suite. If MinGW is installed, it also refreshes the MinGW logs; use
`set SKIP_MINGW=1` only when that toolchain is unavailable.

Manual security checks, if you need per-case console evidence:

```bat
rem 1. Copied sidecar must be rejected from its header in a fresh directory.
"%TOOL%" encrypt --mode gcm --aead --key-hex %KEY% --iv 00112233445566778899aabb ^
    --text first --out sidecar.bin --sidecar sidecar.json
mkdir sidecar-reuse
copy sidecar.json sidecar-reuse\
pushd sidecar-reuse
"%TOOL%" encrypt --mode gcm --aead --key-hex %KEY% --iv 00112233445566778899aabb ^
    --text second --out new.bin --sidecar sidecar.json
if not errorlevel 1 echo FAIL: sidecar reuse accepted
popd

rem 2. XTS without --iv must fail and write no output.
"%TOOL%" encrypt --mode xts --key-hex %KEY% --text data --out xts.bin
if not errorlevel 1 echo FAIL: XTS accepted missing IV

rem 3. --threads is bench-only.
"%TOOL%" encrypt --mode cbc --key-hex %KEY% --text data --threads 2 --out t.bin
if not errorlevel 1 echo FAIL: encrypt accepted --threads
```

Do not run the concurrency check until the normal sidecar/XTS tests pass:

```bat
rem 4. Same key+nonce from eight processes in one directory: exactly one output.
for /l %%i in (1,1,8) do start /b "" "%TOOL%" encrypt --mode ctr --key-hex %KEY% ^
    --iv 00112233445566778899aabbccddeeff --text race --out race_%%i.bin
timeout /t 2 >nul
dir /b race_*.bin
```

Expected: one `race_*.bin`, seven failures. This exercises the per-directory
`.aestool_nonces` registry and `.aestool_nonces.lock` on Windows. The registry is
still per-working-directory; that limitation is intentional and must stay
documented unless a central nonce store is added.

## Grade

Rubric from `docs/blueprint/labs/lab1.md`: 100 core points + 5 bonus.

| Criterion | Weight | Baseline | After fixes | Why |
|---|---:|---:|---:|---|
| Correctness and KATs | 25 | 23 | 24 | All modes pass; 20 vectors now on Ubuntu, Windows rerun pending |
| Security hygiene | 15 | 12 | 14 | Sidecar and XTS gaps closed; per-directory registry remains |
| UX and I/O design | 10 | 9 | 10 | `--threads`, base64 decode and optional sidecar path fixed |
| Performance methodology | 20 | 16 | 19 | 96 honest CSVs and controlled Windows/Linux runs; Clang claim removed |
| Cross-platform build | 10 | 10 | 10 | MSVC, MinGW GCC/Clang, Linux GCC/Clang verified |
| Report quality | 20 | 13 | 15 | False Clang data fixed; identity, PDF/archive still missing |
| **Core total** | **100** | **83** | **92** | |
| Bonus: library + GUI | +5 | +5 | +5 | Shared library + Tkinter GUI |
| **Total** | **105** | **88** | **97** | 97/100 after the bonus cap |

Baseline **88/100**, post-fix **97/100**. Report quality was intentionally left
unchanged in this pass. The remaining gap is submission work and Windows
evidence refresh, not the AES implementation. Do not claim 20/20 Windows KAT or
a final PDF until those artifacts exist.

## Applied fixes

- `lab01/src/main.cpp`: sidecar `(mode, iv)` check before writing; malformed
  sidecar fails closed; XTS requires an explicit IV.
- `lab01/src/main.cpp`: `--threads` is rejected outside `bench`; `--decode`
  supports hex/base64/raw decrypt input; `--sidecar` without a path defaults to
  `OUT.json`/`IN.json`.
- `lab01/src/aescore.hpp/.cpp`: added `fromBase64()` with validation.
- `lab01/tests/cli_test.sh`, `lab01/tests/cli_test.cmake`: sidecar-reuse,
  missing-XTS-IV, thread rejection, base64 decode and default-sidecar tests.
- `lab01/kat/vectors.json`: 20 vectors; full AES-192/256 CBC/CFB/OFB/CTR.
- `lab01/output/ubuntu/test_results/kat_gcc.txt`, `kat_clang.txt`: 20/20 logs.
- `lab01/REPORT.md`: removed unsupported Clang benchmark data; corrected CSV
  counts to 96; corrected KAT count and Windows rerun note; updated UX docs.
- `lab01/README.md`, `README.md`: XTS rule, KAT count, deleted-link cleanup,
  `--decode`/sidecar/threads behavior.
- `lab01/scripts/verify_windows.bat`: expects 20/20 after the Windows rerun.
