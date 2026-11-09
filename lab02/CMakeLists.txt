cmake_minimum_required(VERSION 3.16)
project(aestool_lab2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build unit tests" ON)
option(ENABLE_CROSSVAL "Enable cross-validation against OpenSSL" ON)

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})")

if(MSVC)
    add_compile_options(/W4 /O2 /Ob2)
else()
    add_compile_options(-Wall -Wextra -O3)
endif()

find_package(Threads REQUIRED)

# ==============================================================================
# Pure C++ AES & Modes Core Library (Zero External Crypto Libraries)
# ==============================================================================
add_library(aes_core STATIC
    src/AES.cpp
    src/modes.cpp
)
target_include_directories(aes_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# ==============================================================================
# Main CLI Tool: aestool
# ==============================================================================
add_executable(aestool src/main.cpp)
target_link_libraries(aestool PRIVATE aes_core Threads::Threads)

# ==============================================================================
# Unit Tests & KAT Runner (Catch2)
# ==============================================================================
if(BUILD_TESTING)
    enable_testing()

    add_executable(test_core
        tests/test_aes.cpp
        tests/test_ctr.cpp
    )
    target_include_directories(test_core PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_link_libraries(test_core PRIVATE aes_core)

    add_test(NAME aes_unit_tests COMMAND test_core)
    add_test(NAME kat COMMAND aestool --kat "${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json")

    if(WIN32)
        add_test(NAME cli COMMAND "${CMAKE_COMMAND}"
                 "-DTOOL=$<TARGET_FILE:aestool>"
                 "-DKAT=${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json"
                 "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_test"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.cmake")
    else()
        add_test(NAME cli COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh"
                 $<TARGET_FILE:aestool> "${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json")
    endif()

    # Cross-validation with OpenSSL (oracle only; never linked into aestool).
    # The brief requires byte-for-byte parity, so a missing oracle is a
    # configuration failure unless the caller explicitly disables the target.
    if(ENABLE_CROSSVAL)
        find_package(OpenSSL REQUIRED)

        message(STATUS "Cross-validation enabled: OpenSSL ${OPENSSL_VERSION}")
        add_executable(test_crossval tests/test_crossval.cpp)
        target_include_directories(test_crossval PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
        )
        target_link_libraries(test_crossval PRIVATE aes_core OpenSSL::Crypto)
        if(WIN32)
            target_link_libraries(test_crossval PRIVATE Ws2_32 Crypt32)
        endif()
        if(MINGW)
            target_link_libraries(test_crossval PRIVATE ws2_32 crypt32 advapi32 user32 bcrypt)
        endif()
        add_test(NAME cross_validation COMMAND test_crossval)

        # Throughput oracle for the performance comparison; test-only.
        add_executable(bench_openssl tests/bench_openssl.cpp)
        target_link_libraries(bench_openssl PRIVATE OpenSSL::Crypto Threads::Threads)
        if(WIN32)
            target_link_libraries(bench_openssl PRIVATE Ws2_32 Crypt32)
        endif()
        if(MINGW)
            target_link_libraries(bench_openssl PRIVATE ws2_32 crypt32 advapi32 user32 bcrypt)
        endif()
    else()
        message(WARNING
            "ENABLE_CROSSVAL=OFF: the build has no OpenSSL oracle and cannot "
            "claim the Lab 2 cross-validation requirement.")
    endif()
endif()
