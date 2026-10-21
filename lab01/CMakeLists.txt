cmake_minimum_required(VERSION 3.16)
project(aestool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build unit tests" ON)
option(CRYPTOPP_STATIC_CRT "Link the MSVC static runtime (/MT) to match a static-CRT Crypto++" ON)

# The Windows Crypto++ static library (cryptlib.lib) is normally built with
# /MT. Mixing it with CMake's default /MD fails at link time with LNK2038
# ("MT_StaticRelease doesn't match MD_DynamicRelease"). Use /MT everywhere,
# /MTd for Debug. Pass -DCRYPTOPP_STATIC_CRT=OFF (or an explicit
# -DCMAKE_MSVC_RUNTIME_LIBRARY=...) for a /MD Crypto++ build such as vcpkg.
if(MSVC AND CRYPTOPP_STATIC_CRT AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_path(CRYPTOPP_INCLUDE_DIR
    NAMES cryptopp/aes.h
    HINTS "${CRYPTOPP_ROOT}/include" "${CRYPTOPP_ROOT}")
find_library(CRYPTOPP_LIBRARY
    NAMES cryptopp cryptopp-static
    HINTS "${CRYPTOPP_ROOT}/lib" "${CRYPTOPP_ROOT}/lib64" "${CRYPTOPP_ROOT}")

if(NOT CRYPTOPP_INCLUDE_DIR OR NOT CRYPTOPP_LIBRARY)
    message(FATAL_ERROR
        "Crypto++ was not found. Install it (Linux: libcryptopp-dev) or pass "
        "-DCRYPTOPP_ROOT=/path/to/cryptopp.")
endif()

message(STATUS "Crypto++ headers: ${CRYPTOPP_INCLUDE_DIR}")
message(STATUS "Crypto++ library: ${CRYPTOPP_LIBRARY}")

add_library(aescore src/aescore.cpp src/aesgui.cpp)
target_include_directories(aescore
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PUBLIC "${CRYPTOPP_INCLUDE_DIR}")
target_link_libraries(aescore PUBLIC "${CRYPTOPP_LIBRARY}")

if(BUILD_SHARED_LIBS)
    target_compile_definitions(aescore PUBLIC AESCORE_SHARED)
    target_compile_definitions(aescore PRIVATE AESCORE_EXPORTS)
endif()

if(CRYPTOPP_LIBRARY MATCHES "cryptopp-static")
    target_compile_definitions(aescore PUBLIC CRYPTOPP_STATIC_LIBRARY)
endif()

add_executable(aestool src/main.cpp)
target_link_libraries(aestool PRIVATE aescore)

if(MSVC)
    target_compile_options(aescore PRIVATE /W4)
    target_compile_options(aestool PRIVATE /W4)
else()
    target_compile_options(aescore PRIVATE -Wall -Wextra)
    target_compile_options(aestool PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_executable(test_core tests/test_core.cpp)
    target_include_directories(test_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_link_libraries(test_core PRIVATE aescore)
    add_test(NAME aescore COMMAND test_core)
    if(UNIX)
        add_test(NAME cli COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh" $<TARGET_FILE:aestool>)
    endif()
endif()

install(TARGETS aestool aescore RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES src/aesgui.h DESTINATION include)
