#pragma once
#include "protocol.hpp"
#include <string>

namespace agentc::lsp {

class HoverEngine {
public:
    HoverResult hover(const std::string& source, const Position& pos);
private:
    // Simple scanner finding token and rendering static information dynamically mapping Type contexts securely cleanly!
    std::string get_annotation_docs(const std::string& token);
};

} // namespace agentc::lsp
