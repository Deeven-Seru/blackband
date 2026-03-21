#include "parser.hpp"
#include <sstream>
#include "../toolchain/pkg.hpp"

namespace agentc {

Parser::Parser(std::vector<Token> tokens, std::string_view filename)
    : tokens_(std::move(tokens)), filename_(filename) {}

bool Parser::has_errors() const { return !errors_.empty(); }

std::string Parser::errors_as_json() const {
    std::ostringstream o; o << "[\n";
    for (size_t i = 0; i < errors_.size(); ++i) {
        auto& e = errors_[i];
        o << "  {\"err\":\"" << e.code << "\", \"loc\":[" << e.line << "," << e.col << "], \"why\":{\"cause\":\"" << e.cause << "\"}, \"fix\":{\"act\":\"" << e.fix << "\"}}";
        if (i < errors_.size() - 1) o << ",";
        o << "\n";
    }
    return o.str() + "]";
}

const Token& Parser::current() const { return pos_ < tokens_.size() ? tokens_[pos_] : eof_tok_; }
const Token& Parser::peek(size_t offset) const { return (pos_ + offset) < tokens_.size() ? tokens_[pos_ + offset] : eof_tok_; }
Token Parser::advance() { Token t = current(); pos_++; return t; }
bool Parser::check(Token::Kind kind) const { return current().kind == kind; }
bool Parser::match(Token::Kind kind) { if (check(kind)) { advance(); return true; } return false; }
bool Parser::at_end() const { return current().kind == Token::Kind::TOK_EOF; }

Token Parser::expect(Token::Kind kind, std::string_view msg) {
    if (check(kind)) return advance();
    emit_error("E102", "Unexpected Token", std::string(msg), {});
    return current(); // Return invalid for progression
}

void Parser::emit_error(const std::string& code, const std::string& cause, const std::string& fix, const std::vector<std::string>& alts) {
    errors_.push_back({code, current().line, current().col, cause, fix, alts});
}

void Parser::synchronize() {
    advance();
    while (!at_end()) {
        if (check(Token::Kind::TOK_FUNC) || check(Token::Kind::TOK_ANN_OPEN)) return;
        advance();
    }
}

ProgramNode Parser::parse() { return parse_program(); }

ProgramNode Parser::parse_program() {
    ProgramNode p; p.loc = SourceLoc{current().line, current().col, current().offset};
    while (!at_end()) {
        try {
            if (check(Token::Kind::TOK_IMPORT)) p.imports.push_back(parse_import(p.decls));
            else p.decls.push_back(parse_top_level());
        } catch(...) { synchronize(); }
    }
    return p;
}



ImportNode Parser::parse_import(std::vector<TopLevelNode>& target_decls) {
    ImportNode n; n.loc = SourceLoc{current().line, current().col, current().offset};
    expect(Token::Kind::TOK_IMPORT, "Expected +>");
    
    if (current().kind == Token::Kind::TOK_STR_LIT) {
        n.module_path = advance().value;
        n.is_remote = true;
        expect(Token::Kind::TOK_SEMICOLON, "Expected ;");

        // Dynamically resolve package via native PackageManager
        toolchain::PackageManager pkg;
        std::string raw_source = pkg.resolve(n.module_path);
        
        if (!raw_source.empty()) {
            // Recursive AST extraction
            Lexer sub_lexer(raw_source, n.module_path);
            std::vector<Token> sub_tokens;
            while (!sub_lexer.at_end()) {
                auto t = sub_lexer.next(); sub_tokens.push_back(t);
                if (t.kind == Token::Kind::TOK_EOF) break;
            }
            if (!sub_lexer.has_errors()) {
                Parser sub_parser(std::move(sub_tokens), n.module_path);
                auto sub_ast = sub_parser.parse();
                if (!sub_parser.has_errors()) {
                    for (auto& decl : sub_ast.decls) {
                        target_decls.push_back(std::move(decl));
                    }
                } else {
                    emit_error("E110", "Package Module Error", "Fix underlying module syntax", {});
                }
            } else {
                emit_error("E110", "Package Lexing Error", "Corrupt remote module", {});
            }
        }
        return n;
    } 

    while(current().kind == Token::Kind::TOK_IDENT) { n.module_path += advance().value; if (match(Token::Kind::TOK_BIND)) n.module_path += "::"; else break; }
    expect(Token::Kind::TOK_SEMICOLON, "Expected ;");
    return n;
}

TopLevelNode Parser::parse_top_level() {
    AnnotationBlock annos;
    if (check(Token::Kind::TOK_ANN_OPEN)) annos = parse_annotation_block();

    if (check(Token::Kind::TOK_FUNC)) return parse_fn(std::move(annos));
    if (check(Token::Kind::TOK_TYPE_AGT)) return parse_agent(std::move(annos));
    if (check(Token::Kind::TOK_TYPE_DAT)) return parse_dat();
    if (check(Token::Kind::TOK_TYPE_ENM)) return parse_enm();

    emit_error("E104", "Expected Top Level Decl", "Provide ƒ or Agt", {});
    advance();
    throw 1;
}

AnnotationBlock Parser::parse_annotation_block() {
    AnnotationBlock b; b.loc = SourceLoc{current().line, current().col, current().offset};
    expect(Token::Kind::TOK_ANN_OPEN, "Expected #[");
    while(!check(Token::Kind::TOK_ANN_CLOSE) && !at_end()) { b.annotations.push_back(parse_annotation()); }
    expect(Token::Kind::TOK_ANN_CLOSE, "Expected ]");
    return b;
}

AnnotationNode Parser::parse_annotation() {
    AnnotationNode n; n.loc = SourceLoc{current().line, current().col, current().offset};
    if (match(Token::Kind::TOK_ANN_INTENT)) { n.kind = AnnotationNode::Kind::Intent; n.value = expect(Token::Kind::TOK_STR_LIT, "Expected String").value; }
    else if (match(Token::Kind::TOK_ANN_COST)) { 
        n.kind = AnnotationNode::Kind::Cost; expect(Token::Kind::TOK_LPAREN, "Expected ("); 
        std::vector<std::string> args;
        while (!check(Token::Kind::TOK_RPAREN) && !at_end()) {
            std::string arg = expect(Token::Kind::TOK_IDENT, "Expected cost key").value;
            if (match(Token::Kind::TOK_COLON)) {
                arg += ":" + expect(Token::Kind::TOK_INT_LIT, "Expected cost value").value;
            }
            args.push_back(arg);
            if (!match(Token::Kind::TOK_PIPE)) break;
        }
        n.value = args;
        expect(Token::Kind::TOK_RPAREN, "Expected )"); 
    }
    else if (match(Token::Kind::TOK_ANN_EFFECT)) { 
        n.kind = AnnotationNode::Kind::Effect; expect(Token::Kind::TOK_LPAREN, "Expected ("); 
        std::vector<std::string> flags;
        while (!check(Token::Kind::TOK_RPAREN) && !at_end()) {
            flags.push_back(expect(Token::Kind::TOK_IDENT, "Expected effect flag").value);
            if (!match(Token::Kind::TOK_PIPE)) break;
        }
        n.value = flags;
        expect(Token::Kind::TOK_RPAREN, "Expected )"); 
    }
    else if (match(Token::Kind::TOK_ANN_TRUST)) { n.kind = AnnotationNode::Kind::Trust; expect(Token::Kind::TOK_LPAREN, "Expected ("); advance(); expect(Token::Kind::TOK_RPAREN, "Expected )"); }
    else if (match(Token::Kind::TOK_ANN_VERIFY)) { n.kind = AnnotationNode::Kind::Verify; expect(Token::Kind::TOK_LPAREN, "Expected ("); std::string verify_kw = expect(Token::Kind::TOK_IDENT, "Expected cond").value; expect(Token::Kind::TOK_COLON, ": expected"); auto e = parse_expr(); n.value = std::make_pair(verify_kw, std::make_unique<ExprNode>(std::move(e))); expect(Token::Kind::TOK_RPAREN, "Expected )"); }
    else if (match(Token::Kind::TOK_ANN_CONF)) { n.kind = AnnotationNode::Kind::Confidence; expect(Token::Kind::TOK_LPAREN, "Expected ("); std::string v = expect(Token::Kind::TOK_FLOAT_LIT, "Expected float").value; n.value = std::stod(v); expect(Token::Kind::TOK_RPAREN, "Expected )"); }
    else if (match(Token::Kind::TOK_ANN_RETRY)) { 
        n.kind = AnnotationNode::Kind::Retry; expect(Token::Kind::TOK_LPAREN, "Expected ("); 
        int retries = std::stoi(expect(Token::Kind::TOK_INT_LIT, "Expected int").value);
        if (match(Token::Kind::TOK_COLON)) {
            int backoff = std::stoi(expect(Token::Kind::TOK_INT_LIT, "Expected int").value);
            n.value = std::make_pair(retries, backoff);
        } else { n.value = std::make_pair(retries, 0); }
        expect(Token::Kind::TOK_RPAREN, "Expected )");
    }
    else if (match(Token::Kind::TOK_ANN_GOAL)) { n.kind = AnnotationNode::Kind::Goal; expect(Token::Kind::TOK_LPAREN, "Expected ("); n.value = expect(Token::Kind::TOK_STR_LIT, "Expected string").value; expect(Token::Kind::TOK_RPAREN, "Expected )"); }
    else if (match(Token::Kind::TOK_ANN_CAP)) {
        n.kind = AnnotationNode::Kind::Capability; expect(Token::Kind::TOK_LPAREN, "Expected (");
        std::vector<std::string> caps;
        while (!check(Token::Kind::TOK_RPAREN) && !at_end()) {
            caps.push_back(expect(Token::Kind::TOK_IDENT, "Expected capability").value);
            if (!match(Token::Kind::TOK_PIPE)) break;
        }
        n.value = caps;
        expect(Token::Kind::TOK_RPAREN, "Expected )");
    }
    else if (match(Token::Kind::TOK_IDENT)) {
        std::string key = peek(-1).value;
        if (key == "trc") {
            n.kind = AnnotationNode::Kind::Trace; expect(Token::Kind::TOK_COLON, "Expected :");
            n.value = expect(Token::Kind::TOK_IDENT, "Expected trace level").value; 
        } else if (key == "prot") {
            n.kind = AnnotationNode::Kind::Protocol; expect(Token::Kind::TOK_COLON, "Expected :");
            n.value = expect(Token::Kind::TOK_IDENT, "Expected protocol").value; 
        } else { emit_error("E101", "Unknown Annotation", "Remove or fix annotation", {}); advance(); }
    }
    else if (match(Token::Kind::TOK_ANN_FFI)) {
        n.kind = AnnotationNode::Kind::FFI; expect(Token::Kind::TOK_LPAREN, "Expected (");
        n.value = expect(Token::Kind::TOK_STR_LIT, "Expected string dialect").value;
        expect(Token::Kind::TOK_RPAREN, "Expected )");
    }
    else { advance(); } // Lazy boundary bypass for minimal LLM
    return n;
}

FnNode Parser::parse_fn(AnnotationBlock annos) {
    FnNode n; n.loc = SourceLoc{current().line, current().col, current().offset}; n.annotations = std::move(annos);
    expect(Token::Kind::TOK_FUNC, "Expected ƒ");
    if(!check(Token::Kind::TOK_IDENT)) { emit_error("E101", "Missing name", "Add identifier", {}); throw 1; }
    n.name = advance().value;
    n.params = parse_params();
    expect(Token::Kind::TOK_ARROW, "Expected ->");
    n.return_type = parse_type();

    if (match(Token::Kind::TOK_SEMICOLON)) {
        n.is_external = true;
        n.body = StmtNode();
        n.body.kind = StmtNode::Kind::BlockStmt;
    } else {
        n.body = parse_block();
    }
    return n;
}

std::vector<ParamNode> Parser::parse_params() {
    std::vector<ParamNode> p;
    if (match(Token::Kind::TOK_TYPE_UNIT)) return p;
    expect(Token::Kind::TOK_LPAREN, "Expected (");
    while(!check(Token::Kind::TOK_RPAREN) && !at_end()) { p.push_back(parse_param()); if(!match(Token::Kind::TOK_COMMA)) break; }
    expect(Token::Kind::TOK_RPAREN, "Expected )");
    return p;
}

ParamNode Parser::parse_param() {
    ParamNode p; p.loc = SourceLoc{current().line, current().col, current().offset};
    if(match(Token::Kind::TOK_DOLLAR)) p.is_mutable = false; else if(match(Token::Kind::TOK_TILDE)) p.is_mutable = true;
    p.name = expect(Token::Kind::TOK_IDENT, "Expected param name").value;
    expect(Token::Kind::TOK_COLON, "Expected :");
    p.type = parse_type();
    return p;
}

AgtNode Parser::parse_agent(AnnotationBlock annos) {
    AgtNode n; n.loc = SourceLoc{current().line, current().col, current().offset}; n.annotations = std::move(annos);
    expect(Token::Kind::TOK_TYPE_AGT, "Expected Agt");
    n.name = expect(Token::Kind::TOK_IDENT, "Expected Agent name").value;
    expect(Token::Kind::TOK_LBRACE, "Expected {");
    
    AnnotationBlock inner_annos;
    if (check(Token::Kind::TOK_ANN_OPEN)) inner_annos = parse_annotation_block();
    
    n.run_fn = parse_fn(std::move(inner_annos));
    expect(Token::Kind::TOK_RBRACE, "Expected }");
    return n;
}
DatNode Parser::parse_dat() {
    DatNode n; n.loc = SourceLoc{current().line, current().col, current().offset};
    expect(Token::Kind::TOK_TYPE_DAT, "Expected Dat");
    n.name = expect(Token::Kind::TOK_IDENT, "Expected struct name").value;
    expect(Token::Kind::TOK_LBRACE, "Expected {");
    while (!check(Token::Kind::TOK_RBRACE) && !at_end()) {
        DatFieldNode f; f.loc = SourceLoc{current().line, current().col, current().offset};
        f.name = expect(Token::Kind::TOK_IDENT, "Expected field name").value;
        expect(Token::Kind::TOK_COLON, "Expected :");
        f.type = parse_type();
        n.fields.push_back(std::move(f));
        match(Token::Kind::TOK_SEMICOLON); match(Token::Kind::TOK_COMMA);
    }
    expect(Token::Kind::TOK_RBRACE, "Expected }");
    return n;
}

EnmNode Parser::parse_enm() {
    EnmNode n; n.loc = SourceLoc{current().line, current().col, current().offset};
    expect(Token::Kind::TOK_TYPE_ENM, "Expected Enm");
    n.name = expect(Token::Kind::TOK_IDENT, "Expected enum name").value;
    expect(Token::Kind::TOK_LBRACE, "Expected {");
    while (!check(Token::Kind::TOK_RBRACE) && !at_end()) {
        EnmVariantNode v; v.loc = SourceLoc{current().line, current().col, current().offset};
        v.name = expect(Token::Kind::TOK_IDENT, "Expected variant name").value;
        if (match(Token::Kind::TOK_LPAREN)) {
            v.payload = std::make_unique<TypeNode>(parse_type());
            expect(Token::Kind::TOK_RPAREN, "Expected )");
        }
        n.variants.push_back(std::move(v));
        match(Token::Kind::TOK_COMMA); match(Token::Kind::TOK_SEMICOLON);
    }
    expect(Token::Kind::TOK_RBRACE, "Expected }");
    return n;
}

TypeNode Parser::parse_type() {
    TypeNode t = parse_type_primary();
    if (match(Token::Kind::TOK_RESULT_OP) || match(Token::Kind::TOK_QUESTION) || match(Token::Kind::TOK_PROPAGATE)) 
        return parse_type_result(std::move(t));
    return t;
}

TypeNode Parser::parse_type_result(TypeNode ok) {
    TypeNode t; t.loc = SourceLoc{current().line, current().col, current().offset}; t.kind = TypeNode::Kind::Result;
    t.ok_type = std::make_unique<TypeNode>(std::move(ok));
    t.err_type = std::make_unique<TypeNode>(parse_type_primary());
    return t;
}

TypeNode Parser::parse_type_primary() {
    TypeNode t; t.loc = SourceLoc{current().line, current().col, current().offset};
    if (match(Token::Kind::TOK_TYPE_I8)) { t.kind = TypeNode::Kind::I8; }
    else if (match(Token::Kind::TOK_TYPE_I16)) { t.kind = TypeNode::Kind::I16; }
    else if (match(Token::Kind::TOK_TYPE_I32)) { t.kind = TypeNode::Kind::I32; }
    else if (match(Token::Kind::TOK_TYPE_I64)) { t.kind = TypeNode::Kind::I64; }
    else if (match(Token::Kind::TOK_TYPE_I128)) { t.kind = TypeNode::Kind::I128; }
    else if (match(Token::Kind::TOK_TYPE_U8)) { t.kind = TypeNode::Kind::U8; }
    else if (match(Token::Kind::TOK_TYPE_U16)) { t.kind = TypeNode::Kind::U16; }
    else if (match(Token::Kind::TOK_TYPE_U32)) { t.kind = TypeNode::Kind::U32; }
    else if (match(Token::Kind::TOK_TYPE_U64)) { t.kind = TypeNode::Kind::U64; }
    else if (match(Token::Kind::TOK_TYPE_U128)) { t.kind = TypeNode::Kind::U128; }
    else if (match(Token::Kind::TOK_TYPE_F32)) { t.kind = TypeNode::Kind::F32; }
    else if (match(Token::Kind::TOK_TYPE_F64)) { t.kind = TypeNode::Kind::F64; }
    else if (match(Token::Kind::TOK_TYPE_B)) { t.kind = TypeNode::Kind::Bool; }
    else if (match(Token::Kind::TOK_TYPE_STR)) { 
        t.kind = TypeNode::Kind::Str; 
        if (match(Token::Kind::TOK_LT)) { 
            t.str_bound = std::stoull(expect(Token::Kind::TOK_INT_LIT, "Expected int").value); 
            expect(Token::Kind::TOK_GT, ">"); 
        } 
    }
    else if (match(Token::Kind::TOK_TYPE_TRU)) { t.kind = TypeNode::Kind::Tru; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_UTR)) { t.kind = TypeNode::Kind::Utr; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_TUP)) { t.kind = TypeNode::Kind::Tup; }
    else if (match(Token::Kind::TOK_TYPE_OWN)) { t.kind = TypeNode::Kind::Own; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_REF)) { t.kind = TypeNode::Kind::Ref; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_SHR)) { t.kind = TypeNode::Kind::Shr; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_OPT)) { t.kind = TypeNode::Kind::Opt; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_LST)) { t.kind = TypeNode::Kind::Lst; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_MAP)) { t.kind = TypeNode::Kind::Map; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_COMMA, ","); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_BND)) { 
        t.kind = TypeNode::Kind::Bnd; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_COMMA, ","); 
        t.bound_lo = std::make_unique<ExprNode>(parse_expr()); expect(Token::Kind::TOK_COMMA, ","); 
        t.bound_hi = std::make_unique<ExprNode>(parse_expr()); expect(Token::Kind::TOK_GT, ">"); 
    }
    else if (match(Token::Kind::TOK_TYPE_CH)) { t.kind = TypeNode::Kind::Ch; expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type_primary()); expect(Token::Kind::TOK_GT, ">"); }
    else if (match(Token::Kind::TOK_TYPE_UNIT)) { t.kind = TypeNode::Kind::Unit; }
    else if (match(Token::Kind::TOK_TYPE_NEVER)) { t.kind = TypeNode::Kind::Never; }
    else if (match(Token::Kind::TOK_IDENT)) { 
        std::string n = peek(-1).value;
        if (n == "AgtR") {
            t.kind = TypeNode::Kind::AgtResult;
            expect(Token::Kind::TOK_LT, "<"); t.params.push_back(parse_type()); expect(Token::Kind::TOK_GT, ">");
        } else if (n == "AgtResult") {
            t.kind = TypeNode::Kind::AgtResult;
        } else {
            t.kind = TypeNode::Kind::Named; t.name = n; 
        }
    }
    else { emit_error("E300", "Unknown type", "Use valid type", {}); advance(); }
    return t;
}

