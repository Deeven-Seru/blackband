#pragma once
#include "token.hpp"
#include <string_view>
#include <vector>

namespace agentc {

class Lexer {
public:
    explicit Lexer(std::string_view source, std::string_view filename);
    
    Token           next();
    Token           peek();
    Token           peek(size_t n);
    bool            at_end() const;
    std::string     errors_as_json() const;
    bool            has_errors() const;
    const std::vector<LexError>& get_errors() const { return errors_; }
    
private:
    std::string_view source_;
    std::string_view filename_;
    size_t           pos_    = 0;
    size_t           line_   = 1;
    size_t           col_    = 1;
    std::vector<LexError> errors_;
    Token            last_tok_ { Token::Kind::TOK_ERROR, "", 0, 0, 0 };
    std::vector<Token> peek_buffer_;
    
    char        current() const;
    char        advance();
    char        peek_char(size_t offset = 1) const;
    bool        match(char expected);
    bool        match(std::string_view expected);
    void        skip_whitespace();
    void        skip_comment();
    
    Token       scan_next_internal();
    Token       scan_symbol();
    Token       scan_annotation();
    Token       scan_at_primitive();
    Token       scan_identifier_or_keyword();
    Token       scan_number_literal();
    Token       scan_string_literal();
    
    Token       make_token(Token::Kind kind, std::string value);
    Token       make_error(std::string message);
};

} // namespace agentc
