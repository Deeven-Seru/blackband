#pragma once
#include <string>
#include <optional>
#include <variant>
#include <vector>

namespace agentc::lsp {

// ── Position & Range ──────────────────────────────────
struct Position {
    int line;       // 0-indexed
    int character;  // 0-indexed, UTF-16 offset
};

struct Range {
    Position start;
    Position end;
};

struct Location {
    std::string uri;
    Range       range;
};

// ── Diagnostic (error/warning shown in editor) ────────
struct Diagnostic {
    enum class Severity { Error=1, Warning=2, Info=3, Hint=4 };

    Range       range;
    Severity    severity;
    std::string code;       // "E301", "W701" etc
    std::string source;     // "agentc"
    std::string message;    // human-readable (for hover)
    // AgentC extension: machine-readable fix
    std::string fix_act;    // "elevate_trust"
    std::string fix_via;    // "val::tru(raw)?"
    std::vector<std::string> alt;
    // patch[] for one-click apply
    struct PatchOp {
        std::string op;     // replace|insert|delete
        Range       range;
        std::string new_text;
    };
    std::vector<PatchOp> patches;
};

// ── Completion Item ───────────────────────────────────
struct CompletionItem {
    enum class Kind {
        Function=3, Variable=6, Module=9, Keyword=14, Snippet=15, TypeParameter=25
    };
    std::string label;
    Kind        kind;
    std::string detail;         // type signature
    std::string documentation;  // #> intent string
    std::string insert_text;    // what gets inserted
};

// ── Hover ─────────────────────────────────────────────
struct HoverResult {
    std::string contents;   // markdown string
    Range       range;
};

// ── JSON-RPC message types ────────────────────────────
struct RequestMessage {
    std::string id;         // string or number
    std::string method;
    std::string params_json; // raw JSON
};

struct ResponseMessage {
    std::string id;
    std::string result_json; // raw JSON
    bool        is_error = false;
    int         error_code = 0;
    std::string error_msg;
};

struct NotificationMessage {
    std::string method;
    std::string params_json;
};

// ── Serialization ─────────────────────────────────────
std::string position_to_json(const Position& p);
std::string range_to_json(const Range& r);
std::string diagnostic_to_json(const Diagnostic& d);
std::string completion_to_json(const CompletionItem& c);
std::string hover_to_json(const HoverResult& h);
std::string response_to_json(const ResponseMessage& r);
std::string notification_to_json(const NotificationMessage& n);

// Parse incoming JSON-RPC
RequestMessage parse_request(const std::string& json);
Position       parse_position(const std::string& json);
std::pair<std::string, std::string> parse_did_open(const std::string& json);
std::pair<std::string, std::string> parse_did_change(const std::string& json);
std::pair<std::string, Position> parse_position_params(const std::string& json);

// Internal json escaping
std::string escape_json(const std::string& s);

} // namespace agentc::lsp