StmtNode Parser::parse_stmt() {
    StmtNode n; n.loc = SourceLoc{current().line, current().col, current().offset};
    if (match(Token::Kind::TOK_DOLLAR)) {
        // $ name: Type = expr;   OR   $ name := expr;  (LocalInfer)
        n.bind_name = expect(Token::Kind::TOK_IDENT, "name").value;
        if (check(Token::Kind::TOK_WALRUS)) {
            // $ name := expr;  — inferred immutable binding
            advance(); // consume :=
            n.kind = StmtNode::Kind::LocalInfer;
            n.bind_expr = std::make_unique<ExprNode>(parse_expr());
        } else {
            n.kind = StmtNode::Kind::LetBind;
            if(match(Token::Kind::TOK_COLON)) { n.bind_type = std::make_unique<TypeNode>(parse_type()); }
            expect(Token::Kind::TOK_EQ, "="); n.bind_expr = std::make_unique<ExprNode>(parse_expr());
        }
        expect(Token::Kind::TOK_SEMICOLON, ";");
    } else if (match(Token::Kind::TOK_TILDE)) {
        // ~ name: Type = expr;   OR   ~ name := expr;  (MutBind)
        n.bind_name = expect(Token::Kind::TOK_IDENT, "name").value;
        n.is_mutable = true;
        if (check(Token::Kind::TOK_WALRUS)) {
            advance(); // consume :=
            n.kind = StmtNode::Kind::LocalInfer;
        } else {
            n.kind = StmtNode::Kind::MutBind;
            if(match(Token::Kind::TOK_COLON)) { n.bind_type = std::make_unique<TypeNode>(parse_type()); }
            expect(Token::Kind::TOK_EQ, "=");
        }
        n.bind_expr = std::make_unique<ExprNode>(parse_expr());
        expect(Token::Kind::TOK_SEMICOLON, ";");
    } else if (check(Token::Kind::TOK_IDENT) && peek(1).kind == Token::Kind::TOK_EQ) {
        // Assign: name = expr;  (bare reassignment, no $ or ~)
        n.kind = StmtNode::Kind::Assign;
        n.bind_name = advance().value; // consume ident
        advance();                      // consume '='
        n.bind_expr = std::make_unique<ExprNode>(parse_expr());
        expect(Token::Kind::TOK_SEMICOLON, ";");
    } else if (check(Token::Kind::TOK_IDENT) && peek(1).kind == Token::Kind::TOK_DOT && peek(2).kind == Token::Kind::TOK_IDENT && peek(3).kind == Token::Kind::TOK_EQ) {
        // Assign: obj.field = expr;  (field reassignment)
        n.kind = StmtNode::Kind::Assign;
        std::string obj = advance().value; advance(); // ident, dot
        n.bind_name = obj + "." + advance().value;   // ident
        advance();                                    // =
        n.bind_expr = std::make_unique<ExprNode>(parse_expr());
        expect(Token::Kind::TOK_SEMICOLON, ";");
    } else if (match(Token::Kind::TOK_QUESTION)) {
        n.kind = StmtNode::Kind::If; expect(Token::Kind::TOK_LPAREN, "("); n.condition = std::make_unique<ExprNode>(parse_expr()); expect(Token::Kind::TOK_RPAREN, ")");
        n.then_branch = std::make_unique<StmtNode>(parse_block());
        if (match(Token::Kind::TOK_COLON)) n.else_branch = std::make_unique<StmtNode>(parse_block());
    } else if (match(Token::Kind::TOK_DBLGT)) {
        n.kind = StmtNode::Kind::Match; expect(Token::Kind::TOK_LPAREN, "("); n.match_expr = std::make_unique<ExprNode>(parse_expr()); expect(Token::Kind::TOK_RPAREN, ")");
        expect(Token::Kind::TOK_LBRACE, "{");
        while(!check(Token::Kind::TOK_RBRACE) && !at_end()) { 
            while(!check(Token::Kind::TOK_LBRACE) && !at_end()) advance(); 
            parse_block(); 
        }
        expect(Token::Kind::TOK_RBRACE, "}");
    } else if (match(Token::Kind::TOK_AT_ITER)) {
        n.kind = StmtNode::Kind::ForIn; n.iter_var = advance().value; expect(Token::Kind::TOK_LARROW, "<-"); n.iter_expr = std::make_unique<ExprNode>(parse_expr()); expect(Token::Kind::TOK_RPAREN, ")"); n.then_branch = std::make_unique<StmtNode>(parse_block());
    } else {
        n.kind = StmtNode::Kind::Expr; n.expr = std::make_unique<ExprNode>(parse_expr()); 
        if(check(Token::Kind::TOK_SEMICOLON)) expect(Token::Kind::TOK_SEMICOLON, ";");
    }
    return n;
}

