# Cross-platform CLI smoke test for Lab 2
if(NOT DEFINED TOOL OR NOT EXISTS "${TOOL}")
    message(FATAL_ERROR "cli_test.cmake: TOOL is missing or does not exist: '${TOOL}'")
endif()
if(NOT DEFINED KAT OR NOT EXISTS "${KAT}")
    message(FATAL_ERROR "cli_test.cmake: KAT is missing or does not exist: '${KAT}'")
endif()
if(NOT DEFINED WORK_DIR)
    set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/cli_test")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(key128 "2b7e151628aed2a6abf7158809cf4f3c")
set(iv128 "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff")
set(keyxts "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")

function(run_expect_success name)
    execute_process(
        COMMAND "${TOOL}" ${ARGN}
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${name}: expected success, got ${result}\n${output}${error}")
    endif()
endfunction()

function(run_expect_failure name)
    execute_process(
        COMMAND "${TOOL}" ${ARGN}
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(result EQUAL 0)
        message(FATAL_ERROR "${name}: expected failure, but command succeeded\n${output}${error}")
    endif()
endfunction()

# 1. Round trip test
run_expect_success(roundtrip-encrypt encrypt --mode ctr --key-hex "${key128}" --iv-hex "${iv128}"
                   --text "Pure C++ AES-CTR Test" --out "${WORK_DIR}/ct.bin")
run_expect_success(roundtrip-decrypt decrypt --mode ctr --key-hex "${key128}" --iv-hex "${iv128}"
                   --in "${WORK_DIR}/ct.bin" --out "${WORK_DIR}/pt.txt")

file(READ "${WORK_DIR}/pt.txt" recovered_text)
if(NOT recovered_text STREQUAL "Pure C++ AES-CTR Test")
    message(FATAL_ERROR "Recovered text mismatch: '${recovered_text}'")
endif()

# 2. Non-ASCII / Vietnamese path and text support
set(viet_dir "${WORK_DIR}/thư mục")
file(MAKE_DIRECTORY "${viet_dir}")
set(viet_text "Xin chào Việt Nam — AES-CTR UTF-8 test")
file(WRITE "${viet_dir}/plain.txt" "${viet_text}")
run_expect_success(utf8-encrypt encrypt --mode ctr --key-hex "${key128}" --iv-hex "${iv128}"
                   --in "${viet_dir}/plain.txt" --out "${viet_dir}/ct.bin")
run_expect_success(utf8-decrypt decrypt --mode ctr --key-hex "${key128}" --iv-hex "${iv128}"
                   --in "${viet_dir}/ct.bin" --out "${viet_dir}/out.txt")
file(READ "${viet_dir}/out.txt" recovered_utf8)
if(NOT recovered_utf8 STREQUAL "${viet_text}")
    message(FATAL_ERROR "UTF-8 path/text round trip mismatch")
endif()

# 3. Invalid Key Length (Fail Closed)
run_expect_failure(bad-key-len encrypt --mode ctr --key-hex "123456" --iv-hex "${iv128}"
                   --text "Data" --out "${WORK_DIR}/bad.bin")

# 4. Invalid IV Length (Fail Closed)
run_expect_failure(bad-iv-len encrypt --mode ctr --key-hex "${key128}" --iv-hex "00112233"
                   --text "Data" --out "${WORK_DIR}/bad.bin")

# 5. XTS bonus round trip, including ciphertext stealing.
run_expect_success(xts-encrypt encrypt --mode xts --key-hex "${keyxts}"
                   --iv-hex "01000000000000000000000000000000"
                   --text "XTS round-trip with a partial final block."
                   --out "${WORK_DIR}/xts.bin")
run_expect_success(xts-decrypt decrypt --mode xts --key-hex "${keyxts}"
                   --iv-hex "01000000000000000000000000000000"
                   --in "${WORK_DIR}/xts.bin" --out "${WORK_DIR}/xts.out")
file(READ "${WORK_DIR}/xts.out" recovered_xts)
if(NOT recovered_xts STREQUAL "XTS round-trip with a partial final block.")
    message(FATAL_ERROR "XTS round trip mismatch")
endif()

# 6. --nonce is an alias for the CTR counter.
run_expect_success(nonce-encrypt encrypt --mode ctr --key-hex "${key128}" --nonce "${iv128}"
                   --text "nonce alias" --out "${WORK_DIR}/nonce.bin")
run_expect_success(nonce-decrypt decrypt --mode ctr --key-hex "${key128}" --nonce-hex "${iv128}"
                   --in "${WORK_DIR}/nonce.bin" --out "${WORK_DIR}/nonce.out")
file(READ "${WORK_DIR}/nonce.out" recovered_nonce)
if(NOT recovered_nonce STREQUAL "nonce alias")
    message(FATAL_ERROR "Nonce alias round trip mismatch")
endif()

# 7. Base64 encode/decode round trip.
run_expect_success(base64-encrypt encrypt --mode ctr --key-hex "${key128}" --iv-hex "${iv128}"
                   --text "base64 round trip" --encode base64 --out "${WORK_DIR}/ct.b64")
run_expect_success(base64-decrypt decrypt --mode ctr --key-hex "${key128}" --iv-hex "${iv128}"
                   --decode base64 --in "${WORK_DIR}/ct.b64" --out "${WORK_DIR}/b64.out")
file(READ "${WORK_DIR}/b64.out" recovered_b64)
if(NOT recovered_b64 STREQUAL "base64 round trip")
    message(FATAL_ERROR "Base64 round trip mismatch")
endif()

# 8. Conflicting key sources are rejected.
run_expect_failure(conflicting-key encrypt --mode ctr --key-hex "${key128}"
                   --key "${WORK_DIR}/pt.txt" --iv-hex "${iv128}"
                   --text "Data" --out "${WORK_DIR}/bad.bin")

# 9. --threads is bench-only.
run_expect_failure(threads-on-encrypt encrypt --mode ctr --key-hex "${key128}" --iv-hex "${iv128}"
                   --text "Data" --threads 2 --out "${WORK_DIR}/bad.bin")

# 10. bench rejects unknown options.
run_expect_failure(bench-unknown-option bench --mode ctr --key-hex "${key128}"
                   --size 1024 --runs 2 --warmup 0 --mystery-flag 1
                   --out "${WORK_DIR}/bad.csv")

# 11. Benchmark command writes header + five rows, including --threads.
run_expect_success(bench bench --mode ctr --key-hex "${key128}" --size 1024
                   --runs 5 --warmup 0 --threads 2 --out "${WORK_DIR}/bench.csv")
file(STRINGS "${WORK_DIR}/bench.csv" bench_lines)
list(LENGTH bench_lines bench_line_count)
if(NOT bench_line_count EQUAL 6)
    message(FATAL_ERROR "bench CSV expected 6 lines, got ${bench_line_count}")
endif()

# 11b. --verbose reports progress and completion on stderr.
execute_process(
    COMMAND "${TOOL}" bench --mode ctr --key-hex "${key128}" --size 1024
            --runs 5 --warmup 0 --threads 1 --verbose
            --out "${WORK_DIR}/bench_verbose.csv"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE bench_verbose_result
    OUTPUT_QUIET
    ERROR_VARIABLE bench_verbose_error)
if(NOT bench_verbose_result EQUAL 0)
    message(FATAL_ERROR "bench --verbose failed: ${bench_verbose_error}")
endif()
string(FIND "${bench_verbose_error}" "progress:" progress_pos)
if(progress_pos EQUAL -1)
    message(FATAL_ERROR "bench --verbose did not report progress:\n${bench_verbose_error}")
endif()
string(FIND "${bench_verbose_error}" "bench: completed 5 runs" complete_pos)
if(complete_pos EQUAL -1)
    message(FATAL_ERROR "bench --verbose did not report completion:\n${bench_verbose_error}")
endif()

# 12. JSON KAT runner covers CTR/ECB/XTS.
execute_process(
    COMMAND "${TOOL}" --kat "${KAT}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE kat_result
    OUTPUT_VARIABLE kat_output
    ERROR_VARIABLE kat_error)
if(NOT kat_result EQUAL 0)
    message(FATAL_ERROR "KAT runner failed: ${kat_output}${kat_error}")
endif()
if(NOT kat_output MATCHES "9 passed, 0 failed")
    message(FATAL_ERROR "KAT summary mismatch: ${kat_output}")
endif()

# 13. Unsupported KAT modes must fail, not silently skip.
file(WRITE "${WORK_DIR}/unsupported.json"
     "{\"vectors\":[{\"name\":\"unsupported\",\"mode\":\"gcm\",\"key\":\"00\",\"iv\":\"\",\"plaintext\":\"00\",\"ciphertext\":\"00\"}]}")
run_expect_failure(unsupported-kat --kat "${WORK_DIR}/unsupported.json")

message(STATUS "cli_test.cmake: all Lab 2 cases passed")
