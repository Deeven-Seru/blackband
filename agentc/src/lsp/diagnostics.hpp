#pragma once
#include "protocol.hpp"
#include "../typechecker/typechecker.hpp"
#include "../parser/parser.hpp"
#include "../lexer/lexer.hpp"
#include <string>
#include <vector>

namespace agentc::lsp {

class DiagnosticsEngine {
public:
    // Run full compiler pipeline on source text
    // Returns list of diagnostics for the editor
    std::vector<Diagnostic> check(const std::string& source, const std::string& uri);

private:
    Diagnostic from_type_error(const TypeError& e);
    Diagnostic from_warning(const TypeWarning& w);
    Diagnostic from_parse_error(const ParseError& e);
    Diagnostic from_lex_error(const LexError& e);

    Range to_range(size_t line, size_t col, size_t end_col = 0);
};

} // namespace agentc::lsp
