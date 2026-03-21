// src/toolchain/repl.hpp
#pragma once
#include <string>

namespace agentc {
namespace toolchain {

class REPL {
public:
    REPL() = default;
    
    // Starts the interactive Read-Eval-Print Loop
    void run();

private:
    std::string session_buffer_;
    int execute_shadow(const std::string& attempt);
};

} // namespace toolchain
} // namespace agentc
