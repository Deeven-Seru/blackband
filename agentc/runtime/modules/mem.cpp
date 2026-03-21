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

void* agc_snapshot() {
    // Stub definition for snapshot capabilities
    return nullptr;
}

void agc_restore(void* snap) {
    // Stub
    (void)snap;
}

} // extern "C"
