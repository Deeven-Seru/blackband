// runtime/modules/net.cpp
#include "net.hpp"
#include <curl/curl.h>
#include <string>
#include <sstream>

namespace {

// libcurl write callback
size_t write_cb(char* ptr, size_t size,
                size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

AgcResult http_request(const std::string& url,
                        const std::string& method,
                        const std::string& body = "") {
    CURL* curl = curl_easy_init();
    if (!curl) return agc_err(801, "net: curl init failed");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         (long)body.size());
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers,
                                "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "net error: %s\n", curl_easy_strerror(res));
        return agc_err(802, std::string("net: ") +
                       curl_easy_strerror(res));
    }

    if (http_code >= 400) {
        fprintf(stderr, "net HTTP error: %ld\n", http_code);
        return agc_err(803, "net: HTTP " +
                       std::to_string(http_code));
    }

    char* heap = agc_heap_alloc(response.size() + 1);
    memcpy(heap, response.c_str(), response.size() + 1);
    return agc_ok_str(heap, (int64_t)response.size());
}

// Strip HTML tags — reduces Wikipedia 500KB → clean text
static std::string strip_html(const std::string& html) {
    std::string result;
    result.reserve(html.size() / 4);
    bool in_tag    = false;
    bool in_script = false;
    bool in_style  = false;

    for (size_t i = 0; i < html.size(); i++) {
        // Detect <script> and <style> blocks to skip entirely
        if (!in_tag && html.substr(i, 7) == "<script") {
            in_script = true;
        }
        if (!in_tag && html.substr(i, 7) == "</scrip") {
            in_script = false; in_tag = true;
        }
        if (!in_tag && html.substr(i, 6) == "<style") {
            in_style = true;
        }
        if (!in_tag && html.substr(i, 7) == "</style") {
            in_style = false; in_tag = true;
        }

        if (in_script || in_style) continue;

        if (html[i] == '<') { in_tag = true; continue; }
        if (html[i] == '>') { in_tag = false; continue; }
        if (in_tag) continue;

        // Convert common HTML entities
        if (html.substr(i,6) == "&nbsp;") { result+=' '; i+=5; continue; }
        if (html.substr(i,4) == "&lt;")   { result+='<'; i+=3; continue; }
        if (html.substr(i,4) == "&gt;")   { result+='>'; i+=3; continue; }
        if (html.substr(i,5) == "&amp;")  { result+='&'; i+=4; continue; }

        result += html[i];
    }

    // Collapse whitespace
    std::string clean;
    clean.reserve(result.size());
    bool last_space = false;
    for (char c : result) {
        if (c == '\n' || c == '\t' || c == '\r') c = ' ';
        if (c == ' ') {
            if (!last_space) clean += c;
            last_space = true;
        } else {
            clean += c;
            last_space = false;
        }
    }

    // Truncate to 60KB max for LLM context safety
    if (clean.size() > 60000)
        clean = clean.substr(0, 60000);

    return clean;
}

} // namespace

extern "C" {

AgcResult agc_net_get(AgcStr url) {
    if (url.ptr == nullptr || url.len <= 0) return agc_err(801, "net::get: invalid url");
    AgcResult r = http_request(std::string(url.ptr, url.len), "GET");
    if (r.ok == 1 && r.val != 0) {
        char* heap = reinterpret_cast<char*>(r.val);
        std::string response(heap);
        agc_free(heap);

        if (response.find("<!DOCTYPE") != std::string::npos ||
            response.find("<html")     != std::string::npos) {
            response = strip_html(response);
        }
        if (response.size() > 60000)
            response = response.substr(0, 60000);

        char* new_heap = agc_heap_alloc(response.size() + 1);
        memcpy(new_heap, response.c_str(), response.size() + 1);
        return agc_ok_str(new_heap, (int64_t)response.size());
    }
    return r;
}

AgcResult agc_net_post(AgcStr url, AgcStr body) {
    if (url.ptr == nullptr || url.len <= 0) return agc_err(801, "net::post: invalid url");
    std::string b = body.ptr && body.len > 0 ? std::string(body.ptr, body.len) : "";
    return http_request(
        std::string(url.ptr, url.len),
        "POST",
        b);
}

AgcResult agc_net_dns(AgcStr host) {
    if (host.ptr == nullptr || host.len <= 0) return agc_err(804, "net::dns: invalid host");
    // Simplified: just return the host as-is for now
    char* heap = agc_heap_alloc(host.len + 1);
    memcpy(heap, host.ptr, host.len + 1);
    return agc_ok_str(heap, host.len);
}

} // extern "C"
