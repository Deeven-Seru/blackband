#include "lexer.hpp"
#include <cctype>
#include <unordered_map>
#include <iostream>

namespace agentc {

Lexer::Lexer(std::string_view source, std::string_view filename) 
    : source_(source), filename_(filename) {}

bool Lexer::at_end() const { return pos_ >= source_.length(); }
char Lexer::current() const { return at_end() ? '\0' : source_[pos_]; }
char Lexer::peek_char(size_t offset) const { return (pos_ + offset >= source_.length()) ? '\0' : source_[pos_ + offset]; }

char Lexer::advance() {
    char c = current();
    pos_++; col_++;
    if (c == '\n') { line_++; col_ = 1; }
    return c;
}

bool Lexer::match(char expected) {
    if (at_end() || current() != expected) return false;
    advance(); return true;
}

bool Lexer::match(std::string_view expected) {
    if (pos_ + expected.length() > source_.length()) return false;
    if (source_.substr(pos_, expected.length()) == expected) {
        for(size_t i=0; i<expected.length(); ++i) advance();
        return true;
    }
    return false;
}

void Lexer::skip_whitespace() {
    while (!at_end()) {
        char c = current();
        if (std::isspace(static_cast<unsigned char>(c))) { advance(); }
        else if ((unsigned char)c == 0xC2 && (unsigned char)peek_char() == 0xA0) { advance(); advance(); }
        else if (c == '/' && peek_char() == '/') { while (!at_end() && current() != '\n') advance(); }
        else break;
    }
}

void Lexer::skip_comment() {
    // Handled in whitespace logic generically
}

Token Lexer::make_token(Token::Kind kind, std::string value) {
    last_tok_ = {kind, std::move(value), line_, col_ - value.length(), pos_ - value.length()};
    return last_tok_;
}

Token Lexer::make_error(std::string message) {
    errors_.push_back({"E101", "Syntax", line_, col_, "valid token", std::string(1, current()), "remove token", message, "check specs"});
    advance();
    return make_token(Token::Kind::TOK_ERROR, "ERROR");
}

std::string Lexer::errors_as_json() const {
    std::string out = "[\n";
    for(size_t i=0; i<errors_.size(); ++i) {
        out += "  " + error_to_json(errors_[i]);
        if (i < errors_.size() - 1) out += ",";
        out += "\n";
    }
    out += "]";
    return out;
}

bool Lexer::has_errors() const { return !errors_.empty(); }

Token Lexer::peek() { return peek(1); }

Token Lexer::peek(size_t n) {
    while(peek_buffer_.size() < n) {
        size_t saved_pos = pos_, saved_line = line_, saved_col = col_;
        Token t = scan_next_internal();
        peek_buffer_.push_back(t);
        pos_ = saved_pos; line_ = saved_line; col_ = saved_col;
    }
    return peek_buffer_[n-1];
}

Token Lexer::next() {
    if (!peek_buffer_.empty()) {
        Token t = peek_buffer_.front();
        peek_buffer_.erase(peek_buffer_.begin());
        pos_ += t.value.length();
        col_ += t.value.length(); 
        if(t.kind == Token::Kind::TOK_EOF) col_ = t.col;
        last_tok_ = t;
        return t;
    }
    return scan_next_internal();
}

Token Lexer::scan_next_internal() {
    skip_whitespace();
    if (at_end()) return make_token(Token::Kind::TOK_EOF, "");

    char c = current();

    // UTF-8 explicitly constrained for ƒ symbol (U+0192) c6 92
    if ((unsigned char)c == 0xC6 && (unsigned char)peek_char() == 0x92) {
        advance(); advance();
        return make_token(Token::Kind::TOK_FUNC, "ƒ");
    }
    
    // Explicit ⊥ symbol
    if ((unsigned char)c == 0xE2 && (unsigned char)peek_char() == 0x8A && (unsigned char)peek_char(2) == 0xA5) {
        advance(); advance(); advance();
        return make_token(Token::Kind::TOK_TYPE_NEVER, "⊥");
    }

    if (c == '@') {
        if (peek_char() == '(') {
            std::string val = "@("; advance(); advance();
            return make_token(Token::Kind::TOK_AT_ITER, val);
        }
        return scan_at_primitive();
    }
    
    if (c == '#') return scan_annotation();
    if (c == '"') return scan_string_literal();
    
    if (std::isdigit(c)) return scan_number_literal();
    if (std::isalpha(c) || c == '_') return scan_identifier_or_keyword();

    return scan_symbol();
}

Token Lexer::scan_at_primitive() {
    std::string val; val += advance();
    while (std::isalpha(current())) val += advance();
    
    if (val == "@work") return make_token(Token::Kind::TOK_MEM_WORK, val);
    if (val == "@ep") return make_token(Token::Kind::TOK_MEM_EP, val);
    if (val == "@sem") return make_token(Token::Kind::TOK_MEM_SEM, val);
    if (val == "@budget") return make_token(Token::Kind::TOK_AT_BUDGET, val);
    if (val == "@trace") return make_token(Token::Kind::TOK_AT_TRACE, val);
    if (val == "@correct") return make_token(Token::Kind::TOK_AT_CORRECT, val);
    if (val == "@prune") return make_token(Token::Kind::TOK_AT_PRUNE, val);
    if (val == "@snapshot") return make_token(Token::Kind::TOK_AT_SNAP, val);
    if (val == "@restore") return make_token(Token::Kind::TOK_AT_RESTORE, val);
    if (val == "@alloc") return make_token(Token::Kind::TOK_AT_ALLOC, val);
    if (val == "@free") return make_token(Token::Kind::TOK_AT_FREE, val);
    if (val == "@spn") return make_token(Token::Kind::TOK_AT_SPN, val);
    if (val == "@kil") return make_token(Token::Kind::TOK_AT_KIL, val);
    if (val == "@snd") return make_token(Token::Kind::TOK_AT_SND, val);
    if (val == "@rcv") return make_token(Token::Kind::TOK_AT_RCV, val);
    if (val == "@syn") return make_token(Token::Kind::TOK_AT_SYN, val);
    if (val == "@par") return make_token(Token::Kind::TOK_AT_PAR, val);
    if (val == "@pool") return make_token(Token::Kind::TOK_AT_POOL, val);
    if (val == "@vouch") return make_token(Token::Kind::TOK_AT_VOUCH, val);
    if (val == "@compress") return make_token(Token::Kind::TOK_AT_COMPRESS, val);
    if (val == "@chunk") return make_token(Token::Kind::TOK_AT_CHUNK, val);
    if (val == "@slide") return make_token(Token::Kind::TOK_AT_SLIDE, val);
    if (val == "@prof") return make_token(Token::Kind::TOK_AT_PROF, val);
    if (val == "@mem") return make_token(Token::Kind::TOK_AT_MEM, val);
    return make_error("Unknown at-primitive");
}

Token Lexer::scan_annotation() {
    std::string val; val += advance();
    char n = current();
    if (n == '[') { val += advance(); return make_token(Token::Kind::TOK_ANN_OPEN, val); }
    if (n == '>') { val += advance(); return make_token(Token::Kind::TOK_ANN_INTENT, val); }
    if (n == '$') { val += advance(); return make_token(Token::Kind::TOK_ANN_COST, val); }
    if (n == '!') { val += advance(); return make_token(Token::Kind::TOK_ANN_EFFECT, val); }
    if (n == '@') { val += advance(); return make_token(Token::Kind::TOK_ANN_VERIFY, val); }
    if (n == '~') { val += advance(); return make_token(Token::Kind::TOK_ANN_TRUST, val); }
    if (n == '?') { val += advance(); return make_token(Token::Kind::TOK_ANN_CONF, val); }
    if (n == '*') { val += advance(); return make_token(Token::Kind::TOK_ANN_RETRY, val); }
    if (n == '^') { val += advance(); return make_token(Token::Kind::TOK_ANN_GOAL, val); }
    if (n == '&') { val += advance(); return make_token(Token::Kind::TOK_ANN_CAP, val); }
    return make_error("Invalid annotation prefix");
}

Token Lexer::scan_number_literal() {
    std::string val;
    bool is_float = false;
    while (std::isdigit(current()) || current() == '.') {
        if (current() == '.') is_float = true;
        val += advance();
    }
    if (current() == 'i') { val += advance(); return make_token(Token::Kind::TOK_INT_LIT, val); }
    if (current() == 'u') { val += advance(); return make_token(Token::Kind::TOK_UINT_LIT, val); }
    if (current() == 'f') { val += advance(); return make_token(Token::Kind::TOK_FLOAT_LIT, val); }
    if (current() == 'b') { val += advance(); return make_token(val == "1b" ? Token::Kind::TOK_BOOL_TRUE : Token::Kind::TOK_BOOL_FALSE, val); }
    return make_token(is_float ? Token::Kind::TOK_FLOAT_LIT : Token::Kind::TOK_INT_LIT, val);
}

Token Lexer::scan_string_literal() {
    std::string val; val += advance(); // Open quote
    while (!at_end() && current() != '"') {
        if (current() == '\\') val += advance();
        val += advance();
    }
    if (match('"')) val += '"';
    return make_token(Token::Kind::TOK_STR_LIT, val);
}

Token Lexer::scan_identifier_or_keyword() {
    std::string val;
    while (std::isalnum(current()) || current() == '_') val += advance();
    
    static const std::unordered_map<std::string, Token::Kind> kw = {
        {"i8", Token::Kind::TOK_TYPE_I8}, {"i16", Token::Kind::TOK_TYPE_I16}, {"i32", Token::Kind::TOK_TYPE_I32}, {"i64", Token::Kind::TOK_TYPE_I64}, {"i128", Token::Kind::TOK_TYPE_I128},
        {"u8", Token::Kind::TOK_TYPE_U8}, {"u16", Token::Kind::TOK_TYPE_U16}, {"u32", Token::Kind::TOK_TYPE_U32}, {"u64", Token::Kind::TOK_TYPE_U64}, {"u128", Token::Kind::TOK_TYPE_U128},
        {"f32", Token::Kind::TOK_TYPE_F32}, {"f64", Token::Kind::TOK_TYPE_F64}, {"B", Token::Kind::TOK_TYPE_B}, {"Str", Token::Kind::TOK_TYPE_STR},
        {"Own", Token::Kind::TOK_TYPE_OWN}, {"Ref", Token::Kind::TOK_TYPE_REF}, {"Shr", Token::Kind::TOK_TYPE_SHR}, {"Opt", Token::Kind::TOK_TYPE_OPT},
        {"Tru", Token::Kind::TOK_TYPE_TRU}, {"Utr", Token::Kind::TOK_TYPE_UTR}, {"Aim", Token::Kind::TOK_TYPE_AIM}, {"Cst", Token::Kind::TOK_TYPE_CST},
        {"Cnf", Token::Kind::TOK_TYPE_CNF}, {"Bnd", Token::Kind::TOK_TYPE_BND}, {"Lst", Token::Kind::TOK_TYPE_LST}, {"Map", Token::Kind::TOK_TYPE_MAP},
        {"Dat", Token::Kind::TOK_TYPE_DAT}, {"Enm", Token::Kind::TOK_TYPE_ENM}, {"Tup", Token::Kind::TOK_TYPE_TUP}, {"Agt", Token::Kind::TOK_TYPE_AGT},
        {"Ch", Token::Kind::TOK_TYPE_CH}, {"work", Token::Kind::TOK_MEM_WORK}, {"ep", Token::Kind::TOK_MEM_EP}, {"sem", Token::Kind::TOK_MEM_SEM}
    };
    if (auto it = kw.find(val); it != kw.end()) return make_token(it->second, val);
    return make_token(Token::Kind::TOK_IDENT, val);
}

Token Lexer::scan_symbol() {
    std::string val; val += advance();
    char nx = current();
    
    if (val[0] == '(' && nx == ')') { val += advance(); return make_token(Token::Kind::TOK_TYPE_UNIT, val); }
    if (val[0] == ':' && nx == '=') { val += advance(); return make_token(Token::Kind::TOK_WALRUS, val); }
    if (val[0] == ':' && nx == ':') { val += advance(); return make_token(Token::Kind::TOK_BIND, val); }
    if (val[0] == '-' && nx == '>') { val += advance(); return make_token(Token::Kind::TOK_ARROW, val); }
    if (val[0] == '<' && nx == '-') { val += advance(); return make_token(Token::Kind::TOK_LARROW, val); }
    if (val[0] == '>' && nx == '>') { val += advance(); return make_token(Token::Kind::TOK_DBLGT, val); }
    if (val[0] == '^' && nx == '!') { val += advance(); return make_token(Token::Kind::TOK_CARET_BANG, val); }
    if (val[0] == '+' && nx == '>') { val += advance(); return make_token(Token::Kind::TOK_IMPORT, val); }
    if (val[0] == '=' && nx == '=') { val += advance(); return make_token(Token::Kind::TOK_EQ, val); }
    if (val[0] == '!' && nx == '=') { val += advance(); return make_token(Token::Kind::TOK_NEQ, val); }
    if (val[0] == '<' && nx == '=') { val += advance(); return make_token(Token::Kind::TOK_LEQ, val); }
    if (val[0] == '>' && nx == '=') { val += advance(); return make_token(Token::Kind::TOK_GEQ, val); }
    
    Token::Kind k = Token::Kind::TOK_ERROR;
    switch(val[0]) {
        case '=': k = Token::Kind::TOK_EQ; break;
        case '$': k = Token::Kind::TOK_DOLLAR; break;
        case '~': k = Token::Kind::TOK_TILDE; break;
        case '^': k = Token::Kind::TOK_CARET; break;
        case ':': k = Token::Kind::TOK_COLON; break;
        case '+': k = Token::Kind::TOK_PLUS; break;
        case '-': k = Token::Kind::TOK_MINUS; break;
        case '*': k = Token::Kind::TOK_STAR; break;
        case '/': k = Token::Kind::TOK_SLASH; break;
        case '<': k = Token::Kind::TOK_LT; break;
        case '>': k = Token::Kind::TOK_GT; break;
        case '&': k = Token::Kind::TOK_AND; break;
        case '|': k = Token::Kind::TOK_PIPE; break;
        case '!': k = Token::Kind::TOK_NOT; break;
        case '{': k = Token::Kind::TOK_LBRACE; break;
        case '}': k = Token::Kind::TOK_RBRACE; break;
        case '(': k = Token::Kind::TOK_LPAREN; break;
        case ')': k = Token::Kind::TOK_RPAREN; break;
        case '[': k = Token::Kind::TOK_LBRACKET; break;
        case ']': k = Token::Kind::TOK_ANN_CLOSE; break;
        case ',': k = Token::Kind::TOK_COMMA; break;
        case ';': k = Token::Kind::TOK_SEMICOLON; break;
        case '.': k = Token::Kind::TOK_DOT; break;
        case '?': {
            if (is_type_token(last_tok_.kind)) { k = Token::Kind::TOK_RESULT_OP; break; }
            if (last_tok_.kind == Token::Kind::TOK_IDENT || last_tok_.kind == Token::Kind::TOK_RPAREN || last_tok_.kind == Token::Kind::TOK_RBRACKET) {
                k = Token::Kind::TOK_PROPAGATE; break;
            }
            k = Token::Kind::TOK_QUESTION;
            break;
        }
        default: return make_error("Unknown symbol");
    }
    
    return make_token(k, val);
}

} // namespace agentc
