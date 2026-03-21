#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "typechecker/typechecker.hpp"
#include "codegen/codegen.hpp"
#include "toolchain/repl.hpp"
#include "toolchain/debugger.hpp"
#include "toolchain/pkg.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char** argv) {
    // Parse flags
    bool emit_ir_flag = false;
    bool run_repl_flag = false;
    bool debug_flag = false;
    std::string get_pkg_url = "";
    std::string output = "a.out";
    std::string input;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--emit-ir")   emit_ir_flag = true;
        else if (arg == "--repl") run_repl_flag = true;
        else if (arg == "--debug") debug_flag = true;
        else if (arg == "get" && i+1 < argc) get_pkg_url = argv[++i];
        else if (arg == "-o" && i+1 < argc) output = argv[++i];
        else input = arg;
    }

    if (!get_pkg_url.empty()) {
        agentc::toolchain::PackageManager pkg;
        std::string res = pkg.resolve(get_pkg_url);
        std::cout << "[pkg] Fetch cycle completed for: " << get_pkg_url << "\n";
        return 0;
    }

    if (run_repl_flag) {
        agentc::toolchain::REPL repl;
        repl.run();
        return 0;
    }

    if (input.empty()) {
        std::cerr << "{\"err\":\"no input file\"}\n";
        return 1;
    }

    // Stage 1: Read source
    std::ifstream file(input);
    std::stringstream buf; buf << file.rdbuf();
    std::string source = buf.str();

    // Stage 2: Lex
    agentc::Lexer lexer(source, input);
    std::vector<agentc::Token> tokens;
    while (!lexer.at_end()) {
        auto t = lexer.next(); tokens.push_back(t);
        if (t.kind == agentc::Token::Kind::TOK_EOF) break;
    }
    if (lexer.has_errors()) {
        std::cerr << lexer.errors_as_json() << "\n"; return 1;
    }

    // Stage 3: Parse
    agentc::Parser parser(std::move(tokens), input);
    auto ast = parser.parse();
    if (parser.has_errors()) {
        std::cerr << parser.errors_as_json() << "\n"; return 1;
    }

    // Stage 4: Type check
    agentc::TypeChecker tc(input);
    auto typed_ast = tc.check(std::move(ast));
    if (tc.has_errors()) {
        std::cerr << tc.errors_as_json() << "\n"; return 1;
    }

    // Stage 5: Code generation
    agentc::CodeGen cg(input, input);
    if (!cg.generate(typed_ast)) {
        std::cerr << cg.errors_as_json() << "\n"; return 1;
    }

    if (emit_ir_flag) {
        std::string ir_path = output + ".ll";
        cg.emit_ir(ir_path);
        std::cout << "{\"status\":\"ok\",\"ir\":\"" << ir_path << "\"}\n";
    } else {
        cg.emit_executable(output);
        std::cout << "{\"status\":\"ok\",\"bin\":\"" << output << "\"}\n";
        if (debug_flag) {
            agentc::toolchain::NativeDebugger dbg;
            dbg.run(output);
        }
    }

    return 0;
}
