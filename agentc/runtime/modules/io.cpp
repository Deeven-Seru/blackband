// runtime/modules/io.cpp
#include "io.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

extern "C" {

// io::rd(path: Str<N>) -> IoR<Utr<Str>>
AgcResult agc_io_rd(AgcStr path) {
    if (path.ptr == nullptr || path.len < 0) return agc_err(701, "io::rd: invalid path");
    std::string p(path.ptr, (size_t)path.len);
    std::ifstream f(p, std::ios::binary);
    if (!f.is_open())
        return agc_err(701, "io::rd failed: cannot open " + p);

    std::ostringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();

    char* heap = agc_heap_alloc(content.size() + 1);
    memcpy(heap, content.c_str(), content.size() + 1);

    return agc_ok_str(heap, (int64_t)content.size());
}

// io::wr(path, data: Tru<Str>) -> IoR<()>
AgcResult agc_io_wr(AgcStr path, AgcStr data) {
    if (path.ptr == nullptr || path.len < 0) return agc_err(702, "io::wr: invalid path");
    std::string p(path.ptr, (size_t)path.len);
    std::ofstream f(p, std::ios::binary);
    if (!f.is_open())
        return agc_err(702, "io::wr failed: " + p);
    if (data.ptr && data.len > 0) {
        f.write(data.ptr, (std::streamsize)data.len);
    }
    return agc_ok_unit();
}

// io::app(path, data: Tru<Str>) -> IoR<()>
AgcResult agc_io_app(AgcStr path, AgcStr data) {
    if (path.ptr == nullptr || path.len < 0) return agc_err(702, "io::app: invalid path");
    std::string p(path.ptr, (size_t)path.len);
    std::ofstream f(p, std::ios::binary | std::ios::app);
    if (!f.is_open())
        return agc_err(702, "io::app failed: " + p);
    if (data.ptr && data.len > 0) {
        f.write(data.ptr, (std::streamsize)data.len);
    }
    return agc_ok_unit();
}

// io::ex(path) -> B
int8_t agc_io_ex(AgcStr path) {
    if (path.ptr == nullptr || path.len <= 0) return 0;
    std::string p(path.ptr, (size_t)path.len);
    std::error_code ec;
    return fs::exists(p, ec) ? 1 : 0;
}

// io::del(path) -> IoR<()>
AgcResult agc_io_del(AgcStr path) {
    if (path.ptr == nullptr || path.len <= 0) return agc_err(703, "io::del: invalid path");
    std::string p(path.ptr, (size_t)path.len);
    std::error_code ec;
    fs::remove(p, ec);
    if (ec) return agc_err(703, "io::del failed: " + p);
    return agc_ok_unit();
}

// io::ls(path) -> IoR<Lst<Str>>
AgcResult agc_io_ls(AgcStr path) {
    if (path.ptr == nullptr || path.len <= 0) return agc_err(704, "io::ls: invalid path");
    std::string p(path.ptr, (size_t)path.len);
    std::error_code ec;
    if (!fs::is_directory(p, ec))
        return agc_err(704, "io::ls: not a directory: " + p);

    std::string result = "[";
    bool first = true;
    for (auto& entry : fs::directory_iterator(p)) {
        if (!first) result += ",";
        result += "\"" + entry.path().filename().string() + "\"";
        first = false;
    }
    result += "]";

    char* heap = agc_heap_alloc(result.size() + 1);
    memcpy(heap, result.c_str(), result.size() + 1);
    return agc_ok_str(heap, (int64_t)result.size());
}

} // extern "C"
