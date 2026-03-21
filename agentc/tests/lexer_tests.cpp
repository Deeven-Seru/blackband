#include "../src/lexer/lexer.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace agentc;

void run_lexer_tests() {
    // Test 1: Basic bindings
    {
        Lexer l("$ x: i32 = 42i;", "test1");
        assert(l.next().kind == Token::Kind::TOK_DOLLAR);
        assert(l.next().value == "x");
        assert(l.next().kind == Token::Kind::TOK_COLON);
        assert(l.next().kind == Token::Kind::TOK_TYPE_I32);
        assert(l.next().kind == Token::Kind::TOK_EQ);
        assert(l.next().kind == Token::Kind::TOK_INT_LIT);
        assert(l.next().kind == Token::Kind::TOK_SEMICOLON);
        assert(l.next().kind == Token::Kind::TOK_EOF);
    }
    // Test 2: Function declaration
    {
        Lexer l("ƒ foo(x: Str) -> i32?IoE { }", "test2");
        assert(l.next().kind == Token::Kind::TOK_FUNC);
        assert(l.next().value == "foo");
        assert(l.next().kind == Token::Kind::TOK_LPAREN);
        assert(l.next().value == "x");
        assert(l.next().kind == Token::Kind::TOK_COLON);
        assert(l.next().kind == Token::Kind::TOK_TYPE_STR);
        assert(l.next().kind == Token::Kind::TOK_RPAREN);
        assert(l.next().kind == Token::Kind::TOK_ARROW);
        assert(l.next().kind == Token::Kind::TOK_TYPE_I32);
        assert(l.next().kind == Token::Kind::TOK_RESULT_OP);
        assert(l.next().value == "IoE");
        assert(l.next().kind == Token::Kind::TOK_LBRACE);
        assert(l.next().kind == Token::Kind::TOK_RBRACE);
    }
    // Test 3: Annotation block
    {
        Lexer l("#[ #>\"reads file\" #$(io) ]", "test3");
        assert(l.next().kind == Token::Kind::TOK_ANN_OPEN);
        assert(l.next().kind == Token::Kind::TOK_ANN_INTENT);
        Token t_str = l.next();
        assert(t_str.kind == Token::Kind::TOK_STR_LIT && t_str.value == "\"reads file\"");
        assert(l.next().kind == Token::Kind::TOK_ANN_COST);
        assert(l.next().kind == Token::Kind::TOK_LPAREN);
        assert(l.next().value == "io");
        assert(l.next().kind == Token::Kind::TOK_RPAREN);
        assert(l.next().kind == Token::Kind::TOK_ANN_CLOSE);
    }
    // Test 4: Boolean literals
    {
        Lexer l("1b 0b", "test4");
        assert(l.next().kind == Token::Kind::TOK_BOOL_TRUE);
        assert(l.next().kind == Token::Kind::TOK_BOOL_FALSE);
    }
    // Test 5: Error propagation vs if-condition
    {
        Lexer l("fetch(url)? ?(x > 0) { }", "test5");
        assert(l.next().value == "fetch");
        assert(l.next().kind == Token::Kind::TOK_LPAREN);
        assert(l.next().value == "url");
        assert(l.next().kind == Token::Kind::TOK_RPAREN);
        assert(l.next().kind == Token::Kind::TOK_PROPAGATE);
        assert(l.next().kind == Token::Kind::TOK_QUESTION);
        assert(l.next().kind == Token::Kind::TOK_LPAREN);
        assert(l.next().value == "x");
        assert(l.next().kind == Token::Kind::TOK_GT);
        assert(l.next().kind == Token::Kind::TOK_INT_LIT); // Matches literal
        assert(l.next().kind == Token::Kind::TOK_RPAREN);
    }
    // Test 6: At-primitives
    {
        Lexer l("@spn(Sum)?", "test6");
        assert(l.next().kind == Token::Kind::TOK_AT_SPN);
        assert(l.next().kind == Token::Kind::TOK_LPAREN);
        assert(l.next().value == "Sum");
        assert(l.next().kind == Token::Kind::TOK_RPAREN);
        assert(l.next().kind == Token::Kind::TOK_PROPAGATE);
    }
    // Test 7: Result type
    {
        Lexer l("Str?IoE", "test7");
        assert(l.next().kind == Token::Kind::TOK_TYPE_STR);
        assert(l.next().kind == Token::Kind::TOK_RESULT_OP);
        assert(l.next().value == "IoE");
    }
    // Test 8: Import
    {
        Lexer l("+> AgentCore::IoError;", "test8");
        assert(l.next().kind == Token::Kind::TOK_IMPORT);
        assert(l.next().value == "AgentCore");
        assert(l.next().kind == Token::Kind::TOK_BIND);
        assert(l.next().value == "IoError");
    }
    // Test 9: Error case - structured JSON output
    {
        Lexer l("$ x: i32 = @@@;", "test9");
        l.next(); l.next(); l.next(); l.next(); l.next(); 
        assert(l.next().kind == Token::Kind::TOK_ERROR); 
        assert(l.has_errors());
    }
    // Test 10: Memory modifier
    {
        Lexer l("$ x: @mem(work) Str", "test10");
        assert(l.next().kind == Token::Kind::TOK_DOLLAR);
        assert(l.next().value == "x");
        assert(l.next().kind == Token::Kind::TOK_COLON);
        assert(l.next().kind == Token::Kind::TOK_AT_MEM);
        assert(l.next().kind == Token::Kind::TOK_LPAREN);
        assert(l.next().kind == Token::Kind::TOK_MEM_WORK);
        assert(l.next().kind == Token::Kind::TOK_RPAREN);
        assert(l.next().kind == Token::Kind::TOK_TYPE_STR);
    }
}
