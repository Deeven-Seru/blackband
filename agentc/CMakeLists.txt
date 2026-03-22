cmake_minimum_required(VERSION 3.20)
project(agentc VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

# LLVM
find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs
    core support irreader passes
    x86codegen x86asmparser x86info)

# libcurl (for net module)
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Compiler libraries
add_library(agentc_lexer    src/lexer/token.cpp src/lexer/lexer.cpp)
add_library(agentc_parser   src/parser/ast.cpp src/parser/parser.cpp)
add_library(agentc_tc       src/typechecker/typeenv.cpp
                            src/typechecker/rules.cpp
                            src/typechecker/typechecker.cpp)
add_library(agentc_codegen  src/codegen/codegen.cpp)
add_library(agentc_toolchain src/toolchain/pkg.cpp
                              src/toolchain/repl.cpp
                              src/toolchain/debugger.cpp)

# Runtime library
add_library(agentc_runtime STATIC
    runtime/agentc_runtime.cpp
    runtime/snapshot.cpp
    runtime/modules/io.cpp
    runtime/modules/net.cpp
    runtime/modules/val.cpp
    runtime/modules/ctx.cpp
    runtime/modules/mem.cpp
    runtime/modules/trc.cpp
    runtime/modules/std_mod.cpp)

target_link_libraries(agentc_runtime CURL::libcurl)
target_include_directories(agentc_lexer     PUBLIC src/)
target_include_directories(agentc_parser    PUBLIC src/)
target_include_directories(agentc_tc        PUBLIC src/)
target_include_directories(agentc_codegen   PUBLIC src/)
target_include_directories(agentc_toolchain PUBLIC src/)
target_include_directories(agentc_runtime   PUBLIC runtime/)

target_link_libraries(agentc_parser   agentc_lexer agentc_toolchain)
target_link_libraries(agentc_tc       agentc_parser)
target_link_libraries(agentc_codegen  agentc_tc ${llvm_libs})

# Main compiler binary
add_executable(agentc src/main.cpp)
target_link_libraries(agentc agentc_codegen agentc_runtime)

# LSP library
add_library(agentc_lsp STATIC
    src/lsp/protocol.cpp
    src/lsp/diagnostics.cpp
    src/lsp/completion.cpp
    src/lsp/hover.cpp)
target_include_directories(agentc_lsp PUBLIC src/)
target_link_libraries(agentc_lsp agentc_tc agentc_parser agentc_lexer)

# LSP Server binary
add_executable(agentc-lsp src/lsp/lsp_server.cpp)
target_link_libraries(agentc-lsp agentc_lsp)

# Tests
enable_testing()
add_executable(agentc_tests
    tests/test_main.cpp
    tests/lexer_tests.cpp
    tests/parser_tests.cpp
    tests/typechecker_tests.cpp
    tests/codegen_tests.cpp
    tests/stdlib_tests.cpp)
target_link_libraries(agentc_tests
    agentc_codegen agentc_runtime)
add_test(NAME AllTests COMMAND agentc_tests)

add_executable(agentc_lsp_tests tests/lsp_tests.cpp)
target_link_libraries(agentc_lsp_tests agentc_lsp)
add_test(NAME LSPTests COMMAND agentc_lsp_tests)

