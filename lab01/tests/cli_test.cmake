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

function(run_expect_failure_in name workdir)
    execute_process(
        COMMAND "${TOOL}" ${ARGN}
        WORKING_DIRECTORY "${workdir}"
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

# Base64 output must be accepted back through --decode.
run_expect_success(base64-encode encrypt --mode cbc --key "${unicode_dir}/key.bin"
                   --in "${unicode_dir}/plain.txt" --out "${unicode_dir}/ct.b64"
                   --encode base64)
run_expect_success(base64-decode decrypt --mode cbc --key "${unicode_dir}/key.bin"
                   --in "${unicode_dir}/ct.b64" --decode base64
                   --out "${unicode_dir}/b64_out.txt")
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${unicode_dir}/plain.txt" "${unicode_dir}/b64_out.txt"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "base64 plaintext does not match")
endif()
file(READ "${unicode_dir}/ct.b64" b64_text)
run_expect_success(base64-text-decode decrypt --mode cbc --key "${unicode_dir}/key.bin"
                   --text "${b64_text}" --decode base64
                   --out "${unicode_dir}/b64_text_out.txt")
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                        "${unicode_dir}/plain.txt" "${unicode_dir}/b64_text_out.txt"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "base64 --text plaintext does not match")
endif()

# The same key+nonce must be rejected on the second CTR use.
run_expect_success(nonce-first encrypt --mode ctr --key-hex "${key}" --iv "${nonce}"
                   --text first --out first.bin)
run_expect_failure(nonce-reuse encrypt --mode ctr --key-hex "${key}" --iv "${nonce}"
                   --text second --out second.bin)

# A held lock must make the command fail closed, not continue unlocked.
file(MAKE_DIRECTORY "${WORK_DIR}/.aestool_nonces.lock")
run_expect_failure(lock-contention encrypt --mode ctr --key-hex "${key}"
                   --iv "ffeeddccbbaa99887766554433221100"
                   --text locked --out locked.bin)
file(REMOVE_RECURSE "${WORK_DIR}/.aestool_nonces.lock")

# ECB policy and IV length validation.
string(REPEAT "A" 16385 large)
file(WRITE "${WORK_DIR}/large.bin" "${large}")
run_expect_failure(ecb-blocked encrypt --mode ecb --key-hex "${key}"
                   --in "${WORK_DIR}/large.bin" --out ecb.bin)
run_expect_failure(short-iv encrypt --mode cbc --key-hex "${key}"
                   --iv 0001020304050607 --text data --out short.bin)
run_expect_failure(xts-missing-iv encrypt --mode xts --key-hex "${key}"
                   --text data --out xts.bin)
run_expect_failure(threads-encrypt encrypt --mode cbc --key-hex "${key}"
                   --text data --threads 2 --out threaded.bin)

# Odd hex is malformed input.
run_expect_failure(odd-hex encrypt --mode ecb --key-hex abc --text data --out odd.bin)

# AEAD flag and AAD policy failures.
run_expect_failure(aead-without-flag encrypt --mode gcm --key-hex "${key}"
                   --text data --out no_aead.bin)
run_expect_failure(aad-non-aead encrypt --mode cbc --key-hex "${key}"
                   --aad-text header --text data --out aad_nonaead.bin)

# GCM with the wrong AAD must fail and leave no plaintext.
set(gcm_aad_nonce "112233445566778899aabbcc")
run_expect_success(gcm-aad-encrypt encrypt --mode gcm --aead --key-hex "${key}"
                   --nonce "${gcm_aad_nonce}" --aad-text right
                   --text "aad payload" --out "${WORK_DIR}/aad_ct.bin")
run_expect_failure(gcm-wrong-aad decrypt --mode gcm --aead --key-hex "${key}"
                   --in "${WORK_DIR}/aad_ct.bin" --aad-text wrong
                   --out "${WORK_DIR}/aad_out.txt")
if(EXISTS "${WORK_DIR}/aad_out.txt")
    message(FATAL_ERROR "gcm-wrong-aad: wrote an output file")
endif()

# CCM with a modified ciphertext must fail tag verification and write nothing.
set(ccm_nonce "ccddeeff0011223344556677")
run_expect_success(ccm-encrypt encrypt --mode ccm --aead --key-hex "${key}"
                   --nonce "${ccm_nonce}" --text "ccm payload"
                   --out "${WORK_DIR}/ccm_ct.bin")
