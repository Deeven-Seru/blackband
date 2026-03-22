// runtime/modules/mem.cpp
#include "mem.hpp"

extern "C" {

void* agc_alloc(int64_t size) {
    if (size <= 0) return nullptr;
    return malloc(size);
}

void agc_free(void* ptr) {
    if (ptr) free(ptr);
}

// agc_snapshot() and agc_restore() are implemented in runtime/snapshot.cpp.

} // extern "C"
