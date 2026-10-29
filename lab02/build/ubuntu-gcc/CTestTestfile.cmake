# CMake generated Testfile for 
# Source directory: /home/han-sui/Downloads/NT219/lab02
# Build directory: /home/han-sui/Downloads/NT219/lab02/build/ubuntu-gcc
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(aes_unit_tests "/home/han-sui/Downloads/NT219/lab02/build/ubuntu-gcc/test_core")
set_tests_properties(aes_unit_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/han-sui/Downloads/NT219/lab02/CMakeLists.txt;54;add_test;/home/han-sui/Downloads/NT219/lab02/CMakeLists.txt;0;")
add_test(cli "/home/han-sui/Downloads/NT219/lab02/tests/cli_test.sh" "/home/han-sui/Downloads/NT219/lab02/build/ubuntu-gcc/aestool")
set_tests_properties(cli PROPERTIES  _BACKTRACE_TRIPLES "/home/han-sui/Downloads/NT219/lab02/CMakeLists.txt;62;add_test;/home/han-sui/Downloads/NT219/lab02/CMakeLists.txt;0;")
add_test(cross_validation "/home/han-sui/Downloads/NT219/lab02/build/ubuntu-gcc/test_crossval")
set_tests_properties(cross_validation PROPERTIES  _BACKTRACE_TRIPLES "/home/han-sui/Downloads/NT219/lab02/CMakeLists.txt;81;add_test;/home/han-sui/Downloads/NT219/lab02/CMakeLists.txt;0;")