StmtNode Parser::parse_block() {
    StmtNode n; n.loc = SourceLoc{current().line, current().col, current().offset}; n.kind = StmtNode::Kind::Block;
    expect(Token::Kind::TOK_LBRACE, "Expected {");
    while(!check(Token::Kind::TOK_RBRACE) && !at_end()) {
        if (check(Token::Kind::TOK_CARET) || check(Token::Kind::TOK_CARET_BANG)) { n.block_tail = std::make_unique<ExprNode>(parse_expr()); match(Token::Kind::TOK_SEMICOLON); break; }
        else if (current().kind >= Token::Kind::TOK_DOLLAR && current().kind <= Token::Kind::TOK_TILDE) n.block_stmts.push_back(parse_stmt());
        else { n.block_stmts.push_back(parse_stmt()); }
    }
    expect(Token::Kind::TOK_RBRACE, "Expected }");
    return n;
}

ExprNode Parser::parse_expr() { return parse_expr_prec(0); }

ExprNode Parser::parse_expr_prec(int min_prec) {
    ExprNode left = parse_unary();
    while(!at_end()) {
        int p = 0;
        if (check(Token::Kind::TOK_PLUS) || check(Token::Kind::TOK_MINUS)) p = 6;
        else if (check(Token::Kind::TOK_LT) || check(Token::Kind::TOK_GT) || check(Token::Kind::TOK_LEQ) || check(Token::Kind::TOK_GEQ)) p = 5;
        else if (check(Token::Kind::TOK_EQ) || check(Token::Kind::TOK_NEQ)) p = 4;
        else if (check(Token::Kind::TOK_PROPAGATE) || check(Token::Kind::TOK_DOT)) p = 9;
        
        if (p == 0 || p < min_prec) break;
        Token op = advance();

        if (op.kind == Token::Kind::TOK_PROPAGATE) {
            ExprNode prop; prop.kind = ExprNode::Kind::Propagate; prop.children.push_back(std::move(left)); left = std::move(prop); continue;
        }
        if (op.kind == Token::Kind::TOK_DOT) {
            ExprNode field; field.kind = ExprNode::Kind::MethodCall; field.field_name = advance().value;
            if (match(Token::Kind::TOK_TYPE_UNIT)) { /* consumed natively */ }
            else if (match(Token::Kind::TOK_LPAREN)) expect(Token::Kind::TOK_RPAREN, ")");
            field.children.push_back(std::move(left)); left = std::move(field); continue;
        }

        ExprNode res; res.children.push_back(std::move(left)); res.children.push_back(parse_expr_prec(p + 1));
        if (op.kind == Token::Kind::TOK_PLUS) res.kind = ExprNode::Kind::Add;
        else if (op.kind == Token::Kind::TOK_MINUS) res.kind = ExprNode::Kind::Sub;
        else if (op.kind == Token::Kind::TOK_LT) res.kind = ExprNode::Kind::Lt;
        else if (op.kind == Token::Kind::TOK_GT) res.kind = ExprNode::Kind::Gt;
        else if (op.kind == Token::Kind::TOK_LEQ) res.kind = ExprNode::Kind::Leq;
        else if (op.kind == Token::Kind::TOK_GEQ) res.kind = ExprNode::Kind::Geq;
        else if (op.kind == Token::Kind::TOK_NEQ) res.kind = ExprNode::Kind::Neq;
        else res.kind = ExprNode::Kind::Eq; 
        left = std::move(res);
    }
    return left;
}

