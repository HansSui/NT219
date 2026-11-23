# Cross-platform CLI smoke test for Lab 4.
if(NOT DEFINED TOOL OR NOT EXISTS "${TOOL}")
    message(FATAL_ERROR "cli_test.cmake: TOOL is missing or does not exist: '${TOOL}'")
endif()
if(NOT DEFINED WORK_DIR)
    set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/cli_test")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

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
    set(RESULT "${output}" PARENT_SCOPE)
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

file(WRITE "${WORK_DIR}/abc.bin" "abc")
run_expect_success(hash --algo sha256 --in abc.bin)
if(NOT RESULT STREQUAL "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    message(FATAL_ERROR "sha256(abc) mismatch: ${RESULT}")
endif()

run_expect_failure(md5 --algo md5 --in abc.bin)
run_expect_failure(fixed-outlen --algo sha256 --outlen 256 --in abc.bin)
run_expect_failure(shake-no-outlen --algo shake128 --in abc.bin)
run_expect_failure(shake-bad-outlen --algo shake128 --outlen 7 --in abc.bin)
run_expect_failure(missing-file --algo sha256 --in missing.bin)
run_expect_failure(bad-encoding --algo sha256 --in abc.bin --encode nope)

run_expect_success(kat --kat "${KAT}")
if(NOT RESULT MATCHES "0 failed")
    message(FATAL_ERROR "KAT did not report zero failures: ${RESULT}")
endif()

file(WRITE "${WORK_DIR}/key.bin" "0123456789abcdef")
file(WRITE "${WORK_DIR}/message.bin" "amount=100&to=alice")
run_expect_success(lenext --mac 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff --key-len 16 --in message.bin --append-text "&to=mallory" --out forged.bin)
if(NOT RESULT MATCHES "forged_mac:")
    message(FATAL_ERROR "lenext did not print a forged MAC")
endif()
run_expect_failure(lenext-short-mac --mac 00 --key-len 16 --in message.bin --append-text x)

run_expect_success(bench --algos sha256 --sizes 1 --io stream,mmap --runs 1 --warmup 0 --out bench.csv)
file(READ "${WORK_DIR}/bench.csv" bench)
if(NOT bench MATCHES "algo,size_bytes,size_mib,io,run,elapsed_ms,mbps,cpu_ms,cpu_pct")
    message(FATAL_ERROR "benchmark CSV header is missing")
endif()

message(STATUS "cli_test.cmake: all Lab 4 cases passed")
