// src/toolchain/pkg.cpp
#include "pkg.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>

namespace agentc {
namespace toolchain {

namespace fs = std::filesystem;

std::string PackageManager::resolve(const std::string& uri) {
    // 1. Determine local cache path: ~/.agentc/pkg/{uri}
    const char* home = getenv("HOME");
    if (!home) return "";

    fs::path base_cache(home);
    base_cache /= ".agentc";
    base_cache /= "pkg";
    
    // Strip "http(s)://" if passed
    std::string clean_uri = uri;
    if (clean_uri.find("https://") == 0) clean_uri = clean_uri.substr(8);
    else if (clean_uri.find("http://") == 0) clean_uri = clean_uri.substr(7);

    fs::path target_path = base_cache / clean_uri;

    // 2. Clone or pull
    if (fs::exists(target_path)) {
        // Assume already cloned. To be robust, we could git pull here.
        std::cout << "[pkg] Cached locally: " << clean_uri << "\n";
    } else {
        std::cout << "[pkg] Fetching remote block: " << clean_uri << "...\n";
        fs::create_directories(target_path.parent_path());
        
        std::string git_cmd = "git clone https://" + clean_uri + " \"" + target_path.string() + "\" --depth 1 -q";
        int res = system(git_cmd.c_str());
        if (res != 0) {
            std::cerr << "Failed to fetch package: " << uri << "\n";
            return "";
        }
    }

    // 3. Sweep for all .agc files in the repository root (or recursively)
    std::stringstream combined_source;
    auto files = get_agc_files(target_path.string());
    
    for (const auto& f : files) {
        combined_source << read_file(f) << "\n";
    }

    return combined_source.str();
}

std::vector<std::string> PackageManager::get_agc_files(const std::string& dir) {
    std::vector<std::string> agc_files;
    if (!fs::exists(dir)) return agc_files;

    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".agc") {
            agc_files.push_back(entry.path().string());
        }
    }
    return agc_files;
}

std::string PackageManager::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace toolchain
} // namespace agentc
