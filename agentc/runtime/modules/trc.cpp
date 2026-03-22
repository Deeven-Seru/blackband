// runtime/modules/trc.cpp
#include "trc.hpp"
#include "../snapshot.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <sstream>

struct TrcOp {
    std::string op;
    std::string fn;
    int64_t     t_ms;
};

struct TrcState {
    std::vector<TrcOp> ops;
    int64_t start_ms;
    int64_t tokens_used;
    void* snap;
};

static thread_local TrcState g_trace;

static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

extern "C" {

void agc_trc_begin() {
    agc_snapshot_discard(g_trace.snap);  // free any previous trace snapshot
    g_trace.ops.clear();
    g_trace.start_ms = now_ms();
    g_trace.tokens_used = agc_budget_used();
    g_trace.snap = agc_snapshot();
}

void agc_trc_op(const char* op, const char* fn) {
    g_trace.ops.push_back({
        op, fn,
        now_ms() - g_trace.start_ms
    });
}

AgcStr agc_trc_end(int64_t val) {
    int64_t elapsed = now_ms() - g_trace.start_ms;
    int64_t cost    = agc_budget_used() - g_trace.tokens_used;

    std::ostringstream o;
    o << "{\"val\":" << val
      << ",\"cst\":" << cost
      << ",\"ms\":"  << elapsed
      << ",\"ops\":[";

    for (size_t i = 0; i < g_trace.ops.size(); i++) {
        auto& op = g_trace.ops[i];
        if (i) o << ",";
        o << "{\"op\":\"" << op.op << "\""
          << ",\"fn\":\"" << op.fn << "\""
          << ",\"t\":"    << op.t_ms << "}";
    }
    o << "]}";

    std::string s = o.str();
    char* heap = agc_heap_alloc(s.size() + 1);
    memcpy(heap, s.c_str(), s.size() + 1);
    return agc_make_str(heap);
}

AgcStr agc_prof_fn(const char* fn_name, int64_t calls, int64_t total_tokens, int64_t total_ms) {
    static char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"fn\":\"%s\","
        "\"num\":%lld,"
        "\"avg_tok\":%lld,"
        "\"avg_ms\":%lld,"
        "\"tnd\":\"stable\"}",
        fn_name,
        (long long)calls,
        (long long)(calls > 0 ? total_tokens / calls : 0),
        (long long)(calls > 0 ? total_ms / calls : 0));
    return agc_make_str(buf);
}

} // extern "C"

// Save/restore trace state for snapshot/restore support (called from snapshot.cpp)
void agc_trace_save(AgcTraceSnapState* out) {
    out->ops.clear();
    for (const auto& op : g_trace.ops) {
        out->ops.push_back({op.op, op.fn, op.t_ms});
    }
    out->start_ms    = g_trace.start_ms;
    out->tokens_used = g_trace.tokens_used;
}

void agc_trace_restore_state(const AgcTraceSnapState* in) {
    agc_snapshot_discard(g_trace.snap);  // free snapshot stored at trace start
    g_trace.ops.clear();
    for (const auto& op : in->ops) {
        g_trace.ops.push_back({op.op, op.fn, op.t_ms});
    }
    g_trace.start_ms    = in->start_ms;
    g_trace.tokens_used = in->tokens_used;
    g_trace.snap        = nullptr;
}