file(APPEND "${WORK_DIR}/ccm_ct.bin" "X")
run_expect_failure(ccm-bad-tag decrypt --mode ccm --aead --key-hex "${key}"
                   --in "${WORK_DIR}/ccm_ct.bin" --out "${WORK_DIR}/ccm_out.txt")
if(EXISTS "${WORK_DIR}/ccm_out.txt")
    message(FATAL_ERROR "ccm-bad-tag: wrote an output file")
endif()

# The container header mode must match --mode.
run_expect_success(cbc-mismatch-encrypt encrypt --mode cbc --key-hex "${key}"
                   --text mismatch --out "${WORK_DIR}/cbc_mismatch.bin")
run_expect_failure(mode-header-mismatch decrypt --mode gcm --aead --key-hex "${key}"
                   --in "${WORK_DIR}/cbc_mismatch.bin" --out "${WORK_DIR}/mismatch.out")

# --allow-ecb is the explicit override for the 16 KiB limit.
run_expect_success(ecb-allowed encrypt --mode ecb --key-hex "${key}" --allow-ecb
                   --in "${WORK_DIR}/large.bin" --out "${WORK_DIR}/ecb_allowed.bin")

# Sidecar JSON header export and round-trip.
set(gcm_nonce "00112233445566778899aabb")
run_expect_success(sidecar-encrypt encrypt --mode gcm --aead --key-hex "${key}"
                   --nonce "${gcm_nonce}" --text "sidecar test" --out "${WORK_DIR}/sidecar_ct.bin"
                   --sidecar "${WORK_DIR}/sidecar.json")
run_expect_success(sidecar-decrypt decrypt --mode gcm --aead --key-hex "${key}"
                   --in "${WORK_DIR}/sidecar_ct.bin" --sidecar "${WORK_DIR}/sidecar.json"
                   --out "${WORK_DIR}/sidecar_out.txt")
file(READ "${WORK_DIR}/sidecar_out.txt" recovered_sidecar)
if(NOT recovered_sidecar STREQUAL "sidecar test")
    message(FATAL_ERROR "sidecar plaintext mismatch: got '${recovered_sidecar}'")
endif()

# --sidecar without a path defaults to OUT.json on encrypt and IN.json on decrypt.
set(default_nonce "aabbccddeeff001122334455")
run_expect_success(sidecar-default-encrypt encrypt --mode gcm --aead --key-hex "${key}"
                   --iv "${default_nonce}" --text "default sidecar"
                   --out "${WORK_DIR}/default_ct.bin" --sidecar)
run_expect_success(sidecar-default-decrypt decrypt --mode gcm --aead --key-hex "${key}"
                   --in "${WORK_DIR}/default_ct.bin" --sidecar
                   --out "${WORK_DIR}/default_out.txt")
file(READ "${WORK_DIR}/default_out.txt" recovered_default)
if(NOT recovered_default STREQUAL "default sidecar")
    message(FATAL_ERROR "default sidecar plaintext mismatch: got '${recovered_default}'")
endif()

# A copied sidecar in a fresh directory must be rejected from the header alone.
set(sidecar_reuse_dir "${WORK_DIR}/sidecar-reuse")
file(MAKE_DIRECTORY "${sidecar_reuse_dir}")
file(COPY "${WORK_DIR}/sidecar.json" DESTINATION "${sidecar_reuse_dir}")
run_expect_failure_in(sidecar-nonce-reuse "${sidecar_reuse_dir}"
    encrypt --mode gcm --aead --key-hex "${key}" --iv "${gcm_nonce}"
    --text reuse --out new.bin --sidecar sidecar.json)
if(EXISTS "${sidecar_reuse_dir}/new.bin")
    message(FATAL_ERROR "sidecar-nonce-reuse: wrote an output file")
endif()

# Threaded benchmark writes one header plus one row per run.
run_expect_success(threaded-bench bench --mode ctr --key-hex "${key}" --size 1024
                   --runs 8 --warmup 0 --threads 2 --out bench.csv)
file(STRINGS "${WORK_DIR}/bench.csv" rows)
list(LENGTH rows count)
if(NOT count EQUAL 9)
    message(FATAL_ERROR "threaded-bench: expected 9 lines (header + 8 rows), got ${count}")
endif()

message(STATUS "cli_test.cmake: all cases passed")
