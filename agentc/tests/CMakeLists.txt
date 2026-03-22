# CMakeLists.txt for running tests with Google Test

cmake_minimum_required(VERSION 3.10)
project(BlackbandTests)

# Google Test
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
add_executable(runTests test_harness.cpp lexer_tests.cpp parser_tests.cpp typechecker_tests.cpp codegen_tests.cpp security_tests.cpp runtime_tests.cpp integration_tests.cpp)
target_link_libraries(runTests gtest gtest_main) 

enable_testing()
add_test(NAME BlackbandTests COMMAND runTests)