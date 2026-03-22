// runtime/snapshot.cpp
// Full transactional snapshot/restore system for AgentC runtime.
//
// Design
// ──────
// Every heap allocation made via agc_heap_alloc() is tracked in a
// thread-local allocation list.  A snapshot captures a "cursor" into that
// list (plus the budget counters and the trace-op count).  On restore, all
// allocations that happened after the cursor are freed and the budget /
// trace state are rewound.
//
// Nested snapshots are fully supported: the snapshot stack is a simple
// singly-linked list of SnapshotRecord values allocated on the C++ heap.
// Thread-safety is achieved via thread_local storage (each agent thread
// has its own independent state).

#include "snapshot.hpp"
#include "agentc_runtime.hpp"
#include "modules/trc.hpp"

#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdio>

// ── Forward declarations for budget state (defined in ctx.cpp) ──────────────
extern "C" {
    int64_t agc_budget_used();
    int64_t agc_budget_peak();
    void    agc_budget_set_cap(int64_t cap);
    int64_t agc_budget();      // returns remaining (cap - used)
}

// ── Allocation tracking ──────────────────────────────────────────────────────

// Every call to agc_heap_alloc() appends a record here.
// agc_restore() frees all records past the snapshot cursor.
struct HeapRecord {
    char*   ptr;    // allocated pointer
    size_t  size;   // original requested size (informational)
};

// Thread-local list of all live heap allocations.
static thread_local std::vector<HeapRecord> g_heap_allocs;

// ── Budget state mirror ──────────────────────────────────────────────────────
// We keep our own mirror of the budget counters so we can restore them
// without having to reach into ctx.cpp internals.  The ctx module's
// thread_local values are the authoritative source; we snapshot them here.

namespace {
    struct BudgetSnapshot {
        int64_t cap;
        int64_t used;
        int64_t peak;
    };
}

static BudgetSnapshot capture_budget() {
    int64_t rem  = agc_budget();   // remaining = cap - used
    int64_t used = agc_budget_used();
    int64_t peak = agc_budget_peak();
    return {rem + used, used, peak}; // cap = remaining + used
}

static void restore_budget(const BudgetSnapshot& b) {
    // Reset cap to saved cap, then charge only the tokens used at snapshot
    // time.  This effectively "refunds" any tokens spent since the snapshot.
    agc_budget_set_cap(b.cap);
    if (b.used > 0) {
        agc_budget_charge(b.used);
    }
}

// ── Snapshot record ──────────────────────────────────────────────────────────

struct SnapshotRecord {
    size_t          heap_cursor;   // index into g_heap_allocs at snapshot time
    BudgetSnapshot  budget;        // budget counters at snapshot time
    size_t          trc_cursor;    // trace op count at snapshot time
};

// ── Public C API ─────────────────────────────────────────────────────────────

extern "C" {

// agc_heap_alloc — central allocation function used by all runtime modules.
// Tracks every allocation so that agc_restore() can free them.
char* agc_heap_alloc(size_t n) {
    char* p = static_cast<char*>(malloc(n));
    if (p) g_heap_allocs.push_back({p, n});
    return p;
}

void* agc_snapshot() {
    auto* rec = new SnapshotRecord();
    rec->heap_cursor = g_heap_allocs.size();
    rec->budget      = capture_budget();
    rec->trc_cursor  = agc_trc_op_count();
    return rec;
}

void agc_restore(void* snap) {
    if (!snap) return;

    auto* rec = static_cast<SnapshotRecord*>(snap);

    // Free all heap allocations made since the snapshot.
    for (size_t i = rec->heap_cursor; i < g_heap_allocs.size(); i++) {
        free(g_heap_allocs[i].ptr);
    }
    g_heap_allocs.resize(rec->heap_cursor);

    // Restore budget counters.
    restore_budget(rec->budget);

    // Restore trace ops.
    agc_trc_trim(rec->trc_cursor);

    delete rec;
}

void agc_snapshot_discard(void* snap) {
    if (!snap) return;
    delete static_cast<SnapshotRecord*>(snap);
}

} // extern "C"
