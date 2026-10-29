# Cross-platform CLI smoke test for Lab 2
if(NOT DEFINED TOOL OR NOT EXISTS "${TOOL}")
    message(FATAL_ERROR "cli_test.cmake: TOOL is missing or does not exist: '${TOOL}'")
endif()
if(NOT DEFINED WORK_DIR)
    set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/cli_test")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(key128 "2b7e151628aed2a6abf7158809cf4f3c")
set(iv128 "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff")

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

# 2. Invalid Key Length (Fail Closed)
run_expect_failure(bad-key-len encrypt --mode ctr --key-hex "123456" --iv-hex "${iv128}"
                   --text "Data" --out "${WORK_DIR}/bad.bin")

# 3. Invalid IV Length (Fail Closed)
run_expect_failure(bad-iv-len encrypt --mode ctr --key-hex "${key128}" --iv-hex "00112233"
                   --text "Data" --out "${WORK_DIR}/bad.bin")

message(STATUS "cli_test.cmake: all Lab 2 cases passed")
