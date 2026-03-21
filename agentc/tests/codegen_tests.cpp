#include "../src/codegen/codegen.hpp"
#include "../src/parser/parser.hpp"
#include <cassert>
#include <iostream>
#include <cstdlib>

using namespace agentc;

bool run_cg_test(std::string_view src, const std::string& tc_name) {
    Lexer l(src, "test"); std::vector<Token> toks;
    while(true) { Token t=l.next(); toks.push_back(t); if(t.kind==Token::Kind::TOK_EOF) break; }
    Parser p(std::move(toks), "test"); ProgramNode ast = p.parse();
    TypeChecker tc("test"); auto typed_ast = tc.check(std::move(ast));
    CodeGen cg("test", "test");
    bool cg_ok = cg.generate(typed_ast);
    if (!cg_ok) std::cerr << cg.errors_as_json() << "\n";
    return cg_ok;
}

void run_codegen_tests() {
    std::cout << "Running LLVM CodeGen Testing Suite...\n";
    
    // 1. Compile hello.agc to IR
    bool t1 = run_cg_test("#[ #>\"main\" #&(fs) #$(io) ] \xc6\x92 main() -> () { agc_print(\"Hello\"); }", "T1");
    assert(t1);

    // 2. Integer arithmetic
    bool t2 = run_cg_test("#[ #>\"main\" ] \xc6\x92 add($ x: i32, $ y: i32) -> i32 { x + y; } \xc6\x92 main() -> i32 { add(3i, 4i); }", "T2");
    assert(t2);

    // 3. Boolean logic
    bool t3 = run_cg_test("#[ #>\"main\" ] \xc6\x92 main() -> i32 { $ x: i32 = 10i; $ y: i32 = 20i; ?(x < y) { ^(1i); } : { ^(0i); } }", "T3");
    assert(t3);

    // 4. String literal print
    bool t4 = run_cg_test("#[ #>\"main\" ] \xc6\x92 main() -> () { agc_print(\"Hello AgentC!\"); }", "T4");
    assert(t4);

    // 5. Result wrap
    bool t5 = run_cg_test("#[ #>\"\" ] \xc6\x92 wrap($ x: i32) -> i32?E { ^(x); } \xc6\x92 main() -> i32?E { wrap(42i)?; }", "T5");
    assert(t5);

    // 6. Branch execution bounds
    bool t6 = run_cg_test("#[ #>\"\" ] \xc6\x92 main() -> i32 { ?(1i == 1i) { ^(2i); } : { ^(3i); } }", "T6");
    assert(t6);

    // 7. Inner functions execution mapping refs
    bool t7 = run_cg_test("#[ #>\"\" ] \xc6\x92 c() -> i32 { ^(4i); } \xc6\x92 b() -> i32 { c(); } \xc6\x92 main() -> i32 { b(); }", "T7");
    assert(t7);

    // 8. Error / Panic paths natively
    bool t8 = run_cg_test("#[ #>\"\" ] \xc6\x92 test() -> i32?E { ^!(5i); }", "T8");
    assert(t8);

    std::cout << "All AgentC CodeGen Unit Tests passed successfully.\n";
}
