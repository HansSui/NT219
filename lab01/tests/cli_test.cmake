# Cross-platform CLI smoke test. Registered on Windows, where tests/cli_test.sh
# is unavailable. Run manually with:
#   cmake -DTOOL=/path/to/aestool -DWORK_DIR=/tmp/cli-test -P cli_test.cmake
if(NOT DEFINED TOOL OR NOT EXISTS "${TOOL}")
    message(FATAL_ERROR "cli_test.cmake: TOOL is missing or does not exist: '${TOOL}'")
endif()
if(NOT DEFINED WORK_DIR)
    set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/cli_test")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(key "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
set(nonce "00112233445566778899aabbccddeeff")

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
        message(FATAL_ERROR "${name}: expected failure, but the command succeeded\n${output}${error}")
    endif()
endfunction()

# Round-trip through a non-ASCII directory and a raw key file.
set(unicode_dir "${WORK_DIR}/thư mục")
file(MAKE_DIRECTORY "${unicode_dir}")
file(WRITE "${unicode_dir}/plain.txt" "round trip payload")
file(WRITE "${unicode_dir}/key.bin" "0123456789abcdef0123456789abcdef")
run_expect_success(encrypt-file encrypt --mode cbc --key "${unicode_dir}/key.bin"
                   --in "${unicode_dir}/plain.txt" --out "${unicode_dir}/ct.bin")
run_expect_success(decrypt-file decrypt --mode cbc --key "${unicode_dir}/key.bin"
                   --in "${unicode_dir}/ct.bin" --out "${unicode_dir}/out.txt")
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${unicode_dir}/plain.txt" "${unicode_dir}/out.txt"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "round-trip plaintext does not match")
endif()

# The same key+nonce must be rejected on the second CTR use.
run_expect_success(nonce-first encrypt --mode ctr --key-hex "${key}" --iv "${nonce}"
                   --text first --out first.bin)
run_expect_failure(nonce-reuse encrypt --mode ctr --key-hex "${key}" --iv "${nonce}"
                   --text second --out second.bin)

# ECB policy and IV length validation.
string(REPEAT "A" 16385 large)
file(WRITE "${WORK_DIR}/large.bin" "${large}")
run_expect_failure(ecb-blocked encrypt --mode ecb --key-hex "${key}"
                   --in "${WORK_DIR}/large.bin" --out ecb.bin)
run_expect_failure(short-iv encrypt --mode cbc --key-hex "${key}"
                   --iv 0001020304050607 --text data --out short.bin)

# Odd hex is malformed input.
run_expect_failure(odd-hex encrypt --mode ecb --key-hex abc --text data --out odd.bin)

# Threaded benchmark writes one header plus one row per run.
run_expect_success(threaded-bench bench --mode ctr --key-hex "${key}" --size 1024
                   --runs 8 --warmup 0 --threads 2 --out bench.csv)
file(STRINGS "${WORK_DIR}/bench.csv" rows)
list(LENGTH rows count)
if(NOT count EQUAL 9)
    message(FATAL_ERROR "threaded-bench: expected 9 lines (header + 8 rows), got ${count}")
endif()

message(STATUS "cli_test.cmake: all cases passed")
