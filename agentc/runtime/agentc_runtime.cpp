#include "agentc_runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

// Escape string for safe JSON embedding
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

extern "C" {

static AgcStr make_str(const char* s) {
    return { s, (int64_t)strlen(s) };
}

AgcStr agc_json_escape(AgcStr s) {
    std::string escaped = json_escape(
        std::string(s.ptr, s.len));
    char* heap = (char*)malloc(escaped.size() + 1);
    memcpy(heap, escaped.c_str(), escaped.size() + 1);
    return make_str(heap);
}

static AgcResult ok_result(int64_t val) {
    return { 1, val };
}

static AgcResult err_result(int64_t code) {
    return { 0, code };
}

// ── std module ────────────────────────────────
void agc_print(AgcStr s) {
    fwrite(s.ptr, 1, (size_t)s.len, stdout);
    putchar('\n');
    fflush(stdout);
}

void agc_panic(AgcStr msg) {
    fprintf(stderr, "PANIC: %.*s\n", (int)msg.len, msg.ptr);
    abort();
}

int64_t agc_time() {
    return (int64_t)(time(nullptr)) * 1000LL;
}

AgcStr agc_uid() {
    static char buf[37];
    snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%012x",
        rand(), rand()&0xffff, rand()&0xffff,
        rand()&0xffff, rand());
    return make_str(buf);
}

} // extern "C"
