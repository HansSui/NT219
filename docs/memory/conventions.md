# Conventions

Last updated: 2026-11-06

- C++17, CMake ≥ 3.16, out-of-source builds.
- Warnings: `-Wall -Wextra` (MSVC `/W4`). Release optimization in scripts.
- UTF-8 arguments; use `std::filesystem::u8path` for file paths on Windows.
- Binary-safe `--in`/`--out`; stdout hex for ciphertext, file output raw unless
  `--encode` overrides.
- CLI shape follows `docs/course-objectives.txt`: `tool <command> [--in|--text]
  [--out] [--encode hex|base64|raw] [--kat FILE] [--verbose]`.
- Crypto failures are fail-closed: validate first, write output only after
  success, and do not leak which check failed.
- Tests: vendored Catch2 `tests/catch.hpp`, one CTest per unit suite, KAT and CLI
  tests registered with `add_test`.
- Evidence: `output/<platform>/{test_results,benchmarks}`; scripts print the
  environment/compiler they used.
- Reuse `lab01/src/minijson.hpp` for JSON parsing rather than adding a library.
- Comments explain *why* only; mark deliberate ceilings with
  `shortcut: <limit>, <upgrade path>`.
