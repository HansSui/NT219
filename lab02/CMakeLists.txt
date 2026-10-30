cmake_minimum_required(VERSION 3.16)
project(aestool_lab2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build unit tests" ON)
option(ENABLE_CROSSVAL "Enable cross-validation against Crypto++" ON)

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})")

if(MSVC)
    add_compile_options(/W4 /O2 /Ob2)
    # Match static runtime if desired
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
else()
    add_compile_options(-Wall -Wextra -O3)
endif()

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
target_link_libraries(aestool PRIVATE aes_core)

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

    if(WIN32)
        add_test(NAME cli COMMAND "${CMAKE_COMMAND}"
                 "-DTOOL=$<TARGET_FILE:aestool>"
                 "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_test"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.cmake")
    else()
        add_test(NAME cli COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh" $<TARGET_FILE:aestool>)
    endif()

    # Optional Cross-Validation with Crypto++ (used strictly for oracle verification)
    if(ENABLE_CROSSVAL)
        find_path(CRYPTOPP_INCLUDE_DIR NAMES cryptopp/aes.h
            HINTS "${CRYPTOPP_ROOT}/include" "${CRYPTOPP_ROOT}" "/usr/include" "D:/cryptolibrary/include")
        find_library(CRYPTOPP_LIBRARY NAMES cryptopp cryptopp-static
            HINTS "${CRYPTOPP_ROOT}/lib" "${CRYPTOPP_ROOT}/lib64" "/usr/lib/x86_64-linux-gnu" "/usr/local/lib" "D:/cryptolibrary/libs/cryptopp/msvc")

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
            message(STATUS "Crypto++ not found; skipping optional cross-validation test target.")
        endif()
    endif()
endif()
