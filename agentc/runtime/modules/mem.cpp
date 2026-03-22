// runtime/modules/mem.cpp
#include "mem.hpp"
#include "../snapshot.hpp"

extern "C" {

void* agc_alloc(int64_t size) {
    if (size <= 0) return nullptr;
    void* ptr = malloc(size);
    if (ptr) agc_alloc_register(ptr, size);
    return ptr;
}

void agc_free(void* ptr) {
    if (ptr) {
        agc_alloc_unregister(ptr);
        free(ptr);
    }
}

} // extern "C"
