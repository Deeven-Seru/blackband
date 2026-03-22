// runtime/snapshot.cpp
// Snapshot/restore implementation for AgentC runtime.
//
// Phases implemented:
//   Phase 1 — Heap memory: tracks all agc_alloc() regions via a thread-local
//              registry; snapshot copies every region; restore frees
//              post-snapshot allocations and writes back pre-snapshot data.
//   Phase 2 — Budget state: saves/restores g_budget_cap/used/peak via
//              helpers in ctx.cpp.
//   Phase 3 — Trace state: saves/restores the execution trace log via
//              helpers in trc.cpp.
//
// Thread-safety: all mutable state is thread_local — no cross-thread sharing.
// Nested snapshots: each agc_snapshot() returns an independent AgcSnapshot*;
//   restore consumes and frees it, so stacks of snapshots work naturally.

#include "snapshot.hpp"
#include <unordered_set>

thread_local std::vector<AgcAllocEntry> g_alloc_registry;

// ── Registry helpers (called from mem.cpp) ────────────────────────────────────

void agc_alloc_register(void* ptr, int64_t size) {
    g_alloc_registry.push_back({ptr, size});
}

void agc_alloc_unregister(void* ptr) {
    for (size_t i = 0; i < g_alloc_registry.size(); ++i) {
        if (g_alloc_registry[i].ptr == ptr) {
            g_alloc_registry.erase(
                g_alloc_registry.begin() + static_cast<ptrdiff_t>(i));
            return;
        }
    }
}

// Free a snapshot's resources without restoring any state.
// Used by agc_trc_begin() and agc_trace_restore_state() to avoid leaks.
void agc_snapshot_discard(void* raw) {
    if (!raw) return;
    auto* snap = static_cast<AgcSnapshot*>(raw);
    for (auto& region : snap->heap) {
        free(region.data_copy);
    }
    delete snap;
}

// ── C ABI ─────────────────────────────────────────────────────────────────────

extern "C" {

// agc_snapshot() — capture heap, budget, and trace state.
// Returns an opaque AgcSnapshot* owned by the caller.
// Pass it to agc_restore() to roll back, or discard it to commit.
// Returns nullptr if any memory allocation fails during snapshot creation.
void* agc_snapshot() {
    auto* snap = new AgcSnapshot();

    // Phase 1: copy all current heap allocations
    snap->heap.reserve(g_alloc_registry.size());
    for (const auto& entry : g_alloc_registry) {
        char* copy = static_cast<char*>(malloc(entry.size));
        if (!copy) {
            // Allocation failed — release all copies already taken and abort.
            for (auto& region : snap->heap) {
                free(region.data_copy);
            }
            delete snap;
            return nullptr;
        }
        memcpy(copy, entry.ptr, entry.size);
        snap->heap.push_back({entry.ptr, entry.size, copy});
    }

    // Phase 2: budget state
    agc_budget_save(&snap->budget);

    // Phase 3: trace state
    agc_trace_save(&snap->trace);

    return snap;
}

// agc_restore() — roll back heap, budget, and trace to a previous checkpoint.
// Post-snapshot allocations are freed; pre-snapshot data is written back.
// The snapshot is consumed (freed) by this call.
void agc_restore(void* raw) {
    if (!raw) return;
    auto* snap = static_cast<AgcSnapshot*>(raw);

    // Phase 1: heap restoration
    // Build a set of pointers that existed at snapshot time for O(n) look-up.
    std::unordered_set<void*> snap_ptrs;
    snap_ptrs.reserve(snap->heap.size());
    for (const auto& r : snap->heap) {
        snap_ptrs.insert(r.ptr);
    }

    // Free every allocation that was made after the snapshot was taken.
    for (const auto& entry : g_alloc_registry) {
        if (snap_ptrs.count(entry.ptr) == 0) {
            free(entry.ptr);
        }
    }

    // Rebuild the registry to exactly match the snapshot.
    g_alloc_registry.clear();
    for (const auto& region : snap->heap) {
        g_alloc_registry.push_back({region.ptr, region.size});
    }

    // Restore the contents of each pre-snapshot allocation.
    for (const auto& region : snap->heap) {
        if (region.data_copy) {
            memcpy(region.ptr, region.data_copy, region.size);
        }
    }

    // Phase 2: budget restoration
    agc_budget_restore_state(&snap->budget);

    // Phase 3: trace restoration
    agc_trace_restore_state(&snap->trace);

    // Release per-region copies and the snapshot itself.
    for (auto& region : snap->heap) {
        free(region.data_copy);
    }
    delete snap;
}

} // extern "C"
