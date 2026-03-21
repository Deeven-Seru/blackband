#pragma once
#include "typeenv.hpp"
#include "rules.hpp"
#include <vector>
#include <string>
#include <optional>
#include <variant>

namespace agentc {

struct PatchOp {
    std::string op;       // replace|insert|delete|wrap
    size_t      line, col;
    std::string old_text, new_text;
};

struct TraceEntry {
    size_t      line, col;
    std::string ctx;
};

struct TypeError {
    std::string              code, category;
    SourceLoc                loc;
    std::string              why_exp, why_got;
    std::string              fix_act, fix_via;
    std::vector<std::string> alts;
    std::vector<PatchOp>     patches;
    std::vector<TraceEntry>  trace;  // max 2
};

struct TypeWarning {
    std::string code;
    SourceLoc   loc;
    std::string message;
};

class TypeChecker {
public:
    explicit TypeChecker(std::string_view filename);

    ProgramNode check(ProgramNode ast);
    bool        has_errors()    const;
    bool        has_warnings()  const;
    std::string errors_as_json() const;
    const std::vector<TypeError>& get_errors() const { return errors_; }
    const std::vector<TypeWarning>& get_warnings() const { return warnings_; }

private:
    TypeEnv                  env_;
    std::string_view         filename_;
    std::vector<TypeError>   errors_;
    std::vector<TypeWarning> warnings_;

    // Pass 1
    void register_program(const ProgramNode& prog);
    void register_fn(const FnNode& fn);
    void register_agent(const AgtNode& agt);
    void register_dat(const DatNode& dat);
    void register_enm(const EnmNode& enm);

    // Pass 2
    void check_program(ProgramNode& prog);
    void check_fn(FnNode& fn);
    void check_agent(AgtNode& agt);

    // Annotation checks
    void check_annotations(const AnnotationBlock& ann, const SourceLoc& loc);
    void check_intent_present(const AnnotationBlock* ann, const SourceLoc& loc);
    void check_annotation_conflict(const AnnotationBlock* ann, const SourceLoc& loc);
    void check_capability_consistency(const AnnotationBlock& ann, const SourceLoc& loc);

    // Statement checks
    void      check_stmt(StmtNode& stmt);
    void      check_block(StmtNode& stmt);
    void      check_binding(StmtNode& stmt);
    void      check_if(StmtNode& stmt);
    void      check_loop(StmtNode& stmt);
    void      check_for_in(StmtNode& stmt);
    void      check_match(StmtNode& stmt);

    // Expression type inference
    TypeNode  infer(ExprNode& expr);
    TypeNode  infer_ident(ExprNode& expr);
    TypeNode  infer_call(ExprNode& expr);
    TypeNode  infer_binary(ExprNode& expr);
    TypeNode  infer_unary(ExprNode& expr);
    TypeNode  infer_propagate(ExprNode& expr);
    TypeNode  infer_return_ok(ExprNode& expr);
    TypeNode  infer_return_err(ExprNode& expr);
    TypeNode  infer_at_primitive(ExprNode& expr);
    TypeNode  infer_field(ExprNode& expr);

    // Rule enforcement
    void      enforce_trust(const TypeNode& from, const TypeNode& to,
                             const SourceLoc& use_loc, const SourceLoc& origin_loc);
    void      check_sem_insert(const TypeNode& val_type, const SourceLoc& loc);
    void      enforce_effects(const EffectSet& actual,
                               const AnnotationBlock* ann, const SourceLoc& loc);
    void      enforce_caps(const CapSet& required,
                            const AnnotationBlock* ctx_ann, const SourceLoc& loc);
    void      check_budget(const AnnotationBlock* ann,
                            const StmtNode& body, const SourceLoc& loc);
    void      check_not_moved(const std::string& name, const SourceLoc& loc);
    uint32_t  estimate_block_cost(const StmtNode& stmt);

    // Error/warning emission
    void error(std::string code, SourceLoc loc, std::string cat,
               std::string why_exp, std::string why_got,
               std::string fix_act, std::string fix_via,
               std::vector<std::string> alts,
               std::vector<PatchOp> patches,
               std::vector<TraceEntry> trace);

    void warning(std::string code, SourceLoc loc, std::string msg);
    std::string error_to_json(const TypeError& e) const;
};

} // namespace agentc
