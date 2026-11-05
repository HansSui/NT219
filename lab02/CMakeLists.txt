cmake_minimum_required(VERSION 3.16)
project(aestool_lab2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build unit tests" ON)
option(ENABLE_CROSSVAL "Enable cross-validation against Crypto++" ON)
option(CRYPTOPP_STATIC_CRT "Link the MSVC static runtime (/MT) to match a static-CRT Crypto++" ON)

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})")

if(MSVC)
    add_compile_options(/W4 /O2 /Ob2)
    # The Windows cryptlib.lib used by this course is normally built with /MT.
    # Pass -DCRYPTOPP_STATIC_CRT=OFF for a /MD Crypto++ build such as vcpkg.
    if(CRYPTOPP_STATIC_CRT AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
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

    # Cross-validation with Crypto++ (oracle only; never linked into aestool).
    # The brief requires byte-for-byte parity, so a missing oracle is a
    # configuration failure unless the caller explicitly disables the target.
    if(ENABLE_CROSSVAL)
        find_path(CRYPTOPP_INCLUDE_DIR NAMES cryptopp/aes.h
            HINTS "${CRYPTOPP_ROOT}/include" "${CRYPTOPP_ROOT}" "/usr/include" "D:/cryptolibrary/include")
        find_library(CRYPTOPP_LIBRARY NAMES cryptopp cryptopp-static cryptlib
            HINTS "${CRYPTOPP_ROOT}/lib" "${CRYPTOPP_ROOT}/lib64"
                  "${CRYPTOPP_ROOT}/libs/cryptopp/msvc"
                  "${CRYPTOPP_ROOT}/libs/cryptopp/gcc"
                  "/usr/lib/x86_64-linux-gnu" "/usr/local/lib"
                  "D:/cryptolibrary/libs/cryptopp/msvc")

        if(CRYPTOPP_INCLUDE_DIR AND CRYPTOPP_LIBRARY)
            message(STATUS "Cross-validation enabled: Crypto++ include ${CRYPTOPP_INCLUDE_DIR}, library ${CRYPTOPP_LIBRARY}")
            add_executable(test_crossval tests/test_crossval.cpp)
            target_include_directories(test_crossval PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                "${CRYPTOPP_INCLUDE_DIR}"
            )
            target_link_libraries(test_crossval PRIVATE aes_core "${CRYPTOPP_LIBRARY}")
            add_test(NAME cross_validation COMMAND test_crossval)
        else()
            message(FATAL_ERROR
                "Crypto++ was not found, but ENABLE_CROSSVAL=ON. Lab 2 requires "
                "byte-for-byte parity against Crypto++/OpenSSL. Install "
                "libcryptopp-dev or pass -DCRYPTOPP_ROOT=/path/to/cryptopp. "
                "Use -DENABLE_CROSSVAL=OFF only to build a correctness-only "
                "binary that cannot claim the cross-validation requirement.")
        endif()
    else()
        message(WARNING
            "ENABLE_CROSSVAL=OFF: the build has no Crypto++ oracle and cannot "
            "claim the Lab 2 cross-validation requirement.")
    endif()
endif()
