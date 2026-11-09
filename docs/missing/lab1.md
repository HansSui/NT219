# Lab 1 - Missing / Lacking

**Audit date:** 2026-11-02 (Ubuntu rerun and rubric audit 2026-11-05)
**Fixes applied:** 2026-11-02 (L1-03, L1-04, L1-05, L1-07, L1-08, L1-09,
L1-10; packaging helper for L1-01/L1-02); 2026-11-05 Ubuntu KAT/ctest/nonce
logs refreshed
**Sources:** `docs/lab1.txt`, `docs/lab requirement.md`, `docs/GradingSystem.txt`,
`docs/blueprint/labs/lab1.md`, `lab01/`
**Method:** rebuilt and ran Lab 1 on Windows with MSVC 19.51, MinGW GCC 16.2.0
and MinGW Clang 22.1.8; reran KAT, `ctest`, GUI and nonce-race evidence. Ubuntu
was rerun on 2026-11-05 with GCC 15.2 and Clang 21.1.8 (KAT 29/29, `ctest` 2/2,
nonce race); the refreshed logs are committed.

Status legend: **OK** = present and verified, **Partial** = present but
incomplete, **Missing** = not present, **Not verified** = no evidence here.

## Verdict

The Lab 1 implementation is complete on the code, KAT and evidence side, on
Ubuntu and Windows. **Rubric audit 2026-11-05:** every technical row is full.
The core is **99/100**, with the only withheld point in report quality because
the report header identity and PDF/DOCX are not produced. If the grader treats
identity/PDF as packaging rather than report quality, the rubric content is
100/100; do not claim that until the package exists. The remaining gaps are
submission identity/PDF/archive and optional binaries (L1-01, L1-02); neither
indicates a broken AES implementation.

Sidecar nonce reuse, automatic XTS tweaks, broken documentation links, thin
AES-256/XTS KAT coverage, the untested nonce lock, and the unsupported Clang
benchmark claims are fixed. The 19-case negative suite is now in `ctest` on both
platforms, and the Ubuntu and Windows KAT logs show 29/29 vectors.

## Verified state after fixes

| Check | Ubuntu | Windows |
|---|---|---|
| Build and `ctest` | Pass, 2/2 (GCC and Clang logs) | Pass, 2/2 (MSVC and MinGW logs) |
| CLI KAT | 29/29 (GCC 15.2, Clang 21.1.8; refreshed 2026-11-05) | 29/29 (MSVC, MinGW GCC, MinGW Clang) |
| Negative tests | 19 cases in `ctest` (ctest log refreshed); legacy per-case logs | 17 committed cases; 19 cases now in `ctest` |
| Benchmark matrix | GCC 48 CSVs; Clang tests/KAT only | MSVC 48 CSVs |
| GUI self-test | Pass | Pass |
| Non-ASCII path | CLI/unit coverage exists | Dedicated log exists |
| Nonce race, 8 processes | Manual check documented | `nonce_race.txt`: 1 success, 7 rejections |

Ubuntu commands to rerun the evidence:

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
| L1-03 | Sidecar header was not checked for nonce reuse | C-06 | Fixed | Fixed; rerun done | Closed |
| L1-04 | XTS auto-generated an IV when omitted | C-06 | Fixed | Fixed; rerun done | Closed |
| L1-05 | AES-192/256 and XTS KAT coverage is thin | C-08 | 29/29 (GCC, Clang) | 29/29 | Closed |
| L1-06 | Ubuntu negative-test evidence is thinner than Windows | C-09 | OK (19 cases in ctest, log refreshed) | OK | Closed |
| L1-07 | Nonce registry is per-directory and concurrency is untested | C-06 | Partial | Fixed (repeatable race test) | Closed |
| L1-08 | Uniform CLI is only partially implemented | C-03, C-04 | Fixed | Fixed; rerun done | Closed |
| L1-09 | Deleted checklist files leave broken links | C-02 | Fixed | Fixed | Closed |
| L1-10 | Report claims Clang benchmark data that is not in the tree | C-11, C-14 | Fixed | N/A | Closed |

### L1-01 - Final report and archive are not produced

`lab01/REPORT.md` exists and is detailed, but:

- Student, student ID, and class fields are blank.
- No PDF/DOCX export exists.
- No single submission archive exists (source, binaries, report, self-grade,
  integrity statement).

Requirement: C-13 and C-14; Lab 1 rubric gives 20 points to report quality.
One point is withheld here for the blank identity fields and absent PDF; the
rest of the report-quality evidence (objectives, design, tests, performance,
security, limitations, citations, self-grade, integrity statement) is present.

