// runtime/modules/trc.hpp
#pragma once
#include "../agentc_runtime.hpp"
#include <cstddef>

extern "C" {
    // Snapshot integration helpers used by runtime/snapshot.cpp
    size_t agc_trc_op_count();
    void   agc_trc_trim(size_t len);
}
