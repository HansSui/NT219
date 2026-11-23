# Backlog

Last updated: 2026-11-19

- Labs 1–3: no course-level PDF/DOCX report or single all-lab submission
  archive; the course wants all six labs. Lab 1 has a per-lab archive without
  the PDF and binaries.
- Lab 2: rerun Windows CTest/cross-validation after the OpenSSL switch and
  update `environment.txt`; the current logs say Crypto++. Optional: rerun the
  noisy Windows OpenSSL points (1 MiB both directions, 100 MiB encrypt, 1 GiB
  decrypt).
- Lab 3: PDF/archive; refresh the MinGW GCC test log after the manual-OAEP and
  CTR+HMAC test additions; optional MinGW Clang rerun; optional CTR+HMAC
  benchmark and authentication-failure timing study (bonus is 8/10).
- Lab 1: archive exists but is generated/uncommitted and has no binaries;
  Ubuntu per-case negative logs are the older 12-case set (ctest has 19).
- Lab 4: implement the real TLS deployment (needs a domain/host the student
  owns) and capture TLS 1.2/1.3 logs, headers, chain and a screenshot. Run the
  Windows MSVC/MinGW build + CTest + benchmark on the Windows host. Full
  chosen-prefix +10 is not attempted (identical-prefix fallback only).
- Labs 5–6: not started (see `docs/blueprint/labs/`).
- Cross-platform evidence for Labs 4–6 needs a Windows host/VM (blueprint A-06).