Fix: fill the three identity fields, export the report, and package the archive.
`lab01/scripts/package_lab1.py` now stages the source, report, evidence and
optional binaries, fills the report header from `--student`, `--student-id` and
`--class-name`, writes `SUBMISSION.md` with the integrity statement, and creates
`output/package/lab01-submission.zip`. PDF/DOCX export still needs the student's
details and a converter; do not duplicate the report into another format unless
the instructor asks.

### L1-02 - Binaries are not in the submission

`lab01/build/` is not part of the submitted artifact. The course checklist says
to include Windows and Linux binaries; `GradingSystem.txt` also accepts clear
build instructions. This conflict should be resolved with the TA.

Fix: if binaries are required, pass one Windows `aestool.exe` and one Linux
`aestool` to `scripts/package_lab1.py` (`--windows-bin`, `--linux-bin`); the
script copies them to `bin/` and keeps the README build instructions. If
instructions are enough, run it with `--skip-binaries` and say so in the report.

### L1-03 - Sidecar nonce reuse was not detected from the sidecar header

**Fixed.** `checkAndRecordNonce()` used to parse only an existing `AESL1`
container. With `--sidecar`, the ciphertext is raw and the IV lives in the JSON
sidecar, so a copied sidecar/output pair in a fresh directory bypassed the
direct check. The function now accepts the sidecar path, parses it before
writing, and rejects the same `(mode, iv)`. A malformed existing sidecar fails
closed. Both `tests/cli_test.sh` and `tests/cli_test.cmake` now copy a sidecar
to a new working directory and expect rejection.

Requirement: C-06 and Lab 1 section 1.5.

Remaining: none; the updated CMake/CLI test passes on MSVC and MinGW and the
Windows logs are refreshed.

### L1-04 - XTS got a random IV when `--iv` was omitted

**Fixed.** `runEncrypt()` used to generate a random tweak for XTS, although the
brief requires an explicit data-unit number. It now rejects XTS without
`--iv`/`--nonce`, and both CLI tests cover the failure. `lab01/README.md` lists
XTS as `required (--iv); never auto-generated`.

Requirement: C-06; Lab 1 section 1.5.

Remaining: none; the updated CMake/CLI test rejects missing XTS IVs on MSVC and
MinGW.

### L1-05 - KAT coverage is thin for AES-192/256 and XTS

**Fixed.** `lab01/kat/vectors.json` now has 29 vectors. AES-192/256 CBC, CFB128,
OFB, and CTR are full four-block NIST SP 800-38A vectors; ECB stays a
first-block smoke test. The expansion adds:

- NIST CAVP XTSGenAES128 COUNT 1 (16 bytes) and COUNT 101 (32 bytes);
- NIST CAVP XTSGenAES256 COUNT 1 (32 bytes) and COUNT 101 (48 bytes);
- NIST CAVP CCM AES-256 VTT Count 20 and 21 (13-byte nonce, 8-byte tag);
- NIST CAVP GCM AES-256 with no AAD, with 16-byte AAD, and with a two-block
  payload and 20-byte AAD.

Windows MSVC, MinGW GCC and MinGW Clang pass 29/29 and the refreshed logs are
committed. Ubuntu GCC 15.2 and Clang 21.1.8 were rerun on 2026-11-05 and pass
29/29; the refreshed logs are committed.

The two CAVP CCM vectors share a key/nonce because they belong to one CAVP test
group. They are algorithm KATs, not CLI nonce-reuse tests, so that does not
weaken the nonce policy.

Requirement: C-08 and Lab 1 section 2.4.

### L1-06 - Ubuntu negative evidence is thinner than Windows

Windows had 17 negative cases and Ubuntu had 12. `tests/cli_test.sh` and
`tests/cli_test.cmake` now run 19 shared negative/edge cases, adding CLI
coverage for:

- CCM bad tag
- GCM wrong AAD
- mode/header mismatch
- AEAD mode without `--aead`
- AAD supplied to a non-AEAD mode
- ECB over 16 KiB with `--allow-ecb`
- nonce-lock contention

`ctest` runs these on Linux and Windows, so a plain Linux `ctest` now covers the
missing paths; the Ubuntu GCC/Clang `ctest` logs were refreshed on 2026-11-05.
The per-case logs in `lab01/output/ubuntu/neg_results/` are still the older
12-case set; regenerate them only if the submission needs per-case console
evidence. Requirement: C-09 and Lab 1 section 2.3.

