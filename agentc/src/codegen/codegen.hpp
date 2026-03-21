#pragma once

#include "../parser/ast.hpp"
#include "../typechecker/typechecker.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Target/TargetMachine.h"

#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

namespace agentc {

class CodeGen {
public:
    explicit CodeGen(std::string_view module_name,
                     std::string_view filename);

    // Generate LLVM IR for entire program
    // Returns false if errors occurred
    bool generate(const ProgramNode& prog);

    // Emit LLVM IR to .ll file
    bool emit_ir(const std::string& output_path);

    // Compile IR to native object file
    bool emit_object(const std::string& output_path);

    // Compile IR to native executable (calls clang/lld)
    bool emit_executable(const std::string& output_path);

    // Get IR as string (for testing/debugging)
    std::string get_ir() const;

    bool        has_errors() const;
    std::string errors_as_json() const;

private:
    llvm::LLVMContext                ctx_;
    std::unique_ptr<llvm::Module>    module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    std::string_view                 filename_;

    // Symbol tables
    std::unordered_map<std::string, llvm::Value*>       values_;
    std::unordered_map<std::string, llvm::Function*>    functions_;
    std::unordered_map<std::string, llvm::StructType*>  struct_types_;
    std::unordered_map<std::string, llvm::GlobalVariable*> globals_;

    // Current function context
    llvm::Function*    current_fn_   = nullptr;
    llvm::BasicBlock*  current_block_= nullptr;

    struct CodeGenError {
        std::string msg;
        SourceLoc loc;
    };
    std::vector<CodeGenError> errors_;

    // ── Top-level generation ──────────────────────
    void gen_program(const ProgramNode& prog);
    void gen_fn_decl(const FnNode& fn);   // Pass 1: declare
    void gen_fn_body(const FnNode& fn);   // Pass 2: define
    void gen_dat(const DatNode& dat);
    void gen_enm(const EnmNode& enm);
    void declare_runtime_fns();           // declare stdlib C fns

    // ── Statement generation ──────────────────────
    void gen_stmt(const StmtNode& stmt);
    void gen_block(const StmtNode& stmt);
    void gen_binding(const StmtNode& stmt);
    void gen_assign(const StmtNode& stmt);
    void gen_if(const StmtNode& stmt);
    void gen_loop(const StmtNode& stmt);
    void gen_for_in(const StmtNode& stmt);
    void gen_match(const StmtNode& stmt);

    // ── Expression generation ─────────────────────
    llvm::Value* gen_expr(const ExprNode& expr);
    llvm::Value* gen_literal(const ExprNode& expr);
    llvm::Value* gen_ident(const ExprNode& expr);
    llvm::Value* gen_binary(const ExprNode& expr);
    llvm::Value* gen_unary(const ExprNode& expr);
    llvm::Value* gen_call(const ExprNode& expr);
    llvm::Value* gen_propagate(const ExprNode& expr);
    llvm::Value* gen_return_ok(const ExprNode& expr);
    llvm::Value* gen_return_err(const ExprNode& expr);
    llvm::Value* gen_at_primitive(const ExprNode& expr);
    llvm::Value* gen_field(const ExprNode& expr);

    // ── Type helpers ──────────────────────────────
    llvm::Type*  llvm_type(const TypeNode& t);
    bool         is_result_llvm_type(llvm::Type* ty);
    llvm::Value* make_result_ok(llvm::Value* val);
    llvm::Value* make_result_err(int64_t errcode);
    llvm::Value* result_is_ok(llvm::Value* result);
    llvm::Value* result_get_val(llvm::Value* result);
    llvm::Value* result_get_err(llvm::Value* result);
    llvm::Value* cast_i64_to_str(llvm::Value* val);

    // ── Stdlib call mapping ───────────────────────
    llvm::Value* gen_stdlib_call(const std::string& fn_name,
                                  const std::vector<llvm::Value*>& args);

    // ── Error emission ────────────────────────────
    void error(std::string msg, SourceLoc loc);
    llvm::Value* poison();  // return undef for error recovery
};

} // namespace agentc
