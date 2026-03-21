#pragma once
#include "protocol.hpp"
#include "diagnostics.hpp"
#include "completion.hpp"
#include "hover.hpp"
#include <string>
#include <unordered_map>
#include <iostream>

namespace agentc::lsp {

class LspServer {
public:
    void run();

private:
    DiagnosticsEngine diag_engine_;
    CompletionEngine  completion_engine_;
    HoverEngine       hover_engine_;

    std::unordered_map<std::string, std::string> documents_;

    void handle(const RequestMessage& req);
    void handle_initialize(const RequestMessage& req);
    void handle_did_open(const RequestMessage& req);
    void handle_did_change(const RequestMessage& req);
    void handle_completion(const RequestMessage& req);
    void handle_hover(const RequestMessage& req);
    void publish_diagnostics(const std::string& uri, const std::string& source);

    std::string read_message();
    void send_response(const std::string& id, const std::string& result);
    void send_notification(const std::string& method, const std::string& params);
    void write_message(const std::string& msg);
    void log(const std::string& msg);
};

} // namespace agentc::lsp