### L1-07 - Nonce registry scope and concurrency are not fully evidenced

**Fixed.** The registry is `.aestool_nonces` in the current working directory. It
stores `SHA-256(key) + IV` in plaintext and is not authenticated. The README
documents that scope and the atomic `.aestool_nonces.lock` behaviour.

`lab01/tests/nonce_race_test.py` is a repeatable cross-platform stress test: it
starts eight processes with the same key and CTR nonce in one directory and
requires exactly one success, seven rejections and one output file. On Windows
MSVC the result is committed as
`lab01/output/windows/test_results/nonce_race.txt`, and
`scripts/verify_windows.bat` runs it as step 7. It is not registered with
`ctest` because it needs Python and depends on host scheduling.

Requirement: C-06. This is a lab tool, so a per-directory registry is a
reasonable simplification if documented.

`shortcut: registry is per-working-directory and unauthenticated; upgrade to a
per-key state store or OS keystore if nonce reuse must be prevented across
directories.`

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

Remaining: none; the updated CMake/CLI tests pass on MSVC and MinGW, and the
Windows logs are refreshed.

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

1. Re-run `aestool --kat kat/vectors.json` and `ctest` to refresh the 29-vector
   and 19-case evidence.
2. Optionally regenerate the per-case logs in `output/ubuntu/neg_results/` to
   match the shared 19-case list.
3. Export the corrected report to PDF/DOCX and fill student fields.
4. Run `scripts/package_lab1.py` with a Linux binary if the archive includes
   binaries.

### Windows

1. Done: KAT 29/29, `ctest` 2/2, GUI self-test and nonce-race logs refreshed on
   2026-11-02 with MSVC, MinGW GCC and MinGW Clang.
2. Rebuild the MSVC/MinGW binaries if the archive includes binaries.
3. Keep the existing Windows CSVs; no benchmark rerun is needed.

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
rejection, `--threads` rejection on `encrypt`, base64 `--decode` round-trip, the
default `--sidecar` path, CCM bad tag, GCM wrong AAD, missing `--aead`, AAD on a
non-AEAD mode, mode/header mismatch, and ECB > 16 KiB with `--allow-ecb`.

Then refresh the committed Windows evidence:

```bat
scripts\verify_windows.bat
```

It rebuilds MSVC, expects `29 passed, 0 failed`, runs `ctest`, reruns the
negative suite and the 8-process nonce race, then refreshes the GUI and
non-ASCII-path logs. If MinGW is installed, it also refreshes the MinGW logs; use
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

Do not run the concurrency check until the normal sidecar/XTS tests pass. Use the
repeatable script rather than the old `start /b` loop:

```bat
python tests\nonce_race_test.py --tool "%TOOL%" --processes 8
```

Expected: `successes=1 failures=7 outputs=1` and `PASS: nonce lock is exclusive
across processes`. This exercises the per-directory `.aestool_nonces` registry
and `.aestool_nonces.lock` on Windows. The registry is still
per-working-directory; that limitation is intentional and must stay documented
unless a central nonce store is added.

## Implementation log - 2026-11-02

This pass implemented the Lab 1 fixes above and refreshed the Windows evidence.
The commands below were run from `E:\NT219\lab01`; output is in
`lab01/output/windows/`.

| Step | Command | Result |
|---|---|---|
| MSVC build | `scripts\build_windows.bat msvc` | Release build succeeded |
| MSVC KAT | `build\windows-msvc\Release\aestool.exe --kat kat\vectors.json` | `29 passed, 0 failed` |
| MSVC `ctest` | `ctest -C Release --output-on-failure` in `build\windows-msvc` | `2/2` tests passed |
| MinGW GCC | `scripts\build_windows.bat gcc` with `D:\mys64\mingw64\bin` first on `PATH` | `29 passed, 0 failed`; `ctest 2/2` |
| MinGW Clang | `scripts\build_windows.bat clang` with `D:\mys64\mingw64\bin` first on `PATH` | `29 passed, 0 failed`; `ctest 2/2` |
| Nonce race | `python tests\nonce_race_test.py --tool build\windows-msvc\Release\aestool.exe --processes 8` | `successes=1 failures=7 outputs=1`; PASS |
| Full refresh | `scripts\verify_windows.bat` (with `SKIP_MINGW=1` for the MSVC-only check) | KAT, `ctest`, negative suite, GUI and non-ASCII logs refreshed |
| Packaging smoke test | `python scripts\package_lab1.py --student "..." --student-id ... --class-name ... --windows-bin build\windows-msvc\Release\aestool.exe` | Staged source, report, evidence and binary; `output/package/lab01-submission.zip` about 5.7 MiB |

