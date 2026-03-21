#pragma once
#include "protocol.hpp"
#include <vector>
#include <string>

namespace agentc::lsp {

class CompletionEngine {
public:
    std::vector<CompletionItem> complete(const std::string& source, const Position& pos);

private:
    enum class Context { Default, Annotation, Type, Import, Expression };
    Context detect_context(const std::string& source, const Position& pos);

    void add_all_completions(std::vector<CompletionItem>& items);
    void add_annotation_completions(std::vector<CompletionItem>& items);
    void add_type_completions(std::vector<CompletionItem>& items);
    void add_module_completions(std::vector<CompletionItem>& items);
    void add_keyword_completions(std::vector<CompletionItem>& items);
    void add_stdlib_completions(std::vector<CompletionItem>& items);
    void add_local_bindings(std::vector<CompletionItem>& items, const std::string& source, const Position& pos);
};

} // namespace agentc::lsp
