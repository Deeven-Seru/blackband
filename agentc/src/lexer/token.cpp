#include "token.hpp"
#include <sstream>

namespace agentc {

std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\n') o << "\\n";
        else if (c == '\t') o << "\\t";
        else if (c == '\r') o << "\\r";
        else o << c;
    }
    return o.str();
}

std::string token_kind_to_string(Token::Kind kind) {
    #define MATCH(k) case Token::Kind::k: return #k;
    switch(kind) {
        MATCH(TOK_DOLLAR) MATCH(TOK_TILDE) MATCH(TOK_WALRUS)
        MATCH(TOK_FUNC) MATCH(TOK_ARROW) MATCH(TOK_CARET) MATCH(TOK_CARET_BANG)
        MATCH(TOK_QUESTION) MATCH(TOK_COLON) MATCH(TOK_AT_ITER) MATCH(TOK_LARROW) MATCH(TOK_DBLGT)
        MATCH(TOK_ANN_OPEN) MATCH(TOK_ANN_CLOSE) MATCH(TOK_ANN_INTENT) MATCH(TOK_ANN_COST)
        MATCH(TOK_ANN_EFFECT) MATCH(TOK_ANN_VERIFY) MATCH(TOK_ANN_TRUST) MATCH(TOK_ANN_CONF)
        MATCH(TOK_ANN_RETRY) MATCH(TOK_ANN_GOAL) MATCH(TOK_ANN_CAP)
        MATCH(TOK_TYPE_I8) MATCH(TOK_TYPE_I16) MATCH(TOK_TYPE_I32) MATCH(TOK_TYPE_I64) MATCH(TOK_TYPE_I128)
        MATCH(TOK_TYPE_U8) MATCH(TOK_TYPE_U16) MATCH(TOK_TYPE_U32) MATCH(TOK_TYPE_U64) MATCH(TOK_TYPE_U128)
        MATCH(TOK_TYPE_F32) MATCH(TOK_TYPE_F64) MATCH(TOK_TYPE_B) MATCH(TOK_TYPE_STR)
        MATCH(TOK_TYPE_UNIT) MATCH(TOK_TYPE_NEVER) MATCH(TOK_TYPE_OWN) MATCH(TOK_TYPE_REF) MATCH(TOK_TYPE_SHR)
        MATCH(TOK_TYPE_OPT) MATCH(TOK_TYPE_TRU) MATCH(TOK_TYPE_UTR) MATCH(TOK_TYPE_AIM) MATCH(TOK_TYPE_CST)
        MATCH(TOK_TYPE_CNF) MATCH(TOK_TYPE_BND) MATCH(TOK_TYPE_LST) MATCH(TOK_TYPE_MAP) MATCH(TOK_TYPE_DAT)
        MATCH(TOK_TYPE_ENM) MATCH(TOK_TYPE_TUP) MATCH(TOK_TYPE_AGT) MATCH(TOK_TYPE_CH)
        MATCH(TOK_RESULT_OP) MATCH(TOK_MEM_WORK) MATCH(TOK_MEM_EP) MATCH(TOK_MEM_SEM)
        MATCH(TOK_AT_BUDGET) MATCH(TOK_AT_TRACE) MATCH(TOK_AT_CORRECT) MATCH(TOK_AT_PRUNE) MATCH(TOK_AT_SNAP)
        MATCH(TOK_AT_RESTORE) MATCH(TOK_AT_ALLOC) MATCH(TOK_AT_FREE) MATCH(TOK_AT_SPN) MATCH(TOK_AT_KIL)
        MATCH(TOK_AT_SND) MATCH(TOK_AT_RCV) MATCH(TOK_AT_SYN) MATCH(TOK_AT_PAR) MATCH(TOK_AT_POOL)
        MATCH(TOK_AT_VOUCH) MATCH(TOK_AT_COMPRESS) MATCH(TOK_AT_CHUNK) MATCH(TOK_AT_SLIDE) MATCH(TOK_AT_PROF) MATCH(TOK_AT_MEM)
        MATCH(TOK_INT_LIT) MATCH(TOK_UINT_LIT) MATCH(TOK_FLOAT_LIT) MATCH(TOK_BOOL_TRUE) MATCH(TOK_BOOL_FALSE) MATCH(TOK_STR_LIT)
        MATCH(TOK_PLUS) MATCH(TOK_MINUS) MATCH(TOK_STAR) MATCH(TOK_SLASH) MATCH(TOK_EQ) MATCH(TOK_NEQ) MATCH(TOK_LT) MATCH(TOK_GT)
        MATCH(TOK_LEQ) MATCH(TOK_GEQ) MATCH(TOK_AND) MATCH(TOK_OR) MATCH(TOK_NOT) MATCH(TOK_PIPE) MATCH(TOK_PROPAGATE)
        MATCH(TOK_IMPORT) MATCH(TOK_BIND)
        MATCH(TOK_LBRACE) MATCH(TOK_RBRACE) MATCH(TOK_LPAREN) MATCH(TOK_RPAREN) MATCH(TOK_LBRACKET) MATCH(TOK_RBRACKET)
        MATCH(TOK_COMMA) MATCH(TOK_SEMICOLON) MATCH(TOK_DOT)
        MATCH(TOK_IDENT) MATCH(TOK_EOF) MATCH(TOK_ERROR)
    }
    return "UNKNOWN";
}

bool is_type_token(Token::Kind k) {
    return k >= Token::Kind::TOK_TYPE_I8 && k <= Token::Kind::TOK_TYPE_CH;
}

std::string token_to_json(const Token& t) {
    std::ostringstream o;
    o << "{\"kind\":\"" << token_kind_to_string(t.kind) << "\", \"value\":\"" 
      << escape_json(t.value) << "\", \"line\":" << t.line << ", \"col\":" << t.col 
      << ", \"offset\":" << t.offset << "}";
    return o.str();
}

std::string error_to_json(const LexError& e) {
    std::ostringstream o;
    o << "{\n"
      << "  \"err\": \"" << e.err << "\",\n"
      << "  \"cat\": \"" << e.cat << "\",\n"
      << "  \"loc\": [" << e.line << ", " << e.col << "],\n"
      << "  \"why\": { \"got\": \"" << escape_json(e.got) << "\", \"expected\": \"" << escape_json(e.expected) << "\" },\n"
      << "  \"fix\": { \"act\": \"" << escape_json(e.act) << "\", \"via\": \"" << escape_json(e.via) << "\" },\n"
      << "  \"alt\": [\"" << escape_json(e.alt) << "\"]\n"
      << "}";
    return o.str();
}

} // namespace agentc