Not run in this pass:

- Ubuntu KAT/`ctest`: rerun on 2026-11-05 on Ubuntu 26.04 with GCC 15.2 and
  Clang 21.1.8; both pass 29/29 KAT and `ctest` 2/2. The nonce-race log was
  also refreshed (`1 success, 7 rejections`).
- PDF/DOCX export and the final named archive: both need the student's name,
  MSSV and class, and a PDF converter is not installed here.

Regrade after this log: **99/100 core plus +5 bonus** (capped at 100). The
2026-11-05 Ubuntu rerun closes the KAT/ctest evidence gap, so correctness is
25/25; report quality stays 19/20 because the identity fields and PDF/DOCX are
still absent. L1-01 and L1-02 remain **Partial** because the final
identity-filled archive has not been produced. The AES/KAT/security/UX/
performance items are closed.

## Grade

Rubric from `docs/blueprint/labs/lab1.md`: 100 core points + 5 bonus.

| Criterion | Weight | Baseline | After fixes | Why |
|---|---:|---:|---:|---|
| Correctness and KATs | 25 | 23 | 25 | 29/29 KAT on Windows MSVC/MinGW and Ubuntu GCC/Clang; unit + mutation + 19-case CLI |
| Security hygiene | 15 | 12 | 15 | Sidecar/XTS gaps closed; atomic nonce lock now stress-tested; per-directory scope documented |
| UX and I/O design | 10 | 9 | 10 | `--threads`, base64 decode and optional sidecar path fixed |
| Performance methodology | 20 | 16 | 20 | 96 honest CSVs and controlled Windows/Linux runs; Clang claim removed |
| Cross-platform build | 10 | 10 | 10 | MSVC, MinGW GCC/Clang, Linux GCC/Clang verified |
| Report quality | 20 | 13 | 19 | False Clang data fixed, KAT/test evidence updated; identity and PDF still need the student's details |
| **Core total** | **100** | **83** | **99** | |
| Bonus: library + GUI | +5 | +5 | +5 | Shared library + Tkinter GUI |
| **Total** | **105** | **88** | **104** | 100/100 after the bonus cap |

Baseline **88/100**, post-fix **99/100** core plus **+5 bonus** (capped at 100).
The Ubuntu 29/29 KAT and 19-case `ctest` logs now exist. The remaining gap is
submission work (identity fields, optional PDF export, archive), not the AES
implementation. Do not claim a final PDF until it exists.

## Regrade 2026-11-05 (report excluded)

Ubuntu KAT/`ctest`/nonce logs were refreshed on 2026-11-05. Report quality is
excluded from this regrade, so the core is out of 80.

| Criterion | Max | Score | Reason |
|---|---:|---:|---|
| Correctness and KATs | 25 | 25 | 29/29 on Windows MSVC/MinGW and Ubuntu GCC/Clang; unit + mutation + 19-case CLI |
| Security hygiene | 15 | 15 | IV/nonce validation, sidecar reuse check, ECB guard, atomic per-directory nonce lock, fail-closed AEAD |
| UX and I/O design | 10 | 10 | uniform CLI, hex/base64/raw, optional sidecar, UTF-8, useful errors |
| Performance methodology | 20 | 20 | 96 CSVs, controlled Windows/Linux runs, mean/median/SD/CI95, plots and interpretation |
| Cross-platform build | 10 | 10 | MSVC, MinGW GCC/Clang, Linux GCC/Clang verified |
| Report quality | 20 | excluded | — |
| **Core (report excluded)** | **80** | **80** | |
| Bonus: library + GUI | +5 | +5 | shared library + Tkinter GUI |
| **Total** | **85** | **85** | report excluded |

With the report counted at 19/20, the core is **99/100**: correctness is
25/25 after the 2026-11-05 Ubuntu KAT/ctest refresh, and report quality keeps a
1-point deduction for the blank identity/PDF. The exclusion above only removes
the 20-point report row.

## Rubric maximization audit 2026-11-05

The rubric evidence is complete on every technical row. The only non-maxed row
is report quality, and its remaining point is submission packaging, not AES
behaviour.

