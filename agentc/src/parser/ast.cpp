#include "ast.hpp"

namespace agentc {

std::string ast_to_json(const ProgramNode& prog) { (void)prog; return "{\"kind\":\"Program\"}"; }
std::string node_to_json(const FnNode& fn) { return "{\"kind\":\"fn\",\"name\":\""+fn.name+"\"}"; }
std::string node_to_json(const StmtNode& stmt) { (void)stmt; return "{\"kind\":\"stmt\"}"; }
std::string node_to_json(const ExprNode& expr) { (void)expr; return "{\"kind\":\"expr\"}"; }
std::string node_to_json(const TypeNode& type) { (void)type; return "{\"kind\":\"type\"}"; }

} // namespace agentc
