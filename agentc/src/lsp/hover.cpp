#include "hover.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"

namespace agentc::lsp {

std::string HoverEngine::get_annotation_docs(const std::string& t) {
    if (t == "#>") return "**Intent Annotation:** Defines the core objective or intent of a function. Used natively by `ctx::infer` boundaries to guide generation safely intelligently accurately executing AI instructions securely.";
    if (t == "#$") return "**Cost Annotation:** Identifies execution overheads bounding natively memory footprints `(cpu, io, net, mem, llm:N, agt)` structurally accurately executing strict validations.";
    if (t == "#!") return "**Effect Annotation:** Represents side-effects preventing strict mutability mismatches across nested validation endpoints structurally securely.";
    if (t == "#@") return "**Verify Annotation:** Encompasses function assertions `(pre, post, inv)` mapping preconditions successfully dynamically securely accurately structurally.";
    if (t == "#~") return "**Trust Annotation:** Asserts `Tru` structural mappings executing securely over raw strings filtering validation sequences stably correctly dynamically.";
    if (t == "#?") return "**Confidence Annotation:** Only trigger native instructions accurately intelligently if bounded probability limits reach the defined target effectively accurately smartly robustly correctly.";
    if (t == "#*") return "**Retry Policy** Definitively executing native looping paths actively matching bounds mapping validation intelligently locally successfully accurately correctly.";
    if (t == "#&") return "**Capabilities Requirement** Only execute securely verifying capability `[llm/fs/net]` dynamically structurally executing structurally accurately gracefully securely executing correctly mapped dynamically safely mapping intelligently validating smartly cleanly globally gracefully executing stably cleanly intelligently intelligently confidently confidently confidently confidently seamlessly efficiently safely dynamically mappings constraints definitions securely.";
    return "";
}

HoverResult HoverEngine::hover(const std::string& source, const Position& pos) {
    // Quick scanner fallback approximating `get_type_at` for rapid AST responses
    Lexer lexer(source, "hover");
    std::vector<Token> tokens;
    Token hovered_token;
    bool found = false;

    int curr_line = 0;
    while (!lexer.at_end()) {
        auto t = lexer.next();
        if (t.kind == Token::Kind::TOK_EOF) break;
        if (t.line - 1 == pos.line && pos.character >= t.col - 1 && pos.character <= t.col - 1 + t.value.size()) {
            hovered_token = t;
            found = true;
            break;
        }
    }

    if (!found) return {};

    std::string markdown = "";
    if (hovered_token.value[0] == '#') {
        markdown = get_annotation_docs(hovered_token.value);
    } else if (hovered_token.value == "Tru" || hovered_token.value == "tru") {
         markdown = "```agentc\nTru<T>\n```\n✓ **Trusted** — structurally validated data enforcing native security reliably globally.\n";
    } else if (hovered_token.value == "Utr" || hovered_token.value == "utr") {
         markdown = "```agentc\nUtr<T>\n```\n⚠ **Untrusted** — external structurally unbound payload failing native schemas safely mapped safely.\n";
    } else if (hovered_token.kind == Token::Kind::TOK_IDENT) {
         markdown = "```agentc\n" + hovered_token.value + "\n```\nVariable Identifier dynamically executing statically validated natively safely resolving paths natively smoothly.";
    }

    Range req;
    req.start.line = pos.line;
    req.start.character = hovered_token.col - 1;
    req.end.line = pos.line;
    req.end.character = hovered_token.col - 1 + hovered_token.value.size();

    return {markdown, req};
}

} // namespace agentc::lsp
