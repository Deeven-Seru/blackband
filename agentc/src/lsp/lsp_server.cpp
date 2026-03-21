#include "lsp_server.hpp"

namespace agentc::lsp {

void LspServer::run() {
    log("AgentC LSP Server starting...");
    while (true) {
        auto msg = read_message();
        if (msg.empty()) break;
        auto req = parse_request(msg);
        handle(req);
    }
}

void LspServer::handle(const RequestMessage& req) {
    if (req.method == "initialize") handle_initialize(req);
    else if (req.method == "initialized") {} // no-op
    else if (req.method == "textDocument/didOpen") handle_did_open(req);
    else if (req.method == "textDocument/didChange") handle_did_change(req);
    else if (req.method == "textDocument/completion") handle_completion(req);
    else if (req.method == "textDocument/hover") handle_hover(req);
    else if (req.method == "shutdown") send_response(req.id, "null");
    else if (req.method == "exit") exit(0);
}

void LspServer::handle_initialize(const RequestMessage& req) {
    std::string result = R"({
        "capabilities": {
            "textDocumentSync": {
                "openClose": true,
                "change": 1
            },
            "completionProvider": {
                "triggerCharacters": ["$","~","#","@","+",":","ƒ"]
            },
            "hoverProvider": true,
            "diagnosticProvider": {
                "interFileDependencies": false,
                "workspaceDiagnostics": false
            }
        },
        "serverInfo": {
            "name": "agentc-lsp",
            "version": "0.6.0"
        }
    })";
    send_response(req.id, result);
}

void LspServer::handle_did_open(const RequestMessage& req) {
    auto [uri, text] = parse_did_open(req.params_json);
    documents_[uri] = text;
    publish_diagnostics(uri, text);
}

void LspServer::handle_did_change(const RequestMessage& req) {
    auto [uri, text] = parse_did_change(req.params_json);
    documents_[uri] = text;
    publish_diagnostics(uri, text);
}

void LspServer::publish_diagnostics(const std::string& uri, const std::string& source) {
    auto diags = diag_engine_.check(source, uri);
    std::string diags_json = "[";
    for (size_t i = 0; i < diags.size(); i++) {
        if (i) diags_json += ",";
        diags_json += diagnostic_to_json(diags[i]);
    }
    diags_json += "]";
    std::string params = "{\"uri\":\"" + uri + "\",\"diagnostics\":" + diags_json + "}";
    send_notification("textDocument/publishDiagnostics", params);
}

void LspServer::handle_completion(const RequestMessage& req) {
    auto [uri, pos] = parse_position_params(req.params_json);
    auto source = documents_[uri];
    auto items = completion_engine_.complete(source, pos);
    std::string result = "{\"items\":[";
    for (size_t i = 0; i < items.size(); i++) {
        if (i) result += ",";
        result += completion_to_json(items[i]);
    }
    result += "]}";
    send_response(req.id, result);
}

void LspServer::handle_hover(const RequestMessage& req) {
    auto [uri, pos] = parse_position_params(req.params_json);
    auto source = documents_[uri];
    auto hover = hover_engine_.hover(source, pos);
    if (hover.contents.empty()) {
        send_response(req.id, "null");
        return;
    }
    send_response(req.id, hover_to_json(hover));
}

std::string LspServer::read_message() {
    std::string header;
    int content_length = 0;
    while (std::getline(std::cin, header)) {
        if (header.length() > 0 && header.back() == '\r') {
            header.pop_back();
        }
        if (header.find("Content-Length: ") == 0) {
            content_length = std::stoi(header.substr(16));
        }
        if (header.empty()) break;
    }
    if (content_length == 0) return "";
    std::string body(content_length, '\0');
    std::cin.read(&body[0], content_length);
    return body;
}

void LspServer::send_response(const std::string& id, const std::string& result) {
    std::string msg = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + result + "}";
    write_message(msg);
}

void LspServer::send_notification(const std::string& method, const std::string& params) {
    std::string msg = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + params + "}";
    write_message(msg);
}

void LspServer::write_message(const std::string& msg) {
    std::cout << "Content-Length: " << msg.size() << "\r\n\r\n" << msg;
    std::cout.flush();
}

void LspServer::log(const std::string& msg) {
    std::cerr << "[agentc-lsp] " << msg << "\n";
    std::cerr.flush();
}

} // namespace agentc::lsp

int main() {
    agentc::lsp::LspServer server;
    server.run();
    return 0;
}
