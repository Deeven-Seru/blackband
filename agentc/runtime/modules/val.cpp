// runtime/modules/val.cpp
#include "val.hpp"
#include <string>

extern "C" {

AgcResult agc_val_tru(AgcStr raw) {
    if (raw.ptr == nullptr || raw.len == 0)
        return agc_err(302, "val::tru: empty input rejected");

    for (int64_t i = 0; i < raw.len; i++) {
        if (raw.ptr[i] == '\0')
            return agc_err(302, "val::tru: null byte in input");
    }

    char* heap = agc_heap_alloc(raw.len + 1);
    memcpy(heap, raw.ptr, raw.len + 1);
    return agc_ok_str(heap, raw.len);
}

AgcResult agc_val_san(AgcStr raw) {
    if (raw.ptr == nullptr || raw.len == 0)
        return agc_err(302, "val::san: empty input");

    std::string s(raw.ptr, raw.len);
    std::string out;
    out.reserve(s.size());

    for (char c : s) {
        if (c >= 0x20 || c == '\t' || c == '\n' || c == '\r')
            out += c;
    }

    if (out.empty())
        return agc_err(302, "val::san: all chars stripped");

    char* heap = agc_heap_alloc(out.size() + 1);
    memcpy(heap, out.c_str(), out.size() + 1);
    return agc_ok_str(heap, (int64_t)out.size());
}

AgcResult agc_val_bnd(int64_t val, int64_t lo, int64_t hi) {
    if (val < lo || val > hi) {
        std::string msg = "val::bnd: " + std::to_string(val) +
                          " not in [" + std::to_string(lo) +
                          "," + std::to_string(hi) + "]";
        return agc_err(303, msg);
    }
    return agc_ok_i64(val);
}

AgcResult agc_val_vch(AgcStr vouched, AgcStr proof) {
    if (proof.ptr == nullptr || proof.len == 0)
        return agc_err(304, "val::vch: missing proof");
    if (vouched.ptr == nullptr || vouched.len == 0)
        return agc_err(304, "val::vch: empty data");

    char* heap = agc_heap_alloc(vouched.len + 1);
    memcpy(heap, vouched.ptr, vouched.len + 1);
    return agc_ok_str(heap, vouched.len);
}

} // extern "C"
