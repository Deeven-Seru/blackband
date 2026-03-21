// src/toolchain/debugger.hpp
#pragma once
#include <string>

namespace agentc {
namespace toolchain {

class NativeDebugger {
public:
    NativeDebugger() = default;
    
    // Launches the debug wrapper tracing the executed binary
    void run(const std::string& binary_target);

private:
    void attach_lldb(const std::string& target);
};

} // namespace toolchain
} // namespace agentc
