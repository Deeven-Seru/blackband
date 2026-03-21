# AgentC Compiler Phase 1 — Lexer

Production-ready C++20 Lexer for the AgentC programming language. Built exclusively restricting generic human primitives favoring token-optimized Agent payloads.

## Build Requirements
- `clang++` (supporting `-std=c++20`)

## Manual Compilation
```bash
# Build the test suite natively
clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror src/lexer/token.cpp src/lexer/lexer.cpp tests/test_main.cpp tests/lexer_tests.cpp -o agentc_tests

# Build the main compiler binary directly
clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror src/lexer/token.cpp src/lexer/lexer.cpp src/main.cpp -o agentc
```

## Running Tests
```bash
./agentc_tests
```

## Running Example
```bash
./agentc examples/hello.agc
```
