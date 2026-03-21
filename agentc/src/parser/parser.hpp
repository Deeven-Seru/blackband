#pragma once
#include "ast.hpp"
#include "../lexer/lexer.hpp"
#include <vector>
#include <string_view>

namespace agentc {

struct ParseError {
    std::string code;
    size_t line;
    size_t col;
    std::string cause;
    std::string fix;
    std::vector<std::string> alts;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens, std::string_view filename);

    ProgramNode     parse();
    bool            has_errors() const;
    std::string     errors_as_json() const;
    const std::vector<ParseError>& get_errors() const { return errors_; }

private:
    std::vector<Token>  tokens_;
    size_t              pos_ = 0;
    std::string_view    filename_;
    std::vector<ParseError> errors_;
    Token               eof_tok_ {Token::Kind::TOK_EOF, "", 0, 0, 0};

    const Token&    current() const;
    const Token&    peek(size_t offset = 1) const;
    Token           advance();
    bool            check(Token::Kind kind) const;
    bool            match(Token::Kind kind);
    bool            match(std::initializer_list<Token::Kind> kinds);
    Token           expect(Token::Kind kind, std::string_view msg);
    bool            at_end() const;

    void            emit_error(std::string code, std::string cause, std::string fix, std::vector<std::string> alts);
    void            synchronize();

    ProgramNode     parse_program();
    ImportNode      parse_import();
    TopLevelNode    parse_top_level();
    FnNode          parse_fn(AnnotationBlock annos);
    AgtNode         parse_agent(AnnotationBlock annos);
    DatNode         parse_dat();
    EnmNode         parse_enm();

    AnnotationBlock parse_annotation_block();
    AnnotationNode  parse_annotation();

    TypeNode        parse_type();
    TypeNode        parse_type_primary();
    TypeNode        parse_type_result(TypeNode ok);
    TypeNode        parse_type_pipe(TypeNode base);

    StmtNode        parse_stmt();
    StmtNode        parse_block();
    StmtNode        parse_binding();
    StmtNode        parse_if();
    StmtNode        parse_loop();
    StmtNode        parse_for_in();
    StmtNode        parse_match();

    ExprNode        parse_expr();
    ExprNode        parse_expr_prec(int min_prec);
    ExprNode        parse_unary();
    ExprNode        parse_primary();
    ExprNode        parse_call(ExprNode callee);
    ExprNode        parse_field(ExprNode obj);
    ExprNode        parse_at_primitive();

    std::vector<ParamNode>  parse_params();
    ParamNode               parse_param();
    std::vector<ExprNode>   parse_args();
    PatternNode             parse_pattern();
};

} // namespace agentc