| Criterion | Max | Score | Reason |
|---|---:|---:|---|
| Correctness and KATs | 25 | 25 | 29/29 on Windows MSVC/MinGW and Ubuntu GCC/Clang; 19-case CLI in `ctest`; unit/mutation tests |
| Security hygiene | 15 | 15 | ECB guard, per-mode IV validation, sidecar/header reuse check, atomic nonce lock, fail-closed AEAD |
| UX and I/O design | 10 | 10 | Uniform CLI, UTF-8, binary-safe files, hex/base64/raw, optional sidecar, useful errors |
| Performance methodology | 20 | 20 | Six required sizes, 1,000 runs, mean/median/SD/CI95, mode and OS comparisons, plots, syscall/I-O analysis |
| Cross-platform build | 10 | 10 | MSVC, MinGW GCC/Clang, Linux GCC/Clang build and test |
| Report quality | 20 | 19 | Content complete; 1 point withheld for blank identity and no PDF/DOCX |
| **Core** | **100** | **99** | |
| Bonus: library + GUI | +5 | +5 | Shared library + Tkinter GUI through the C ABI |
| **Total** | **105** | **104 -> 100** | Bonus cap at 100 |

No code or KAT gaps remain. `lab01/REPORT.md` was corrected from its stale
24/98 self-grade to 25/99. The per-case Ubuntu negative logs are still the old
12-case set, but `ctest` runs the shared 19-case suite on both platforms, so
this is documentation cleanup rather than a rubric deduction. The final
submission still needs identity, PDF/DOCX, an archive and (if the TA requires
them) binaries.

## Applied fixes

- `lab01/src/main.cpp`: sidecar `(mode, iv)` check before writing; malformed
  sidecar fails closed; XTS requires an explicit IV.
- `lab01/src/main.cpp`: `--threads` is rejected outside `bench`; `--decode`
  supports hex/base64/raw decrypt input; `--sidecar` without a path defaults to
  `OUT.json`/`IN.json`.
- `lab01/src/aescore.hpp/.cpp`: added `fromBase64()` with validation.
- `lab01/tests/cli_test.sh`, `lab01/tests/cli_test.cmake`: 19 shared negative
  cases including CCM bad tag, GCM wrong AAD, missing `--aead`, AAD on a
  non-AEAD mode, mode/header mismatch, and ECB > 16 KiB with `--allow-ecb`.
- `lab01/tests/nonce_race_test.py`: repeatable 8-process nonce-lock stress test.
- `lab01/kat/vectors.json`: 29 vectors; adds NIST CAVP XTSGen AES-128/256,
  AES-256 GCM (with/without AAD) and AES-256 CCM (8-byte tag).
- `lab01/output/windows/test_results/kat_msvc.txt`, `kat_mingw_gcc.txt`,
  `kat_mingw_clang.txt`: refreshed 29/29 logs.
- `lab01/output/windows/test_results/nonce_race.txt`: 1 success, 7 rejections.
- `lab01/scripts/package_lab1.py`: stages source/evidence/binaries, fills the
  report identity fields, writes `SUBMISSION.md`, creates the submission zip.
  The evidence copy now ignores `output/package` so the staging tree cannot
  copy itself recursively.
- `lab01/REPORT.md`: 29-vector KAT table and Windows log, expanded coverage
  notes, 19-case negative suite, concurrency evidence, updated self-grade.
- `lab01/README.md`: KAT count, 19-case test description, nonce-race command,
  refreshed verification table, packaging instructions.
- `lab01/scripts/verify_windows.bat`: expects `29 passed, 0 failed` and runs the
  nonce-race step.

## Regrade 2026-11-09 (Ubuntu recheck)

Re-ran Ubuntu 26.04 / GCC 15.2 after the Lab 2/3 plot work: build passed,
`--kat` reported `29 passed, 0 failed`, and `ctest` passed `2/2`. No code or
evidence regressed.

The grouped-bar cross-platform chart
(`output/compare/graph/cross_throughput_by_mode_1MB.png`) remains the visual
standard: one bar per case, one bar per platform, value labels, clear title
and a host caveat. Lab 2 and Lab 3 now use the same grouped-bar style with 95%
CI whiskers.

Core remains **99/100**. The only withheld point is report identity/PDF/archive
(L1-01); L1-02 (optional binaries) is unchanged. Windows was not rerun in this
pass, so the committed 2026-11-02 MSVC/MinGW logs remain the Windows evidence.

Packaging update: the report identity is now filled (Nguyễn Công Tuấn /
25522012 / NT219.R11.ANTT) and
`lab01/output/package/lab01-submission.zip` was generated (5.5 MiB, 255 files,
no binaries). The archive is a generated artifact and stays uncommitted. L1-01
is therefore reduced to the missing PDF/DOCX export; L1-02 remains optional.

Not checked: Windows rerun, PDF/DOCX export.
