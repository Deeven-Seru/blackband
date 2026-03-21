// src/toolchain/debugger.cpp
#include "debugger.hpp"
#include <iostream>
#include <cstdlib>

namespace agentc {
namespace toolchain {

void NativeDebugger::run(const std::string& binary_target) {
    std::cout << "AgentC Native Debugger (wrapping LLDB) v0.1.0\n";
    std::cout << "Target: " << binary_target << "\n\n";

    // Since our binaries are native Mach-O / ELF, we can wrap standard LLDB
    // For maximum UX, we generate an LLDB init script to automatically break on run_agent
    std::string lldb_script = 
        "breakpoint set --name run_agent\n"
        "run\n";
    
    std::string script_path = "/tmp/__agc_lldb_init.txt";
    FILE* f = fopen(script_path.c_str(), "w");
    if (f) {
        fputs(lldb_script.c_str(), f);
        fclose(f);
    }

    std::string cmd = "lldb -s " + script_path + " " + binary_target;
    std::cout << "Launching debugger console...\n(Type 'continue' to resume execution or 'step' to walk lines)\n\n";

    int result = system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Debugger exited abnormally.\n";
    }
}

} // namespace toolchain
} // namespace agentc
