// runtime/snapshot.hpp
// Core snapshot/restore data structures and internal C++ helpers.
// C++-only — not part of the C ABI.
#pragma once
#include "agentc_runtime.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ── Heap allocation tracking ──────────────────────────────────────────────────

struct AgcAllocEntry {
    void*   ptr;
    int64_t size;
};

// Thread-local registry of all agc_alloc() allocations (defined in snapshot.cpp)
extern thread_local std::vector<AgcAllocEntry> g_alloc_registry;

// Called by agc_alloc() / agc_free() in mem.cpp
void agc_alloc_register(void* ptr, int64_t size);
void agc_alloc_unregister(void* ptr);

// Free a snapshot without restoring state (used internally to avoid leaks)
void agc_snapshot_discard(void* snap);

// ── Budget state ──────────────────────────────────────────────────────────────

struct AgcBudgetState {
    int64_t cap;
    int64_t used;
    int64_t peak;
};

// Implemented in ctx.cpp
void agc_budget_save(AgcBudgetState* out);
void agc_budget_restore_state(const AgcBudgetState* in);

// ── Trace state ───────────────────────────────────────────────────────────────

struct AgcTraceOpSnap {
    std::string op;
    std::string fn;
    int64_t     t_ms;
};

struct AgcTraceSnapState {
    std::vector<AgcTraceOpSnap> ops;
    int64_t start_ms;
    int64_t tokens_used;
};

// Implemented in trc.cpp
void agc_trace_save(AgcTraceSnapState* out);
void agc_trace_restore_state(const AgcTraceSnapState* in);

// ── Full snapshot ─────────────────────────────────────────────────────────────

struct AgcHeapRegion {
    void*   ptr;        // original allocation pointer
    int64_t size;       // allocation size in bytes
    char*   data_copy;  // malloc'd copy of the data at snapshot time
};

struct AgcSnapshot {
    std::vector<AgcHeapRegion> heap;    // per-region copies at snapshot time
    AgcBudgetState             budget;  // budget counters at snapshot time
    AgcTraceSnapState          trace;   // trace ops at snapshot time
};
