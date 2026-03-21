// runtime/modules/std_mod.cpp
#include "std_mod.hpp"
#include <iostream>
#include <chrono>

extern "C" {

void agc_print(AgcStr s) {
    if (s.ptr) {
        std::cout << std::string(s.ptr, s.len) << std::endl;
    } else {
        std::cout << "(null string)" << std::endl;
    }
}

void agc_panic(AgcStr msg) {
    if (msg.ptr) {
        std::cerr << "PANIC: " << std::string(msg.ptr, msg.len) << std::endl;
    } else {
        std::cerr << "PANIC: Unknown error" << std::endl;
    }
    abort();
}

int64_t agc_time() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

AgcStr agc_uid() {
    static int64_t counter = 0;
    std::string uid = "uid_" + std::to_string(agc_time()) + "_" + std::to_string(++counter);
    char* heap = agc_heap_alloc(uid.size() + 1);
    memcpy(heap, uid.c_str(), uid.size() + 1);
    return agc_make_str(heap);
}

} // extern "C"
