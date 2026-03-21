#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace agentc {

struct LexError {
    std::string err;
    std::string cat;
    size_t line;
    size_t col;
    std::string expected;
    std::string got;
    std::string act;
    std::string via;
    std::string alt;
};

struct Token {
    enum class Kind {
        // BINDING TOKENS
        TOK_DOLLAR, TOK_TILDE, TOK_WALRUS,
        // FUNCTION TOKENS
        TOK_FUNC, TOK_ARROW, TOK_CARET, TOK_CARET_BANG,
        // CONTROL FLOW
        TOK_QUESTION, TOK_COLON, TOK_AT_ITER, TOK_LARROW, TOK_DBLGT,
        // ANNOTATION TOKENS
        TOK_ANN_OPEN, TOK_ANN_CLOSE, TOK_ANN_INTENT, TOK_ANN_COST,
        TOK_ANN_EFFECT, TOK_ANN_VERIFY, TOK_ANN_TRUST, TOK_ANN_CONF,
        TOK_ANN_RETRY, TOK_ANN_GOAL, TOK_ANN_CAP,
        // TYPE TOKENS
        TOK_TYPE_I8, TOK_TYPE_I16, TOK_TYPE_I32, TOK_TYPE_I64, TOK_TYPE_I128,
        TOK_TYPE_U8, TOK_TYPE_U16, TOK_TYPE_U32, TOK_TYPE_U64, TOK_TYPE_U128,
        TOK_TYPE_F32, TOK_TYPE_F64, TOK_TYPE_B, TOK_TYPE_STR,
        TOK_TYPE_UNIT, TOK_TYPE_NEVER, TOK_TYPE_OWN, TOK_TYPE_REF, TOK_TYPE_SHR,
        TOK_TYPE_OPT, TOK_TYPE_TRU, TOK_TYPE_UTR, TOK_TYPE_AIM, TOK_TYPE_CST,
        TOK_TYPE_CNF, TOK_TYPE_BND, TOK_TYPE_LST, TOK_TYPE_MAP, TOK_TYPE_DAT,
        TOK_TYPE_ENM, TOK_TYPE_TUP, TOK_TYPE_AGT, TOK_TYPE_CH,
        // RESULT TYPE
        TOK_RESULT_OP,
        // MEMORY TOKENS
        TOK_MEM_WORK, TOK_MEM_EP, TOK_MEM_SEM,
        // STDLIB TOKENS
        TOK_AT_BUDGET, TOK_AT_TRACE, TOK_AT_CORRECT, TOK_AT_PRUNE, TOK_AT_SNAP,
        TOK_AT_RESTORE, TOK_AT_ALLOC, TOK_AT_FREE, TOK_AT_SPN, TOK_AT_KIL,
        TOK_AT_SND, TOK_AT_RCV, TOK_AT_SYN, TOK_AT_PAR, TOK_AT_POOL,
        TOK_AT_VOUCH, TOK_AT_COMPRESS, TOK_AT_CHUNK, TOK_AT_SLIDE, TOK_AT_PROF, TOK_AT_MEM,
        // LITERAL TOKENS
        TOK_INT_LIT, TOK_UINT_LIT, TOK_FLOAT_LIT, TOK_BOOL_TRUE, TOK_BOOL_FALSE, TOK_STR_LIT,
        // OPERATOR TOKENS
        TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT,
        TOK_LEQ, TOK_GEQ, TOK_AND, TOK_OR, TOK_NOT, TOK_PIPE, TOK_PROPAGATE,
        TOK_IMPORT, TOK_BIND,
        // STRUCTURAL TOKENS
        TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
        TOK_COMMA, TOK_SEMICOLON, TOK_DOT,
        // META TOKENS
        TOK_IDENT, TOK_EOF, TOK_ERROR
    };

    Kind        kind;
    std::string value;     
    size_t      line;      
    size_t      col;       
    size_t      offset;    
};

std::string token_to_json(const Token& t);
std::string token_kind_to_string(Token::Kind kind);
std::string error_to_json(const LexError& e);
bool is_type_token(Token::Kind k);

} // namespace agentc
