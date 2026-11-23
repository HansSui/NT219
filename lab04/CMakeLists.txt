cmake_minimum_required(VERSION 3.16)
project(hashtool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build unit tests" ON)

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
# OpenSSL 3.x and 4.x both expose the EVP/X509 APIs used here.
find_package(OpenSSL 3.0 REQUIRED)
message(STATUS "OpenSSL: ${OPENSSL_VERSION}")

add_library(hashcore STATIC
    src/hash_core.cpp
    src/sha256_ext.cpp
    src/x509_inspect.cpp
)
target_include_directories(hashcore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(hashcore PUBLIC OpenSSL::Crypto)
if(WIN32)
    target_link_libraries(hashcore PUBLIC Ws2_32 Crypt32)
endif()
if(MINGW)
    target_link_libraries(hashcore PUBLIC ws2_32 crypt32 advapi32 user32 bcrypt)
endif()

add_executable(hashtool src/main.cpp)
target_link_libraries(hashtool PRIVATE hashcore)

if(MSVC)
    target_compile_options(hashcore PRIVATE /W4)
    target_compile_options(hashtool PRIVATE /W4)
else()
    target_compile_options(hashcore PRIVATE -Wall -Wextra)
    target_compile_options(hashtool PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
    enable_testing()

    foreach(suite hashes x509 lenext)
        add_executable(test_${suite} tests/test_${suite}.cpp)
        target_include_directories(test_${suite} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        target_link_libraries(test_${suite} PRIVATE hashcore)
        add_test(NAME ${suite} COMMAND test_${suite})
    endforeach()

    add_test(NAME kat COMMAND hashtool --kat "${CMAKE_CURRENT_SOURCE_DIR}/kat/hash_vectors.json")

    if(WIN32)
        add_test(NAME cli COMMAND "${CMAKE_COMMAND}"
                 "-DTOOL=$<TARGET_FILE:hashtool>"
                 "-DKAT=${CMAKE_CURRENT_SOURCE_DIR}/kat/hash_vectors.json"
                 "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_test"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.cmake")
    else()
        add_test(NAME cli COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh"
                 $<TARGET_FILE:hashtool>)
    endif()
endif()

install(TARGETS hashtool hashcore RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
