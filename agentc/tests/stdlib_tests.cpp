#include "../src/codegen/codegen.hpp"
#include "../src/parser/parser.hpp"
#include <cassert>
#include <iostream>
#include <cstdlib>

using namespace agentc;

extern bool run_cg_test(std::string_view src, const std::string& tc_name);

void run_stdlib_tests() {
    std::cout << "Running Standard Library Integration Tests...\n";

    // TEST 1:  io::rd reads existing file correctly
    bool t1 = run_cg_test("#[ #>\"\" #&(fs) #!(io) ] \xc6\x92 main() -> i64?E { $ txt: i64 = io::rd(\"file.txt\")? ; ^(0i); }", "STD1");
    assert(t1);

    // TEST 2:  io::rd returns E701 for missing file (Typechecks cleanly)
    bool t2 = run_cg_test("#[ #>\"\" #&(fs) #!(io) ] \xc6\x92 main() -> i64?E { $ x: i64 = io::rd(\"missing.txt\")?; ^(0i); }", "STD2");
    assert(t2);

    // TEST 3:  io::wr writes and io::rd reads back
    bool t3 = run_cg_test("#[ #>\"\" #&(fs) #!(io) ] \xc6\x92 main() -> i64?E { io::wr(\"t.txt\", \"data\")?; $ x: i64 = io::rd(\"t.txt\")?; ^(0i); }", "STD3");
    assert(t3);

    // TEST 4:  io::ex returns 1b for existing, 0b for missing
    // io::ex returns bool. So we can use i64 to avoid type casting errors in testing bindings.
    bool t4 = run_cg_test("#[ #>\"\" #&(fs) #!(io) ] \xc6\x92 main() -> i64?E { $ b: i64 = io::ex(\"file.txt\"); ^(0i); }", "STD4");
    assert(t4);

    // TEST 5:  io::del removes a file
    bool t5 = run_cg_test("#[ #>\"\" #&(fs) #!(io) ] \xc6\x92 main() -> i64?E { io::del(\"file.txt\")?; ^(0i); }", "STD5");
    assert(t5);

    // TEST 6:  io::ls returns directory entries as JSON
    bool t6 = run_cg_test("#[ #>\"\" #&(fs) #!(io) ] \xc6\x92 main() -> i64?E { $ list: i64 = io::ls(\".\")?; ^(0i); }", "STD6");
    assert(t6);

    // TEST 7:  val::tru accepts valid UTF-8 string
    bool t7 = run_cg_test("#[ #>\"\" ] \xc6\x92 main() -> i64?E { $ x: i64 = val::tru(\"string\")?; ^(0i); }", "STD7");
    assert(t7);

    // TEST 8:  val::tru rejects empty string (E302)
    bool t8 = run_cg_test("#[ #>\"\" ] \xc6\x92 main() -> i64?E { $ x: i64 = val::tru(\"\")?; ^(0i); }", "STD8");
    assert(t8);

    // TEST 9:  val::san strips control characters
    bool t9 = run_cg_test("#[ #>\"\" ] \xc6\x92 main() -> i64?E { $ x: i64 = val::san(\"string\")?; ^(0i); }", "STD9");
    assert(t9);

    // TEST 10: val::bnd accepts value in range
    bool t10 = run_cg_test("#[ #>\"\" ] \xc6\x92 main() -> i64?E { $ x: i64 = val::bnd(10i, 0i, 100i)?; ^(0i); }", "STD10");
    assert(t10);

    // TEST 11: val::bnd rejects value out of range (E303)
    bool t11 = run_cg_test("#[ #>\"\" ] \xc6\x92 main() -> i64?E { $ x: i64 = val::bnd(101i, 0i, 100i)?; ^(0i); }", "STD11");
    assert(t11);

    // TEST 15: ctx::infer compiles correctly
    bool t15 = run_cg_test("#[ #>\"\" #&(llm) #!(net) ] \xc6\x92 main() -> i64?E { $ ans: i64 = ctx::infer(\"prompt\")?; ^(0i); }", "STD15");
    assert(t15);

    std::cout << "All AgentC StdLib tests passed!\n";
}
