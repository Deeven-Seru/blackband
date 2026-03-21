// runtime/modules/ctx.cpp

#include "ctx.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <curl/curl.h>

// Budget state — per-agent in Phase 6
// Global for Phase 5
static thread_local int64_t g_budget_cap  = 4096;
static thread_local int64_t g_budget_used = 0;
static thread_local int64_t g_budget_peak = 0;

extern "C" {

// @budget() → remaining tokens
int64_t agc_budget() {
    return g_budget_cap - g_budget_used;
}

int64_t agc_budget_used() { return g_budget_used; }
int64_t agc_budget_peak() { return g_budget_peak; }

int64_t agc_budget_cost_str(AgcStr s) {
    if (s.ptr == nullptr || s.len <= 0) return 0;
    return (s.len / 4) + 1;  // BPE approximation
}

// Set budget cap (called by runtime when #$(llm:N) scope starts)
void agc_budget_set_cap(int64_t cap) {
    g_budget_cap  = cap;
    g_budget_used = 0;
    g_budget_peak = 0;
}

// Charge tokens against budget
// Returns 0 if ok, 1 if overflow
int8_t agc_budget_charge(int64_t tokens) {
    g_budget_used += tokens;
    if (g_budget_used > g_budget_peak)
        g_budget_peak = g_budget_used;
    return (g_budget_used > g_budget_cap) ? 1 : 0;
}

// @budget(inspect) → JSON
AgcStr agc_budget_inspect() {
    static thread_local char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"total\":%lld,\"used\":%lld,\"rem\":%lld,"
        "\"peak\":%lld,\"W701_at\":%lld,\"E701_at\":%lld}",
        (long long)g_budget_cap,
        (long long)g_budget_used,
        (long long)(g_budget_cap - g_budget_used),
        (long long)g_budget_peak,
        (long long)(g_budget_cap * 8 / 10),  // 80%
        (long long)g_budget_cap);
    return agc_make_str(buf);
}

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

AgcResult agc_ctx_infer(AgcStr prompt) {
    const char* api_key = getenv("AGENTC_API_KEY");
    if (!api_key)
        return agc_err(901, "AGENTC_API_KEY not set");

    std::string prompt_str(prompt.ptr, prompt.len);

    // JSON escape the prompt
    std::string escaped;
    for (char c : prompt_str) {
        if (c == '"')       escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else if (c == '\n') escaped += " ";
        else escaped += c;
    }

    // Gemini request body format
    std::string body =
        "{\"contents\":[{\"parts\":[{\"text\":\""
        + escaped + "\"}]}]}";

    // Gemini endpoint — API key goes in URL
    std::string url =
        "https://generativelanguage.googleapis.com"
        "/v1beta/models/gemini-2.5-flash:generateContent"
        "?key=" + std::string(api_key);

    CURL* curl = curl_easy_init();
    if (!curl) return agc_err(902, "curl init failed");

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers,
        "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,         url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,  body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,  headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,   &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,     30L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return agc_err(903,
            std::string("Gemini call failed: ") +
            curl_easy_strerror(res));

    // Debug: print raw response to stderr
    fprintf(stderr, "[agentc-ctx] Gemini response: %s\n",
            response.substr(0, 300).c_str());

    // Gemini response format:
    // {"candidates":[{"content":{"parts":[{"text":"..."}]}},...]}
    size_t pos = response.find("\"text\"");
    if (pos == std::string::npos)
        return agc_err(904,
            "No text in Gemini response: " +
            response.substr(0, 300));
    
    pos = response.find(':', pos + 6);
    if (pos == std::string::npos) return agc_err(904, "Malformed JSON format");
    
    pos = response.find('"', pos);
    if (pos == std::string::npos) return agc_err(904, "Malformed JSON format missing quote");
    pos++; // skip the opening quote
    std::string text;
    while (pos < response.size() && response[pos] != '"') {
        if (response[pos] == '\\' &&
            pos + 1 < response.size()) {
            pos++;
            if      (response[pos] == 'n')  text += '\n';
            else if (response[pos] == '"')  text += '"';
            else if (response[pos] == '\\') text += '\\';
            else if (response[pos] == 't')  text += '\t';
            else                            text += response[pos];
        } else {
            text += response[pos];
        }
        pos++;
    }

    if (text.empty())
        return agc_err(905, "Empty Gemini response text");

    char* heap = agc_heap_alloc(text.size() + 1);
    memcpy(heap, text.c_str(), text.size() + 1);
    return agc_ok_str(heap, (int64_t)text.size());
}

AgcResult agc_ctx_embed(AgcStr text) {
    (void)text;
    const char* stub = "[0.1,0.2,0.3,0.4,0.5]";
    char* heap = agc_heap_alloc(strlen(stub) + 1);
    strcpy(heap, stub);
    return agc_ok_str(heap, (int64_t)strlen(stub));
}

void agc_prune(void* ptr) {
    free(ptr);
}

AgcResult agc_summarize(AgcStr input) {
    if (input.ptr == nullptr || input.len <= 0) return agc_err(902, "ctx::summarize: empty input");
    std::string prompt =
        "Summarize in 1-2 sentences: " +
        std::string(input.ptr, input.len);
    AgcStr p = agc_make_str(prompt.c_str());
    return agc_ctx_infer(p);
}

AgcResult agc_chunk(AgcStr input, int64_t chunk_size) {
    if (input.ptr == nullptr || input.len <= 0) return agc_err(902, "ctx::chunk: empty input");
    int64_t char_size = chunk_size * 4;
    std::string in(input.ptr, input.len);
    std::string result = "[";
    bool first = true;

    for (int64_t i = 0; i < (int64_t)in.size(); i += char_size) {
        if (!first) result += ",";
        std::string chunk = in.substr(i,
            std::min((int64_t)in.size() - i, char_size));
        result += "\"" + chunk + "\"";
        first = false;
    }
    result += "]";

    char* heap = agc_heap_alloc(result.size() + 1);
    memcpy(heap, result.c_str(), result.size() + 1);
    return agc_ok_str(heap, (int64_t)result.size());
}

} // extern "C"
