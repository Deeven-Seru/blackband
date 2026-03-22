// runtime/agentc_runtime.hpp
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {

// ── Core types ────────────────────────────────
struct AgcStr    { const char* ptr; int64_t len; };
struct AgcResult { int8_t ok; int64_t val; };

// ── Heap allocation ───────────────────────────
// All AgentC heap allocs go through here.
// Defined in runtime/snapshot.cpp so that allocation tracking works
// for the snapshot/restore system.
char* agc_heap_alloc(size_t n);

// ── Result constructors ───────────────────────
inline AgcResult agc_ok_i64(int64_t v)  { return {1, v}; }
inline AgcResult agc_ok_unit()          { return {1, 0}; }
inline AgcResult agc_ok_str(const char* p, int64_t len) {
    // Pack str ptr into i64
    (void)len;
    return {1, (int64_t)(uintptr_t)p};
}
inline AgcResult agc_err(int64_t code, const std::string& msg) {
    // In Phase 5: store msg in thread-local error buffer
    (void)msg;
    return {0, code};
}

// ── String constructors ───────────────────────
inline AgcStr agc_make_str(const char* s) {
    return {s, (int64_t)strlen(s)};
}

// ── Snapshot/restore ──────────────────────────
void* agc_snapshot();
void  agc_restore(void* snap);
void  agc_snapshot_discard(void* snap);

// ── Budget declarations ───────────────────────
int64_t agc_budget();
int64_t agc_budget_used();
int64_t agc_budget_peak();
void    agc_budget_set_cap(int64_t cap);
int8_t  agc_budget_charge(int64_t tokens);
AgcStr  agc_budget_inspect();

// ── io module ─────────────────────────────────
AgcResult agc_io_rd(AgcStr path);
AgcResult agc_io_wr(AgcStr path, AgcStr data);
AgcResult agc_io_app(AgcStr path, AgcStr data);
int8_t    agc_io_ex(AgcStr path);
AgcResult agc_io_del(AgcStr path);
AgcResult agc_io_ls(AgcStr path);

// ── net module ────────────────────────────────
AgcResult agc_net_get(AgcStr url);
AgcResult agc_net_post(AgcStr url, AgcStr body);
AgcResult agc_net_dns(AgcStr host);

// ── val module ────────────────────────────────
AgcResult agc_val_tru(AgcStr raw);
AgcResult agc_val_san(AgcStr raw);
AgcResult agc_val_bnd(int64_t val, int64_t lo, int64_t hi);
AgcResult agc_val_vch(AgcStr vouched, AgcStr proof);

// ── ctx module ────────────────────────────────
AgcResult agc_ctx_infer(AgcStr prompt);
AgcResult agc_ctx_embed(AgcStr text);
void      agc_prune(void* ptr);
AgcResult agc_summarize(AgcStr input);
AgcResult agc_chunk(AgcStr input, int64_t chunk_size);
int64_t   agc_budget_cost_str(AgcStr s);

// ── mem module ────────────────────────────────
void*     agc_alloc(int64_t size);
void      agc_free(void* ptr);

// ── trc module ────────────────────────────────
void      agc_trc_begin();
void      agc_trc_op(const char* op, const char* fn);
AgcStr    agc_trc_end(int64_t val);
AgcStr    agc_prof_fn(const char* name, int64_t calls,
                       int64_t total_tokens, int64_t total_ms);

// ── std module ────────────────────────────────
void      agc_print(AgcStr s);
void      agc_panic(AgcStr msg);
int64_t   agc_time();
AgcStr    agc_uid();

} // extern "C"