ExprNode Parser::parse_unary() {
    if (match(Token::Kind::TOK_NOT)) { ExprNode e; e.kind = ExprNode::Kind::Not; e.children.push_back(parse_unary()); return e; }
    if (match(Token::Kind::TOK_CARET)) { ExprNode e; e.kind = ExprNode::Kind::ReturnOk; expect(Token::Kind::TOK_LPAREN, "("); e.children.push_back(parse_expr()); expect(Token::Kind::TOK_RPAREN, ")"); return e; }
    if (match(Token::Kind::TOK_CARET_BANG)) { ExprNode e; e.kind = ExprNode::Kind::ReturnErr; expect(Token::Kind::TOK_LPAREN, "("); e.children.push_back(parse_expr()); expect(Token::Kind::TOK_RPAREN, ")"); return e; }
    if (match(Token::Kind::TOK_AT_PAR)) {
        ExprNode e; e.kind = ExprNode::Kind::Par;
        expect(Token::Kind::TOK_LBRACE, "{");
        while (!check(Token::Kind::TOK_RBRACE) && !at_end()) {
            // Parse named field: name: expr,
            if (check(Token::Kind::TOK_IDENT) && peek(1).kind == Token::Kind::TOK_COLON) {
                e.named_keys.push_back(advance().value); // name
                advance();                                // :
                e.children.push_back(parse_expr());
                match(Token::Kind::TOK_COMMA);
            } else { advance(); }
        }
        expect(Token::Kind::TOK_RBRACE, "}");
        return e;
    }
    if (match(Token::Kind::TOK_AT_SYN)) {
        ExprNode e; e.kind = ExprNode::Kind::Sync;
        // @syn!  — hard sync
        if (match(Token::Kind::TOK_NOT)) e.bool_val = true; // flag hard sync
        match(Token::Kind::TOK_SEMICOLON);
        return e;
    }
    return parse_primary();
}

