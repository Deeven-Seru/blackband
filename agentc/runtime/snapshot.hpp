// runtime/snapshot.hpp
// Snapshot/Restore transaction system for AgentC runtime.
// Provides heap state checkpointing, budget rollback, and trace history
// restoration for speculative and retryable agent execution.
#pragma once
#include "agentc_runtime.hpp"
#include <cstdint>
#include <vector>

// ── Public C API ────────────────────────────────────────────────────────────
// (declarations also present in agentc_runtime.hpp for overall visibility)
extern "C" {

// Create a checkpoint of the current runtime state (heap allocations,
// budget usage, trace ops).  Returns an opaque handle.  The caller must
// eventually call agc_snapshot_discard() OR agc_restore() — never both.
void* agc_snapshot();

// Roll back all heap allocations, budget charges, and trace ops that
// occurred since the checkpoint identified by `snap`.  The snapshot handle
// is consumed and must not be used again.
void  agc_restore(void* snap);

// Discard a snapshot without restoring state (commit the work done
// since the checkpoint).
void  agc_snapshot_discard(void* snap);

} // extern "C"
