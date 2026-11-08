# Cross-platform CLI smoke test for Lab 3.
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

run_expect_success(keygen keygen --bits 3072 --pub pub.pem --priv priv.pem)
foreach(file pub.pem priv.pem pub.der priv.der priv.json)
    if(NOT EXISTS "${WORK_DIR}/${file}")
        message(FATAL_ERROR "keygen did not write ${file}")
    endif()
endforeach()

file(WRITE "${WORK_DIR}/small.txt" "direct OAEP payload")
run_expect_success(direct-encrypt encrypt --in small.txt --pub pub.pem --out small.ct --label ctx)
run_expect_success(direct-decrypt decrypt --in small.ct --priv priv.pem --out small.out --label ctx)
file(READ "${WORK_DIR}/small.out" recovered)
if(NOT recovered STREQUAL "direct OAEP payload")
    message(FATAL_ERROR "direct OAEP round-trip mismatch")
endif()

run_expect_failure(wrong-label decrypt --in small.ct --priv priv.pem --out wrong.out --label other)
if(EXISTS "${WORK_DIR}/wrong.out")
    message(FATAL_ERROR "wrong label wrote an output file")
endif()

file(WRITE "${WORK_DIR}/big.bin" "")
foreach(i RANGE 1 64)
    file(APPEND "${WORK_DIR}/big.bin" "0123456789abcdef0123456789abcdef")
endforeach()
run_expect_success(hybrid-encrypt encrypt --in big.bin --pub pub.pem --out big.env)
run_expect_success(hybrid-decrypt decrypt --in big.env --priv priv.pem --out big.out)
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${WORK_DIR}/big.bin" "${WORK_DIR}/big.out"
                RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "hybrid round-trip mismatch")
endif()

run_expect_success(base64-encrypt encrypt --in big.bin --pub pub.pem --out big.b64 --encode base64)
run_expect_success(base64-decrypt decrypt --in big.b64 --priv priv.pem --decode base64 --out big_b64.out)
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${WORK_DIR}/big.bin" "${WORK_DIR}/big_b64.out"
                RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "base64 round-trip mismatch")
endif()

file(WRITE "${WORK_DIR}/short.ct" "short")
run_expect_failure(short-ciphertext decrypt --in short.ct --priv priv.pem --out short.out)
run_expect_failure(weak-keygen keygen --bits 2048 --pub weak.pem --priv weak-priv.pem)

run_expect_success(second-keygen keygen --bits 3072 --pub pub2.pem --priv priv2.pem)
run_expect_failure(wrong-key decrypt --in small.ct --priv priv2.pem --out wrongkey.out --label ctx)
if(EXISTS "${WORK_DIR}/wrongkey.out")
    message(FATAL_ERROR "wrong key wrote an output file")
endif()

message(STATUS "cli_test.cmake: all Lab 3 cases passed")