ExprNode Parser::parse_primary() {
    ExprNode e; e.loc = SourceLoc{current().line, current().col, current().offset};

    // Integer literal: 42i
    if (match(Token::Kind::TOK_INT_LIT)) {
        e.kind = ExprNode::Kind::IntLit;
        std::string raw = peek(-1).value;
        if (!raw.empty() && raw.back() == 'i') raw.pop_back();
        e.int_val = std::stoll(raw);
        return e;
    }
    // Unsigned literal: 1u
    if (match(Token::Kind::TOK_UINT_LIT)) {
        e.kind = ExprNode::Kind::UintLit;
        std::string raw = peek(-1).value;
        if (!raw.empty() && raw.back() == 'u') raw.pop_back();
        e.uint_val = std::stoull(raw);
        return e;
    }
    // Float literal: 3.14f
    if (match(Token::Kind::TOK_FLOAT_LIT)) {
        e.kind = ExprNode::Kind::FloatLit;
        std::string raw = peek(-1).value;
        if (!raw.empty() && raw.back() == 'f') raw.pop_back();
        e.float_val = std::stod(raw);
        return e;
    }
    // Boolean literals: 1b = true, 0b = false
    if (match(Token::Kind::TOK_BOOL_TRUE))  { e.kind = ExprNode::Kind::BoolLit; e.bool_val = true;  return e; }
    if (match(Token::Kind::TOK_BOOL_FALSE)) { e.kind = ExprNode::Kind::BoolLit; e.bool_val = false; return e; }

    // String literal — strip enclosing quotes
    if (match(Token::Kind::TOK_STR_LIT)) { 
        e.kind = ExprNode::Kind::StrLit; 
        std::string s = peek(-1).value;
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
        e.str_val = s; 
        return e; 
    }

    if (match(Token::Kind::TOK_TYPE_UNIT)) { e.kind = ExprNode::Kind::UnitLit; return e; }

    if (match(Token::Kind::TOK_IDENT)) { 
        e.kind = ExprNode::Kind::Ident; e.str_val = peek(-1).value;
        // module::fn(...) call
        if (match(Token::Kind::TOK_BIND)) { 
            e.field_name = advance().value; 
            if (match(Token::Kind::TOK_TYPE_UNIT)) { e.kind = ExprNode::Kind::Call; }
            else if (match(Token::Kind::TOK_LPAREN)) { 
                e.kind = ExprNode::Kind::Call;
                while (!check(Token::Kind::TOK_RPAREN) && !at_end()) {
                    e.children.push_back(parse_expr());
                    if (!match(Token::Kind::TOK_COMMA)) break;
                }
                expect(Token::Kind::TOK_RPAREN, ")"); 
            } 
        }
        // fn() or fn(args) call
        else if (match(Token::Kind::TOK_TYPE_UNIT)) { e.kind = ExprNode::Kind::Call; }
        else if (match(Token::Kind::TOK_LPAREN)) { 
            e.kind = ExprNode::Kind::Call;
            while (!check(Token::Kind::TOK_RPAREN) && !at_end()) {
                e.children.push_back(parse_expr());
                if (!match(Token::Kind::TOK_COMMA)) break;
            }
            expect(Token::Kind::TOK_RPAREN, ")"); 
        }
        return e;
    }

    // @spn Agent; / @snd(ch, val); / @rcv(ch); — multi-agent primitives
    if (match(Token::Kind::TOK_AT_SPN)) {
        e.kind = ExprNode::Kind::Spawn;
        if (check(Token::Kind::TOK_IDENT)) e.str_val = advance().value; // agent name
        match(Token::Kind::TOK_SEMICOLON);
        return e;
    }
    if (match(Token::Kind::TOK_AT_SND)) {
        e.kind = ExprNode::Kind::Send;
        expect(Token::Kind::TOK_LPAREN, "(");
        e.children.push_back(parse_expr()); // channel
        match(Token::Kind::TOK_COMMA);
        if (!check(Token::Kind::TOK_RPAREN)) e.children.push_back(parse_expr()); // payload
        expect(Token::Kind::TOK_RPAREN, ")");
        return e;
    }
    if (match(Token::Kind::TOK_AT_RCV)) {
        e.kind = ExprNode::Kind::Recv;
        expect(Token::Kind::TOK_LPAREN, "(");
        e.children.push_back(parse_expr()); // channel
        expect(Token::Kind::TOK_RPAREN, ")");
        return e;
    }

    // @budget(inspect) / @budget(used) / @budget(peak)
    if (match(Token::Kind::TOK_AT_BUDGET)) {
        e.kind = ExprNode::Kind::BudgetInspect;
        if (match(Token::Kind::TOK_LPAREN)) {
            e.str_val = advance().value; // inspect / used / peak
            expect(Token::Kind::TOK_RPAREN, ")");
        }
        return e;
    }

    // @mem(tier).write(...) / @mem(tier).read(...)
    if (match(Token::Kind::TOK_AT_MEM)) {
        e.kind = ExprNode::Kind::Snapshot; // reuse node type for @mem
        if (match(Token::Kind::TOK_LPAREN)) {
            e.str_val = advance().value; // work/ep/sem/ext
            expect(Token::Kind::TOK_RPAREN, ")");
        }
        return e;
    }

    // @correct { block } (repair: |err| { ... })
    if (match(Token::Kind::TOK_AT_CORRECT)) {
        e.kind = ExprNode::Kind::Correct;
        e.children.push_back(ExprNode()); // placeholder — body parsed as block
        // Consume the main block
        if (check(Token::Kind::TOK_LBRACE)) {
            expect(Token::Kind::TOK_LBRACE, "{");
            while (!check(Token::Kind::TOK_RBRACE) && !at_end()) advance();
            expect(Token::Kind::TOK_RBRACE, "}");
        }
        // Optional (repair: |err| { ... })
        if (match(Token::Kind::TOK_LPAREN)) {
            while (!check(Token::Kind::TOK_RPAREN) && !at_end()) advance();
            expect(Token::Kind::TOK_RPAREN, ")");
        }
        return e;
    }
    
    emit_error("E105", "Unexpected Expression", "Add valid literal", {}); advance();
    return e;
}

} // namespace agentc
