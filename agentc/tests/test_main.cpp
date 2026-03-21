#include <iostream>

void run_lexer_tests();
void run_parser_tests();
void run_typechecker_tests();
void run_codegen_tests();
void run_stdlib_tests();

int main() {
    std::cout << "Starting AgentC Test Suite...\n";
    run_lexer_tests();
    run_parser_tests();
    run_typechecker_tests();
    run_codegen_tests();
    run_stdlib_tests();
    std::cout << "All modules passed successfully.\n";
    return 0;
}
