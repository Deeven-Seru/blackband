// src/toolchain/repl.cpp
#include "repl.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sstream>

namespace agentc {
namespace toolchain {

void REPL::run() {
    std::cout << "AgentC Interactive REPL v0.1.0\n";
    std::cout << "Type an expression or statement. Type 'exit' to quit.\n";

    // Base context with standard library
    session_buffer_ = "+> io::wr;\n+> net::get;\n+> ctx::infer;\n+> val::tru;\n\n";

    std::string line;
    while (true) {
        std::cout << "agc> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        // Create a shadow payload appending the new line inside the main agent
        std::string payload = session_buffer_ + 
            "#[ #>\"REPL Session\" #!(net|io|llm) #&(net|fs|llm) ]\n" +
            "ƒ run_agent() -> ()?AgtE {\n" +
            "    " + line + "\n" +
            "    ^(())\n" +
            "}\n";

        // Write to temporary shadow file
        std::ofstream out("/tmp/__agc_repl.agc");
        out << payload;
        out.close();

        // Compile it silently
        std::string cmd = "./agentc /tmp/__agc_repl.agc -o /tmp/__agc_repl_bin 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::cerr << "Failed to invoke compiler.\n";
            continue;
        }

        std::stringstream output;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output << buffer;
        }
        int status = pclose(pipe);

        if (status != 0) {
            // It failed to compile. Print the compiler diagnostics (JSON or error output)
            std::string out_str = output.str();
            // Filter out the Apple LLVM triple warning
            if (out_str.find("warning: overriding the module target triple") != std::string::npos) {
                size_t pos = out_str.find("]\"");
                if (pos != std::string::npos) {
                    out_str = out_str.substr(pos + 2);
                } else if ((pos = out_str.find('\n')) != std::string::npos) {
                    out_str = out_str.substr(pos + 1); // skip warning
                    if ((pos = out_str.find('\n')) != std::string::npos) out_str = out_str.substr(pos + 1); // skip 1 warning generated.
                }
            }
            std::cout << "Compiler Rejected:\n" << out_str << "\n";
        } else {
            // Success! The line is valid. Save it to the eternal session buffer conditionally?
            // Wait, if it's an assignment, we should add it outside the run loop if it's a global, 
            // but in the REPL, all lines are executed linearly. 
            // For now, execute the compiled binary to show side effects
            system("/tmp/__agc_repl_bin");
            
            // To make state persistent, we append it to the accumulating session
            // But only if it's an assignment or declaration, not just an expression print, 
            // to avoid re-executing side effects repeatedly! 
            // Better: just execute once. Real REPL state retention without re-triggering side effects 
            // requires AST walking or LLVM ORC JIT hooks. For shadow REPL, we accept it as a preview tool.
        }
    }
}

} // namespace toolchain
} // namespace agentc
