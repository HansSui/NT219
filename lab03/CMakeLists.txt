cmake_minimum_required(VERSION 3.16)
project(rsatool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build unit tests" ON)

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
# The course requires OpenSSL 4.0.x or newer. Accept any 4.0+ patch/minor so a
# 4.0.3 install (e.g. D:\cryptolibrary) works without editing the project.
find_package(OpenSSL 4.0 REQUIRED)
if(OPENSSL_VERSION VERSION_LESS "4.0.0")
    message(FATAL_ERROR "OpenSSL 4.0 or newer is required, found ${OPENSSL_VERSION}")
endif()
find_package(Threads REQUIRED)
message(STATUS "OpenSSL: ${OPENSSL_VERSION}")

add_library(rsacore STATIC
    src/rsa.cpp
    src/hybrid.cpp
)
target_include_directories(rsacore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(rsacore PUBLIC OpenSSL::Crypto)
if(WIN32)
    target_link_libraries(rsacore PUBLIC Ws2_32 Crypt32)
endif()
if(MINGW)
    target_link_libraries(rsacore PUBLIC ws2_32 crypt32 advapi32 user32 bcrypt)
endif()

add_executable(rsatool src/main.cpp)
target_link_libraries(rsatool PRIVATE rsacore Threads::Threads)

if(MSVC)
    target_compile_options(rsacore PRIVATE /W4)
    target_compile_options(rsatool PRIVATE /W4)
else()
    target_compile_options(rsacore PRIVATE -Wall -Wextra)
    target_compile_options(rsatool PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
    enable_testing()

    foreach(suite oaep hybrid negative)
        add_executable(test_${suite} tests/test_${suite}.cpp)
        target_include_directories(test_${suite} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        target_link_libraries(test_${suite} PRIVATE rsacore)
        add_test(NAME ${suite} COMMAND test_${suite})
    endforeach()

    add_test(NAME kat COMMAND rsatool --kat "${CMAKE_CURRENT_SOURCE_DIR}/kat/oaep_vectors.json")

    if(WIN32)
        add_test(NAME cli COMMAND "${CMAKE_COMMAND}"
                 "-DTOOL=$<TARGET_FILE:rsatool>"
                 "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_test"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.cmake")
    else()
        add_test(NAME cli COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh"
                 $<TARGET_FILE:rsatool>)
    endif()
endif()

install(TARGETS rsatool rsacore RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
