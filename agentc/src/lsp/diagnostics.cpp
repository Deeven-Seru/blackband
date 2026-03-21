#include "diagnostics.hpp"

namespace agentc::lsp {

std::vector<Diagnostic> DiagnosticsEngine::check(const std::string& source, const std::string& uri) {
    std::vector<Diagnostic> diags;

    // Stage 1: Lex
    Lexer lexer(source, uri);
    std::vector<Token> tokens;
    while (!lexer.at_end()) {
        auto t = lexer.next();
        tokens.push_back(t);
        if (t.kind == Token::Kind::TOK_EOF) break;
    }
    for (auto& e : lexer.get_errors())
        diags.push_back(from_lex_error(e));

    if (lexer.has_errors()) return diags;

    // Stage 2: Parse
    Parser parser(std::move(tokens), uri);
    auto ast = parser.parse();
    for (auto& e : parser.get_errors())
        diags.push_back(from_parse_error(e));

    if (parser.has_errors()) return diags;

    // Stage 3: Type check
    TypeChecker tc(uri);
    tc.check(std::move(ast));

    for (auto& e : tc.get_errors())
        diags.push_back(from_type_error(e));

    for (auto& w : tc.get_warnings())
        diags.push_back(from_warning(w));

    return diags;
}

Diagnostic DiagnosticsEngine::from_type_error(const TypeError& e) {
    Diagnostic d;
    d.range    = to_range(e.loc.line, e.loc.col, e.loc.col + 5); 
    d.severity = Diagnostic::Severity::Error;
    d.code     = e.code;
    d.source   = "agentc";
    d.message  = "[" + e.code + "] " + e.category + ": expected " + e.why_exp + ", got " + e.why_got;
    if (!e.fix_via.empty()) d.message += "\nFix: " + e.fix_via;

    d.fix_act = e.fix_act;
    d.fix_via = e.fix_via;
    d.alt     = e.alts;

    for (auto& p : e.patches) {
        Diagnostic::PatchOp op;
        op.op       = p.op;
        op.range    = to_range(p.line, p.col, p.col + p.old_text.length()); // Use old text length!
        op.new_text = p.new_text;
        d.patches.push_back(op);
    }
    return d;
}

Diagnostic DiagnosticsEngine::from_warning(const TypeWarning& w) {
    Diagnostic d;
    d.range    = to_range(w.loc.line, w.loc.col, w.loc.col + 5);
    d.severity = Diagnostic::Severity::Warning;
    d.code     = w.code;
    d.source   = "agentc";
    d.message  = "[" + w.code + "] " + w.message;
    return d;
}

Diagnostic DiagnosticsEngine::from_parse_error(const ParseError& e) {
    Diagnostic d;
    d.range    = to_range(e.line, e.col, e.col + 1);
    d.severity = Diagnostic::Severity::Error;
    d.code     = e.code;
    d.source   = "agentc";
    d.message  = "[" + e.code + "] " + e.cause + "\n" + e.fix;
    return d;
}

Diagnostic DiagnosticsEngine::from_lex_error(const LexError& e) {
    Diagnostic d;
    d.range    = to_range(e.line, e.col, e.col + 1);
    d.severity = Diagnostic::Severity::Error;
    d.code     = e.cat;
    d.source   = "agentc";
    d.message  = "[" + e.cat + "] " + e.err + "\n" + e.act + " " + e.via;
    return d;
}

Range DiagnosticsEngine::to_range(size_t line, size_t col, size_t end_col) {
    Range r;
    r.start.line = line > 0 ? line - 1 : 0;
    r.start.character = col > 0 ? col - 1 : 0;
    r.end.line = r.start.line;
    r.end.character = end_col > 0 ? end_col - 1 : r.start.character;
    if (r.end.character == r.start.character) r.end.character += 1;
    return r;
}

} // namespace agentc::lsp
