#include "completion.hpp"

namespace agentc::lsp {

CompletionEngine::Context CompletionEngine::detect_context(const std::string& source, const Position& pos) {
    if (source.empty() || pos.line < 0 || pos.character < 0) return Context::Default;
    // Basic heuristics iterating backwards from cursor
    int curr_line = 0;
    int curr_char = 0;
    size_t i = 0;
    while (i < source.size()) {
        if (curr_line == pos.line && curr_char == pos.character) break;
        if (source[i] == '\n') { curr_line++; curr_char = 0; }
        else { curr_char++; }
        i++;
    }
    std::string prefix = source.substr(0, i);
    // Find last token
    size_t last_open = prefix.rfind("#[");
    size_t last_close = prefix.rfind("]");
    if (last_open != std::string::npos && (last_close == std::string::npos || last_open > last_close)) {
        return Context::Annotation;
    }
    size_t last_hash = prefix.rfind('#');
    size_t last_space = prefix.rfind(' ');
    if (last_hash != std::string::npos && last_hash + 1 == prefix.size()) {
        return Context::Annotation;
    }
    size_t last_colon = prefix.rfind(':');
    if (last_colon != std::string::npos && (last_space == std::string::npos || last_colon > last_space)) {
        return Context::Type;
    }
    size_t last_import = prefix.rfind("+>");
    if (last_import != std::string::npos && (last_space == std::string::npos || last_import > last_space)) {
        return Context::Import;
    }
    return Context::Expression;
}

std::vector<CompletionItem> CompletionEngine::complete(const std::string& source, const Position& pos) {
    std::vector<CompletionItem> items;
    auto ctx = detect_context(source, pos);

    switch (ctx) {
        case Context::Annotation: add_annotation_completions(items); break;
        case Context::Type:       add_type_completions(items); break;
        case Context::Import:     add_module_completions(items); break;
        case Context::Expression:
            add_keyword_completions(items);
            add_stdlib_completions(items);
            add_local_bindings(items, source, pos);
            add_type_completions(items);
            break;
        default: add_all_completions(items);
    }
    return items;
}

void CompletionEngine::add_all_completions(std::vector<CompletionItem>& items) {
    add_keyword_completions(items);
    add_module_completions(items);
    add_annotation_completions(items);
}

void CompletionEngine::add_annotation_completions(std::vector<CompletionItem>& items) {
    items.push_back({"#>", CompletionItem::Kind::Snippet, "intent annotation", "Declares what this function does", "#>\"$1\""});
    items.push_back({"#$", CompletionItem::Kind::Snippet, "cost annotation", "Declares resource usage: cpu|io|net|mem|llm|agt", "#\\$(${1|cpu,io,net,mem,llm,agt|})"});
    items.push_back({"#!", CompletionItem::Kind::Snippet, "side effects annotation", "Declares effects: none|io|net|mem|sys|agt", "#!(${1|none,io,net,mem,sys,agt|})"});
    items.push_back({"#@", CompletionItem::Kind::Snippet, "verification annotation", "Pre/post/invariant contract", "#@(${1|pre,post,inv|}: $2)"});
    items.push_back({"#~", CompletionItem::Kind::Snippet, "trust annotation", "Required trust level: tru|utr|sys", "#~(${1|tru,utr,sys|})"});
    items.push_back({"#?", CompletionItem::Kind::Snippet, "confidence threshold", "Only execute if agent confidence >= N", "#?(${1:0.9})"});
    items.push_back({"#*", CompletionItem::Kind::Snippet, "retry policy", "Retry N times with optional backoff ms", "#*(${1:3}:${2:500})"});
    items.push_back({"#^", CompletionItem::Kind::Snippet, "goal annotation", "Links function to agent planning goal", "#^(\"$1\")"});
    items.push_back({"#&", CompletionItem::Kind::Snippet, "capability requirement", "Required capabilities: net|fs|llm|agt|sys", "#&(${1|net,fs,llm,agt,sys|})"});
}

void CompletionEngine::add_type_completions(std::vector<CompletionItem>& items) {
    for (auto& t : {"i8","i16","i32","i64","i128","u8","u16","u32","u64","u128","f32","f64","B","()"}) {
        items.push_back({t, CompletionItem::Kind::TypeParameter, "primitive type", "", t});
    }
    items.push_back({"Str<", CompletionItem::Kind::TypeParameter, "Str<N> — bounded UTF-8 string", "N = max byte length (required)", "Str<${1:256}>"});
    items.push_back({"Tru<", CompletionItem::Kind::TypeParameter, "Tru<T> — trusted, validated data", "Data that has passed val:: validation", "Tru<${1:Str<256>}>"});
    items.push_back({"Utr<", CompletionItem::Kind::TypeParameter, "Utr<T> — untrusted external data", "Default for all input", "Utr<${1:Str<256>}>"});
    items.push_back({"Own<", CompletionItem::Kind::TypeParameter, "Own<T> — owned value (RAII)", "Freed automatically", "Own<${1:T}>"});
    items.push_back({"Ref<", CompletionItem::Kind::TypeParameter, "Ref<T> — borrowed reference", "Read-only borrowed", "Ref<${1:T}>"});
    items.push_back({"Opt<", CompletionItem::Kind::TypeParameter, "Opt<T> — optional value", "Must be handled explicitly", "Opt<${1:T}>"});
    items.push_back({"Lst<", CompletionItem::Kind::TypeParameter, "Lst<T> — typed list", "Homogeneous collection", "Lst<${1:T}>"});
    items.push_back({"Cnf<", CompletionItem::Kind::TypeParameter, "Cnf<T,N> — value with confidence level", "N = 0.0-1.0", "Cnf<${1:T},${2:0.8}>"});
    items.push_back({"Bnd<", CompletionItem::Kind::TypeParameter, "Bnd<T,L,H> — range-bounded value", "Compiler enforces L <= value <= H", "Bnd<${1:i32},${2:0},${3:100}>"});
}

void CompletionEngine::add_module_completions(std::vector<CompletionItem>& items) {
    struct Mod { const char* name; const char* desc; };
    Mod mods[] = {
        {"io", "Filesystem IO operations"},
        {"net", "Network request module"},
        {"val", "Validation module"},
        {"ctx", "LLM Inference module"},
        {"mem", "Native Memory hooks module"},
        {"trc", "Tracing span generators module"}
    };
    for (auto& m : mods) {
        items.push_back({m.name, CompletionItem::Kind::Module, m.desc, "", m.name});
    }
}

void CompletionEngine::add_stdlib_completions(std::vector<CompletionItem>& items) {
    struct Mod { const char* name; const char* desc; };
    Mod mods[] = {
        {"io::rd",      "Read file -> IoR<Utr<Str>>"},
        {"io::wr",      "Write file (requires Tru<Str>)"},
        {"io::ls",      "List directory -> IoR<Lst<Utr<Str>>>"},
        {"io::del",     "Delete file"},
        {"io::ex",      "File exists returns boolean natively"},
        {"net::get",    "HTTP GET -> NetR<Utr<Str>>"},
        {"net::post",   "HTTP POST -> NetR<Utr<Str>>"},
        {"val::tru",    "Validate Utr->Tru -> T?E"},
        {"val::san",    "Sanitize string -> Tru<Str>?E"},
        {"val::bnd",    "Range check -> T?E"},
        {"ctx::infer",  "LLM call -> AgtR<Utr<Str>>"},
        {"ctx::embed",  "Embedding -> AgtR<Lst<f32>>"},
        {"std::time",   "Unix timestamp -> u64"},
        {"std::uid",    "Generate UUID -> Str"}
    };
    for (auto& m : mods) {
        items.push_back({m.name, CompletionItem::Kind::Function, m.desc, "", m.name});
    }
}

void CompletionEngine::add_keyword_completions(std::vector<CompletionItem>& items) {
    items.push_back({"ƒ", CompletionItem::Kind::Snippet, "function declaration", "Declare an AgentC function", "ƒ ${1:name}(${2}) -> ${3:i32} {\n\t$0\n}"});
    items.push_back({"$", CompletionItem::Kind::Snippet, "immutable binding", "Declare immutable value", "\\$ ${1:name}: ${2:T} = $0;"});
    items.push_back({"~", CompletionItem::Kind::Snippet, "mutable binding", "Declare mutable value", "~ ${1:name}: ${2:T} = $0;"});
    items.push_back({"?()", CompletionItem::Kind::Snippet, "if condition", "Conditional branch", "?(${1:condition}) {\n\t$2\n} : {\n\t$3\n}"});
    items.push_back({"loop", CompletionItem::Kind::Snippet, "infinite loop", "Loop until break", "loop {\n\t$0\n}"});
    items.push_back({"@(", CompletionItem::Kind::Snippet, "for-in loop", "Iterate collection", "@(${1:item} <- ${2:collection}) {\n\t$0\n}"});
    items.push_back({">>(", CompletionItem::Kind::Snippet, "match/dispatch", "Exhaustive match", ">>(${1:expr}) {\n\t${2:Pattern} => { $0 }\n}"});
    items.push_back({"^(", CompletionItem::Kind::Snippet, "return ok", "Return successful value", "^($0)"});
    items.push_back({"^!(", CompletionItem::Kind::Snippet, "return error", "Return error value", "^!($0)"});
    items.push_back({"@correct", CompletionItem::Kind::Snippet, "self-correction block", "Execute with auto-retry", "@correct {\n\t$1\n} repair(err) {\n\t patch_apply(err.patch)?;\n}"});
    items.push_back({"@par", CompletionItem::Kind::Snippet, "parallel block", "Run concurrently", "@par {\n\t${1:fst}: ${2:expr1},\n\t${3:snd}: ${4:expr2}\n}"});
    items.push_back({"@budget()", CompletionItem::Kind::Function, "remaining token budget", "Remaining AI tokens", "@budget()"});
    items.push_back({"@trace", CompletionItem::Kind::Snippet, "execution trace", "Capture history", "$ ${1:result}: Trc<${2:T}> = @trace { $0 };"});
}

void CompletionEngine::add_local_bindings(std::vector<CompletionItem>& items, const std::string& source, const Position& pos) {
    // We could parse AST for local vars here if needed, keeping basic
    items.push_back({"result", CompletionItem::Kind::Variable, "local result variable", "", "result"});
}

} // namespace agentc::lsp
