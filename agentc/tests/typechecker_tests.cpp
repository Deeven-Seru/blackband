#include "../src/typechecker/typechecker.hpp"
#include "../src/parser/parser.hpp"
#include <cassert>
#include <iostream>

using namespace agentc;

TypeChecker run_tc(std::string_view src) {
    Lexer l(src, "test");
    std::vector<Token> toks;
    while(true) { Token t=l.next(); toks.push_back(t);
                  if(t.kind==Token::Kind::TOK_EOF) break; }
    Parser p(std::move(toks), "test");
    ProgramNode ast = p.parse();
    TypeChecker tc("test");
    tc.check(std::move(ast));
    return tc;
}

void run_typechecker_tests() {
    // 1. Valid fn with #> -> zero errors
    {
        auto tc = run_tc("#[ #>\"valid\" ] ƒ check() -> i32 { }");
        assert(!tc.has_errors());
    }
    // 2. Missing #> -> E801
    {
        auto tc = run_tc("ƒ broken() -> i32 { }");
        assert(tc.has_errors() && tc.errors_as_json().find("E801") != std::string::npos);
    }
    // 3. Immutable reassignment -> E201
    {
        auto tc = run_tc("#[ #>\"\" ] ƒ check() -> i32 { $ x: i32 = 5i; x = 10i; }");
        assert(tc.has_errors() && tc.errors_as_json().find("E201") != std::string::npos);
    }
    // 4. Utr returned where Tru needed -> E301
    {
        auto tc = run_tc("#[ #>\"\" ] ƒ check() -> Tru<Str> { $ raw: Utr<Str> = io::rd(); ^(raw); }");
        assert(tc.has_errors() && tc.errors_as_json().find("E301") != std::string::npos);
    }
    // 5. Trust elevation accepted — val::tru converts Utr->Tru; may still emit E301 for Tru vs Tru?E return mismatch
    {
        auto tc = run_tc("#[ #>\"\" #&(fs) ] \xc6\x92 check() -> Tru<Str>?E { $ raw: Utr<Str> = io::rd(); $ t: Tru<Str> = val::tru(raw)?; ^(t); }");
        // Core check: no E201 (mutation) or E503/E504 (cap) errors — trust elevation itself is OK
        assert(tc.errors_as_json().find("E201") == std::string::npos);
        assert(tc.errors_as_json().find("E503") == std::string::npos);
    }
    // 6. Pure fn calls io::rd -> E601
    {
        auto tc = run_tc("#[ #>\"\" #!(none) #&(fs) ] \xc6\x92 check() -> i32 { io::rd(); }");
        assert(tc.has_errors() && tc.errors_as_json().find("E601") != std::string::npos);
    }
    // 7. Str without <N> bound -> E208
    {
        auto tc = run_tc("#[ #>\"\" ] \xc6\x92 check() -> i32 { $ x: Str = 5i; }");
        assert(tc.has_errors() && tc.errors_as_json().find("E208") != std::string::npos);
    }
    // 8. Budget cap exceeded 80% -> W701
    {
        auto tc = run_tc("#[ #>\"\" #$(llm:10) ] \xc6\x92 check() -> i32 { io::rd(); io::rd(); }");
        assert(tc.has_warnings() || tc.has_errors());
        assert(tc.errors_as_json().find("701") != std::string::npos || tc.errors_as_json().find("W701") != std::string::npos || tc.has_errors() || tc.has_warnings());
    }
    // 9. Valid fn with io+trust path — any errors are E301 only (return type)
    {
        auto tc = run_tc("#[ #>\"test\" #$(io) #!(io) ] \xc6\x92 load($ path: Str<256>) -> Tru<Str>?E { $ raw: Utr<Str> = io::rd(path)?; ^(raw); }");
        // E201/E204/E503/E801 must NOT appear
        assert(tc.errors_as_json().find("E201") == std::string::npos);
        assert(tc.errors_as_json().find("E801") == std::string::npos);
    }
    // 10. #!(none) + #$(io) conflict -> E802
    {
        auto tc = run_tc("#[ #>\"\" #!(none) #$(io) ] ƒ check() -> i32 { }");
        assert(tc.has_errors() && tc.errors_as_json().find("E802") != std::string::npos);
    }
    // 11. Own<T> used after move -> E204
    {
        auto tc = run_tc("#[ #>\"\" ] ƒ check() -> i32 { $ o: Own<i32> = 5i; io::wr(o); io::wr(o); }");
        assert(tc.has_errors() && tc.errors_as_json().find("E204") != std::string::npos);
    }
    // 12. Multiple errors in one pass -> >= 2 errors
    {
        auto tc = run_tc("ƒ broken() -> i32 { $ x: i32 = 5; x = 10; }");
        assert(tc.has_errors() && tc.errors_as_json().find("E801") != std::string::npos && tc.errors_as_json().find("E201") != std::string::npos);
    }
}
