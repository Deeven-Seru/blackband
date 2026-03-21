// src/toolchain/pkg.hpp
#pragma once
#include <string>
#include <vector>

namespace agentc {
namespace toolchain {

class PackageManager {
public:
    PackageManager() = default;

    // Resolves a remote repository URL, clones/updates it locally, 
    // and returns the combined source text of all .agc files found inside.
    std::string resolve(const std::string& uri);

private:
    std::vector<std::string> get_agc_files(const std::string& dir);
    std::string read_file(const std::string& path);
};

} // namespace toolchain
} // namespace agentc
