#include "protocol.hpp"
#include <sstream>

namespace agentc::lsp {

std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else o << c;
    }
    return o.str();
}

std::string position_to_json(const Position& p) {
    return "{\"line\":" + std::to_string(p.line) + ",\"character\":" + std::to_string(p.character) + "}";
}

std::string range_to_json(const Range& r) {
    return "{\"start\":" + position_to_json(r.start) + ",\"end\":" + position_to_json(r.end) + "}";
}

std::string diagnostic_to_json(const Diagnostic& d) {
    std::string json = "{\"range\":" + range_to_json(d.range) + ",";
    json += "\"severity\":" + std::to_string((int)d.severity) + ",";
    json += "\"code\":\"" + escape_json(d.code) + "\",";
    json += "\"source\":\"" + escape_json(d.source) + "\",";
    json += "\"message\":\"" + escape_json(d.message) + "\",";
    json += "\"data\":{";
    json += "\"fix_act\":\"" + escape_json(d.fix_act) + "\",";
    json += "\"fix_via\":\"" + escape_json(d.fix_via) + "\"";
    if (!d.patches.empty()) {
        json += ",\"patches\":[";
        for (size_t i = 0; i < d.patches.size(); i++) {
            if (i) json += ",";
            json += "{\"op\":\"" + escape_json(d.patches[i].op) + "\",";
            json += "\"range\":" + range_to_json(d.patches[i].range) + ",";
            json += "\"new_text\":\"" + escape_json(d.patches[i].new_text) + "\"}";
        }
        json += "]";
    }
    json += "}}";
    return json;
}

std::string completion_to_json(const CompletionItem& c) {
    std::string json = "{";
    json += "\"label\":\"" + escape_json(c.label) + "\",";
    json += "\"kind\":" + std::to_string((int)c.kind) + ",";
    json += "\"detail\":\"" + escape_json(c.detail) + "\",";
    json += "\"documentation\":\"" + escape_json(c.documentation) + "\",";
    json += "\"insertText\":\"" + escape_json(c.insert_text) + "\",";
    json += "\"insertTextFormat\":2"; // Snippet
    json += "}";
    return json;
}

std::string hover_to_json(const HoverResult& h) {
    return "{\"contents\":{\"kind\":\"markdown\",\"value\":\"" + escape_json(h.contents) + "\"},\"range\":" + range_to_json(h.range) + "}";
}

std::string response_to_json(const ResponseMessage& r) {
    if (r.is_error) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + r.id + ",\"error\":{\"code\":" + std::to_string(r.error_code) + ",\"message\":\"" + escape_json(r.error_msg) + "\"}}";
    }
    return "{\"jsonrpc\":\"2.0\",\"id\":" + r.id + ",\"result\":" + r.result_json + "}";
}

std::string notification_to_json(const NotificationMessage& n) {
    return "{\"jsonrpc\":\"2.0\",\"method\":\"" + escape_json(n.method) + "\",\"params\":" + n.params_json + "}";
}

// Very basic hacky JSON parsing for LSP tests (Regex/string find)
static std::string extract_json_field(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos += key.length() + 2;
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == ':')) pos++;
    if (pos >= json.length()) return "";
    if (json[pos] == '"') {
        size_t start = pos + 1;
        size_t end = start;
        while (end < json.length() && json[end] != '"') {
            if (json[end] == '\\') end++;
            end++;
        }
        return json.substr(start, end - start);
    }
    if (json[pos] == '{' || json[pos] == '[') {
        size_t start = pos;
        int depth = 0;
        do {
            if (json[pos] == '{' || json[pos] == '[') depth++;
            else if (json[pos] == '}' || json[pos] == ']') depth--;
            pos++;
        } while (depth > 0 && pos < json.length());
        return json.substr(start, pos - start);
    }
    // Number or boolean
    size_t start = pos;
    while (pos < json.length() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']' && json[pos] != ' ' && json[pos] != '\n') pos++;
    return json.substr(start, pos - start);
}

RequestMessage parse_request(const std::string& json) {
    RequestMessage r;
    r.id = extract_json_field(json, "id");
    r.method = extract_json_field(json, "method");
    r.params_json = extract_json_field(json, "params");
    if (r.id.empty() && r.method.empty()) {
        // Try parsing as raw unescaped JSON for simplicity during testing
    }
    return r;
}

Position parse_position(const std::string& json) {
    Position p{0, 0};
    std::string line_s = extract_json_field(json, "line");
    std::string char_s = extract_json_field(json, "character");
    if (!line_s.empty()) p.line = std::stoi(line_s);
    if (!char_s.empty()) p.character = std::stoi(char_s);
    return p;
}

std::pair<std::string, std::string> parse_did_open(const std::string& json) {
    std::string doc = extract_json_field(json, "textDocument");
    std::string uri = extract_json_field(doc, "uri");
    std::string text = extract_json_field(doc, "text");
    // Hacky unescape
    std::string unescaped;
    for(size_t i=0; i<text.length(); i++) {
        if (text[i] == '\\' && i+1<text.length()) {
            if (text[i+1] == 'n') unescaped += '\n';
            else if (text[i+1] == 'r') unescaped += '\r';
            else if (text[i+1] == 't') unescaped += '\t';
            else if (text[i+1] == '"') unescaped += '"';
            else if (text[i+1] == '\\') unescaped += '\\';
            i++;
        } else {
            unescaped += text[i];
        }
    }
    return {uri, unescaped};
}

std::pair<std::string, std::string> parse_did_change(const std::string& json) {
    std::string doc = extract_json_field(json, "textDocument");
    std::string uri = extract_json_field(doc, "uri");
    std::string changes = extract_json_field(json, "contentChanges");
    // Find the text in the first change
    std::string text = extract_json_field(changes, "text");
    std::string unescaped;
    for(size_t i=0; i<text.length(); i++) {
        if (text[i] == '\\' && i+1<text.length()) {
            if (text[i+1] == 'n') unescaped += '\n';
            else if (text[i+1] == 'r') unescaped += '\r';
            else if (text[i+1] == 't') unescaped += '\t';
            else if (text[i+1] == '"') unescaped += '"';
            else if (text[i+1] == '\\') unescaped += '\\';
            i++;
        } else {
            unescaped += text[i];
        }
    }
    return {uri, unescaped};
}

std::pair<std::string, Position> parse_position_params(const std::string& json) {
    std::string doc = extract_json_field(json, "textDocument");
    std::string uri = extract_json_field(doc, "uri");
    std::string pos_json = extract_json_field(json, "position");
    return {uri, parse_position(pos_json)};
}

} // namespace agentc::lsp
