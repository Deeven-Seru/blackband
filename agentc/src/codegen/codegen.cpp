#include "codegen.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <system_error>

namespace agentc {

CodeGen::CodeGen(std::string_view module_name, std::string_view filename)
    : module_(std::make_unique<llvm::Module>(module_name, ctx_)),
      builder_(std::make_unique<llvm::IRBuilder<>>(ctx_)),
      filename_(filename) {}

bool CodeGen::has_errors() const {
    return !errors_.empty();
}

std::string CodeGen::errors_as_json() const {
    std::string s = "{\"errors\":[\n";
    for (size_t i = 0; i < errors_.size(); i++) {
        s += "  {\"msg\":\"" + errors_[i].msg + "\"}";
        if (i < errors_.size() - 1) s += ",";
        s += "\n";
    }
    s += "]}";
    return s;
}

void CodeGen::error(std::string msg, SourceLoc loc) {
    errors_.push_back({msg, loc});
}

llvm::Value* CodeGen::poison() {
    return llvm::UndefValue::get(llvm::Type::getInt64Ty(ctx_));
}

bool CodeGen::generate(const ProgramNode& prog) {
    gen_program(prog);
    return !has_errors();
}

void CodeGen::gen_program(const ProgramNode& prog) {
    declare_runtime_fns();
    bool has_main = false;
    for (auto& decl : prog.decls) {
        if (auto* fn = std::get_if<FnNode>(&decl)) {
            gen_fn_decl(*fn);
            if (fn->name == "main") has_main = true;
        }
        else if (auto* dat = std::get_if<DatNode>(&decl))
            gen_dat(*dat);
        else if (auto* enm = std::get_if<EnmNode>(&decl))
            gen_enm(*enm);
    }
    for (auto& decl : prog.decls) {
        if (auto* fn = std::get_if<FnNode>(&decl))
            if (!fn->is_external)
                gen_fn_body(*fn);
    }

    if (!has_main) {
        auto* fn_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx_), false);
        auto* main_fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, "main", module_.get());
        auto* entry = llvm::BasicBlock::Create(ctx_, "entry", main_fn);
        
        auto* old_fn = current_fn_;
        auto* old_blk = current_block_;
        current_fn_ = main_fn;
        current_block_ = entry;
        builder_->SetInsertPoint(entry);
        
        if (functions_.count("run_agent")) {
            builder_->CreateCall(functions_["run_agent"], {});
        }
        
        builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0));
        
        current_fn_ = old_fn;
        current_block_ = old_blk;
        if (current_block_) builder_->SetInsertPoint(current_block_);
    }
}

void CodeGen::declare_runtime_fns() {
    auto* i8ptr = llvm::PointerType::getUnqual(ctx_);
    auto* i64   = llvm::Type::getInt64Ty(ctx_);
    auto* i1    = llvm::Type::getInt1Ty(ctx_);
    auto* void_ = llvm::Type::getVoidTy(ctx_);

    auto* str_ty = llvm::StructType::get(ctx_, {i8ptr, i64});
    auto* res_ty = llvm::StructType::get(ctx_, {i1, i64});

    auto declare = [&](const std::string& name, llvm::Type* ret, std::vector<llvm::Type*> params) {
        auto* fn_ty = llvm::FunctionType::get(ret, params, false);
        auto* fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, module_.get());
        functions_[name] = fn;
        return fn;
    };

    declare("agc_io_rd",   res_ty, {str_ty});
    declare("agc_io_wr",   res_ty, {str_ty, str_ty});
    declare("agc_io_app",  res_ty, {str_ty, str_ty});
    declare("agc_io_ex",   i1,     {str_ty});
    declare("agc_io_del",  res_ty, {str_ty});
    declare("agc_io_ls",   res_ty, {str_ty});
    
    declare("agc_net_get", res_ty, {str_ty});
    declare("agc_net_post",res_ty, {str_ty, str_ty});
    declare("agc_net_dns", res_ty, {str_ty});
    
    declare("agc_val_tru", res_ty, {str_ty});
    declare("agc_val_san", res_ty, {str_ty});
    declare("agc_val_bnd", res_ty, {i64, i64, i64});
    declare("agc_val_vch", res_ty, {str_ty, str_ty});
    
    declare("agc_ctx_infer", res_ty, {str_ty});
    declare("agc_ctx_embed", res_ty, {str_ty});
    declare("agc_chunk",     res_ty, {str_ty, i64});
    declare("agc_summarize", res_ty, {str_ty});
    
    declare("agc_budget",        i64,  {});
    declare("agc_budget_used",   i64,  {});
    declare("agc_budget_inspect",str_ty,{});
    
    declare("agc_alloc",    i8ptr, {i64});
    declare("agc_free",     void_, {i8ptr});
    declare("agc_snapshot", i8ptr, {});
    declare("agc_restore",  void_, {i8ptr});
    
    declare("agc_print",   void_, {str_ty});
    declare("agc_panic",   void_, {str_ty});
    declare("agc_time",    i64,   {});
    declare("agc_uid",     str_ty,{});
    
    declare("strlen",      i64,   {i8ptr});
}

llvm::Type* CodeGen::llvm_type(const TypeNode& t) {
    switch (t.kind) {
    case TypeNode::Kind::I8:   return llvm::Type::getInt8Ty(ctx_);
    case TypeNode::Kind::I16:  return llvm::Type::getInt16Ty(ctx_);
    case TypeNode::Kind::I32:  return llvm::Type::getInt32Ty(ctx_);
    case TypeNode::Kind::I64:  return llvm::Type::getInt64Ty(ctx_);
    case TypeNode::Kind::I128: return llvm::Type::getInt128Ty(ctx_);
    case TypeNode::Kind::U8:   return llvm::Type::getInt8Ty(ctx_);
    case TypeNode::Kind::U16:  return llvm::Type::getInt16Ty(ctx_);
    case TypeNode::Kind::U32:  return llvm::Type::getInt32Ty(ctx_);
    case TypeNode::Kind::U64:  return llvm::Type::getInt64Ty(ctx_);
    case TypeNode::Kind::F32:  return llvm::Type::getFloatTy(ctx_);
    case TypeNode::Kind::F64:  return llvm::Type::getDoubleTy(ctx_);
    case TypeNode::Kind::Bool: return llvm::Type::getInt1Ty(ctx_);
    case TypeNode::Kind::Unit: return llvm::Type::getVoidTy(ctx_);
    case TypeNode::Kind::Never: return llvm::Type::getVoidTy(ctx_);
    case TypeNode::Kind::Str: {
        return llvm::StructType::get(ctx_, {
            llvm::PointerType::getUnqual(ctx_),
            llvm::Type::getInt64Ty(ctx_)
        });
    }
    case TypeNode::Kind::Tru:
    case TypeNode::Kind::Utr:
    case TypeNode::Kind::Aim:
    case TypeNode::Kind::Cst:
    case TypeNode::Kind::Cnf:
    case TypeNode::Kind::Own:
    case TypeNode::Kind::Ref:
    case TypeNode::Kind::Shr:
        if (!t.params.empty()) return llvm_type(t.params[0]);
        return llvm::PointerType::getUnqual(ctx_);
    case TypeNode::Kind::Opt: {
        if (t.params.empty()) return llvm::Type::getInt8Ty(ctx_);
        return llvm::StructType::get(ctx_, {
            llvm::Type::getInt1Ty(ctx_),
            llvm_type(t.params[0])
        });
    }
    case TypeNode::Kind::Result:
    case TypeNode::Kind::AgtResult: {
        return llvm::StructType::get(ctx_, {
            llvm::Type::getInt1Ty(ctx_),   // 1 = ok, 0 = err
            llvm::Type::getInt64Ty(ctx_)   // ok value or err code
        });
    }
    case TypeNode::Kind::Bnd:
        if (!t.params.empty()) return llvm_type(t.params[0]);
        return llvm::Type::getInt64Ty(ctx_);
    case TypeNode::Kind::Named: {
        auto it = struct_types_.find(t.name);
        if (it != struct_types_.end())
            return it->second;
        return llvm::PointerType::getUnqual(ctx_);
    }
    case TypeNode::Kind::Lst:
    case TypeNode::Kind::Map:
    case TypeNode::Kind::Tup:
    case TypeNode::Kind::Ch:
    case TypeNode::Kind::Agt:
        return llvm::PointerType::getUnqual(ctx_); // opaque ptr
    default:
        return llvm::Type::getInt64Ty(ctx_);
    }
}

bool CodeGen::is_result_llvm_type(llvm::Type* ty) {
    if (auto* st = llvm::dyn_cast<llvm::StructType>(ty)) {
        if (st->getNumElements() == 2 && st->getElementType(0)->isIntegerTy(1) && st->getElementType(1)->isIntegerTy(64)) {
            return true;
        }
    }
    return false;
}

void CodeGen::gen_fn_decl(const FnNode& fn) {
    std::vector<llvm::Type*> param_types;
    for (auto& p : fn.params) param_types.push_back(llvm_type(p.type));
    llvm::Type* ret_type = llvm_type(fn.return_type);
    auto* fn_type = llvm::FunctionType::get(ret_type, param_types, false);
    auto* llvm_fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, fn.name, module_.get());
    size_t i = 0;
    for (auto& arg : llvm_fn->args()) {
        if (i < fn.params.size()) arg.setName(fn.params[i++].name);
    }
    functions_[fn.name] = llvm_fn;
}

void CodeGen::gen_fn_body(const FnNode& fn) {
    auto* llvm_fn = functions_[fn.name];
    current_fn_   = llvm_fn;
    auto* entry = llvm::BasicBlock::Create(ctx_, "entry", llvm_fn);
    builder_->SetInsertPoint(entry);
    current_block_ = entry;
    size_t i = 0;
    for (auto& arg : llvm_fn->args()) {
        auto& param = fn.params[i++];
        auto* alloca = builder_->CreateAlloca(llvm_type(param.type), nullptr, param.name);
        builder_->CreateStore(&arg, alloca);
        values_[param.name] = alloca;
    }
    gen_block(fn.body);
    if (!current_block_->getTerminator()) {
        if (llvm_fn->getReturnType()->isVoidTy()) builder_->CreateRetVoid();
        else {
            llvm::Type* rt = llvm_fn->getReturnType();
            if (rt->isIntegerTy()) builder_->CreateRet(llvm::ConstantInt::get(rt, 0));
            else builder_->CreateRet(llvm::UndefValue::get(rt));
        }
    }
    std::string verify_err;
    llvm::raw_string_ostream verify_stream(verify_err);
    if (llvm::verifyFunction(*llvm_fn, &verify_stream)) {
        error("LLVM verification failed for fn '" + fn.name + "': " + verify_err, fn.loc);
    }
}

void CodeGen::gen_dat(const DatNode& dat) {
    std::vector<llvm::Type*> elements;
    for (const auto& field : dat.fields) {
        elements.push_back(llvm_type(field.type));
    }
    auto* st = llvm::StructType::create(ctx_, elements, dat.name);
    struct_types_[dat.name] = st;
}

void CodeGen::gen_enm(const EnmNode& enm) {
    (void)enm; // ENM maps cleanly directly statically to tag-struct unions logic, unimplemented Phase 4.
}

void CodeGen::gen_stmt(const StmtNode& stmt) {
    switch (stmt.kind) {
        case StmtNode::Kind::LetBind:
        case StmtNode::Kind::MutBind: gen_binding(stmt); break;
        case StmtNode::Kind::Assign: gen_assign(stmt); break;
        case StmtNode::Kind::Expr: if (stmt.expr) gen_expr(*stmt.expr); break;
        case StmtNode::Kind::Block: gen_block(stmt); break;
        case StmtNode::Kind::If: gen_if(stmt); break;
        case StmtNode::Kind::Loop: gen_loop(stmt); break;
        case StmtNode::Kind::ForIn: gen_for_in(stmt); break;
        case StmtNode::Kind::Match: gen_match(stmt); break;
        default: break;
    }
}

void CodeGen::gen_block(const StmtNode& stmt) {
    for (const auto& s : stmt.block_stmts) {
        if (!current_block_->getTerminator()) {
            gen_stmt(s);
        }
    }
}

void CodeGen::gen_binding(const StmtNode& stmt) {
    llvm::Value* init = nullptr;
    if (stmt.bind_expr) init = gen_expr(*stmt.bind_expr);
    llvm::Type* ty = stmt.bind_type ? llvm_type(*stmt.bind_type) : (init ? init->getType() : llvm::Type::getInt64Ty(ctx_));
    auto* alloca = builder_->CreateAlloca(ty, nullptr, stmt.bind_name);
    if (init) {
        if (init->getType() != ty) {
            if (init->getType()->isIntegerTy(64) && ty->isStructTy()) {
                init = cast_i64_to_str(init);
            } else if (!ty->isStructTy() && !init->getType()->isStructTy()) {
                init = builder_->CreateZExtOrBitCast(init, ty);
            }
        }
        builder_->CreateStore(init, alloca);
    }
    values_[stmt.bind_name] = alloca;
}

void CodeGen::gen_assign(const StmtNode& stmt) {
    auto it = values_.find(stmt.bind_name);
    if (it != values_.end() && stmt.bind_expr) {
        llvm::Value* val = gen_expr(*stmt.bind_expr);
        llvm::Type* pty = llvm::cast<llvm::AllocaInst>(it->second)->getAllocatedType();
        if (val->getType() != pty) {
            if (val->getType()->isIntegerTy(64) && pty->isStructTy()) {
                val = cast_i64_to_str(val);
            } else if (!pty->isStructTy() && !val->getType()->isStructTy()) {
                val = builder_->CreateZExtOrBitCast(val, pty);
            }
        }
        builder_->CreateStore(val, it->second);
    }
}

void CodeGen::gen_if(const StmtNode& stmt) {
    llvm::Value* cond = gen_expr(*stmt.condition);
    if (!cond->getType()->isIntegerTy(1)) {
        cond = builder_->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0));
    }
    auto* then_bb  = llvm::BasicBlock::Create(ctx_, "then", current_fn_);
    auto* else_bb  = llvm::BasicBlock::Create(ctx_, "else", current_fn_);
    auto* merge_bb = llvm::BasicBlock::Create(ctx_, "merge", current_fn_);

    builder_->CreateCondBr(cond, then_bb, else_bb);

    builder_->SetInsertPoint(then_bb);
    current_block_ = then_bb;
    if (stmt.then_branch) gen_block(*stmt.then_branch);
    if (!current_block_->getTerminator()) builder_->CreateBr(merge_bb);

    builder_->SetInsertPoint(else_bb);
    current_block_ = else_bb;
    if (stmt.else_branch) gen_block(*stmt.else_branch);
    if (!current_block_->getTerminator()) builder_->CreateBr(merge_bb);

    builder_->SetInsertPoint(merge_bb);
    current_block_ = merge_bb;
}

void CodeGen::gen_loop(const StmtNode& stmt) {
    auto* loop_bb  = llvm::BasicBlock::Create(ctx_, "loop", current_fn_);
    auto* after_bb = llvm::BasicBlock::Create(ctx_, "after", current_fn_);
    builder_->CreateBr(loop_bb);
    builder_->SetInsertPoint(loop_bb);
    current_block_ = loop_bb;
    if (stmt.then_branch) gen_block(*stmt.then_branch);
    if (!current_block_->getTerminator()) builder_->CreateBr(loop_bb);
    builder_->SetInsertPoint(after_bb);
    current_block_ = after_bb;
}

void CodeGen::gen_for_in(const StmtNode& stmt) {
    (void)stmt; // Not yet mapped
}

void CodeGen::gen_match(const StmtNode& stmt) {
    (void)stmt; // Not yet mapped fully in phase 4 baseline
}

llvm::Value* CodeGen::gen_expr(const ExprNode& expr) {
    switch (expr.kind) {
        case ExprNode::Kind::IntLit:
        case ExprNode::Kind::UintLit:
        case ExprNode::Kind::FloatLit:
        case ExprNode::Kind::BoolLit:
        case ExprNode::Kind::StrLit:
        case ExprNode::Kind::UnitLit: return gen_literal(expr);
        case ExprNode::Kind::Ident: return gen_ident(expr);
        case ExprNode::Kind::Add:
        case ExprNode::Kind::Sub:
        case ExprNode::Kind::Mul:
        case ExprNode::Kind::Div:
        case ExprNode::Kind::Eq:
        case ExprNode::Kind::Neq:
        case ExprNode::Kind::Lt:
        case ExprNode::Kind::Gt:
        case ExprNode::Kind::Leq:
        case ExprNode::Kind::Geq:
        case ExprNode::Kind::And:
        case ExprNode::Kind::Or: return gen_binary(expr);
        case ExprNode::Kind::Call: return gen_call(expr);
        case ExprNode::Kind::Propagate: return gen_propagate(expr);
        case ExprNode::Kind::ReturnOk: return gen_return_ok(expr);
        case ExprNode::Kind::ReturnErr: return gen_return_err(expr);
        default: return poison();
    }
}

llvm::Value* CodeGen::gen_literal(const ExprNode& expr) {
    switch (expr.kind) {
    case ExprNode::Kind::IntLit:
        // 42i → i32 by default (matches type annotation i32)
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), expr.int_val, true);
    case ExprNode::Kind::UintLit:
        // 1u → u32 by default
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), expr.uint_val, false);
    case ExprNode::Kind::FloatLit:
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), expr.float_val);
    case ExprNode::Kind::BoolLit:
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_), expr.bool_val ? 1 : 0);
    case ExprNode::Kind::StrLit: {
        auto* str_gv = builder_->CreateGlobalString(expr.str_val, ".str");
        llvm::Value* str_const = llvm::cast<llvm::Value>(str_gv);
        auto* str_ty = llvm::StructType::get(ctx_, { llvm::PointerType::getUnqual(ctx_), llvm::Type::getInt64Ty(ctx_) });
        llvm::Value* str_val = llvm::UndefValue::get(str_ty);
        str_val = builder_->CreateInsertValue(str_val, str_const, {0});
        str_val = builder_->CreateInsertValue(str_val, llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), expr.str_val.size()), {1});
        return str_val;
    }
    case ExprNode::Kind::UnitLit:
        return llvm::UndefValue::get(llvm::Type::getVoidTy(ctx_));
    default:
        return poison();
    }
}

llvm::Value* CodeGen::gen_ident(const ExprNode& expr) {
    auto it = values_.find(expr.str_val);
    if (it == values_.end()) {
        error("Unknown variable in codegen: " + expr.str_val, expr.loc);
        return poison();
    }
    auto* alloca = it->second;
    llvm::Type* ptrElemType = llvm::cast<llvm::AllocaInst>(alloca)->getAllocatedType();
    return builder_->CreateLoad(ptrElemType, alloca, expr.str_val);
}

llvm::Value* CodeGen::gen_binary(const ExprNode& expr) {
    if (expr.children.size() < 2) return poison();
    llvm::Value* lhs = gen_expr(expr.children[0]);
    llvm::Value* rhs = gen_expr(expr.children[1]);
    if (!lhs || !rhs) return poison();

    bool is_float = lhs->getType()->isFloatingPointTy();
    if (!is_float) { // Equalize sizes for Int types
        if (lhs->getType()->getIntegerBitWidth() < rhs->getType()->getIntegerBitWidth()) lhs = builder_->CreateSExt(lhs, rhs->getType());
        else if (rhs->getType()->getIntegerBitWidth() < lhs->getType()->getIntegerBitWidth()) rhs = builder_->CreateSExt(rhs, lhs->getType());
    }

    switch (expr.kind) {
    case ExprNode::Kind::Add: return is_float ? builder_->CreateFAdd(lhs, rhs) : builder_->CreateAdd(lhs, rhs);
    case ExprNode::Kind::Sub: return is_float ? builder_->CreateFSub(lhs, rhs) : builder_->CreateSub(lhs, rhs);
    case ExprNode::Kind::Mul: return is_float ? builder_->CreateFMul(lhs, rhs) : builder_->CreateMul(lhs, rhs);
    case ExprNode::Kind::Div: return is_float ? builder_->CreateFDiv(lhs, rhs) : builder_->CreateSDiv(lhs, rhs);
    case ExprNode::Kind::Eq:  return is_float ? builder_->CreateFCmpOEQ(lhs, rhs) : builder_->CreateICmpEQ(lhs, rhs);
    case ExprNode::Kind::Neq: return is_float ? builder_->CreateFCmpONE(lhs, rhs) : builder_->CreateICmpNE(lhs, rhs);
    case ExprNode::Kind::Lt:  return is_float ? builder_->CreateFCmpOLT(lhs, rhs) : builder_->CreateICmpSLT(lhs, rhs);
    case ExprNode::Kind::Gt:  return is_float ? builder_->CreateFCmpOGT(lhs, rhs) : builder_->CreateICmpSGT(lhs, rhs);
    case ExprNode::Kind::Leq: return is_float ? builder_->CreateFCmpOLE(lhs, rhs) : builder_->CreateICmpSLE(lhs, rhs);
    case ExprNode::Kind::Geq: return is_float ? builder_->CreateFCmpOGE(lhs, rhs) : builder_->CreateICmpSGE(lhs, rhs);
    case ExprNode::Kind::And: return builder_->CreateAnd(lhs, rhs);
    case ExprNode::Kind::Or:  return builder_->CreateOr(lhs, rhs);
    default: return poison();
    }
}

llvm::Value* CodeGen::gen_call(const ExprNode& expr) {
    std::string fn_name;
    if (!expr.str_val.empty()) {
        fn_name = expr.str_val;
        if (!expr.field_name.empty()) fn_name += "::" + expr.field_name;
    } else if (expr.children.size() > 0 && expr.children[0].kind == ExprNode::Kind::Ident) {
        fn_name = expr.children[0].str_val;
        if (!expr.children[0].field_name.empty()) fn_name += "::" + expr.children[0].field_name;
    }

    // stdlib translations
    if (fn_name == "io::rd") fn_name = "agc_io_rd";
    else if (fn_name == "io::wr") fn_name = "agc_io_wr";
    else if (fn_name == "io::ex") fn_name = "agc_io_ex";
    else if (fn_name == "io::del") fn_name = "agc_io_del";
    else if (fn_name == "io::ls") fn_name = "agc_io_ls";
    else if (fn_name == "net::get") fn_name = "agc_net_get";
    else if (fn_name == "net::post") fn_name = "agc_net_post";
    else if (fn_name == "val::tru") fn_name = "agc_val_tru";
    else if (fn_name == "val::san") fn_name = "agc_val_san";
    else if (fn_name == "val::bnd") fn_name = "agc_val_bnd";
    else if (fn_name == "val::vch") fn_name = "agc_val_vch";
    else if (fn_name == "agc_budget") fn_name = "agc_budget";
    else if (fn_name == "ctx::infer") fn_name = "agc_ctx_infer";
    else if (fn_name == "ctx::embed") fn_name = "agc_ctx_embed";
    else if (fn_name == "agc_print") fn_name = "agc_print";

    if (!functions_.count(fn_name)) {
        error("Call generation failed. Missing function reference: " + fn_name, expr.loc);
        return poison();
    }

    auto* callee = functions_[fn_name];
    auto* fn_ty = callee->getFunctionType();
    std::vector<llvm::Value*> args;
    size_t start_idx = (expr.str_val.empty() && expr.children.size() > 0 && expr.children[0].kind == ExprNode::Kind::Ident) ? 1 : 0;
    
    for (size_t i = start_idx; i < expr.children.size(); i++) {
        llvm::Value* arg_val = gen_expr(expr.children[i]);
        size_t param_idx = i - start_idx;
        if (param_idx < fn_ty->getNumParams()) {
            llvm::Type* pty = fn_ty->getParamType(param_idx);
            if (arg_val && arg_val->getType() != pty) {
                if (arg_val->getType()->isIntegerTy(64) && pty->isStructTy()) {
                    arg_val = cast_i64_to_str(arg_val);
                } else if (!pty->isStructTy() && !arg_val->getType()->isStructTy()) {
                    arg_val = builder_->CreateZExtOrBitCast(arg_val, pty);
                }
            }
        }
        if (arg_val) args.push_back(arg_val);
    }

    return builder_->CreateCall(callee, args);
}

llvm::Value* CodeGen::make_result_ok(llvm::Value* val) {
    auto* result_ty = llvm::StructType::get(ctx_, { llvm::Type::getInt1Ty(ctx_), llvm::Type::getInt64Ty(ctx_) });
    llvm::Value* result = llvm::UndefValue::get(result_ty);
    auto* ok_flag = llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_), 1);
    result = builder_->CreateInsertValue(result, ok_flag, {0});

    llvm::Value* stored_val;
    if (val->getType()->isIntegerTy(64)) stored_val = val;
    else if (val->getType()->isPointerTy()) stored_val = builder_->CreatePtrToInt(val, llvm::Type::getInt64Ty(ctx_));
    else if (val->getType()->isStructTy()) {
        llvm::Value* ptr = builder_->CreateExtractValue(val, {0});
        stored_val = builder_->CreatePtrToInt(ptr, llvm::Type::getInt64Ty(ctx_));
    } else stored_val = builder_->CreateZExtOrBitCast(val, llvm::Type::getInt64Ty(ctx_));

    return builder_->CreateInsertValue(result, stored_val, {1});
}

llvm::Value* CodeGen::cast_i64_to_str(llvm::Value* val) {
    auto* ctx_i64 = llvm::Type::getInt64Ty(ctx_);
    auto* ctx_i8ptr = llvm::PointerType::getUnqual(ctx_);
    auto* target_ty = llvm::StructType::get(ctx_, {ctx_i8ptr, ctx_i64});
    
    llvm::Value* ptr = builder_->CreateIntToPtr(val, ctx_i8ptr);
    llvm::Value* len = builder_->CreateCall(functions_["strlen"], {ptr});
    
    llvm::Value* str_struct = llvm::UndefValue::get(target_ty);
    str_struct = builder_->CreateInsertValue(str_struct, ptr, {0});
    str_struct = builder_->CreateInsertValue(str_struct, len, {1});
    return str_struct;
}

llvm::Value* CodeGen::make_result_err(int64_t errcode) {
    auto* result_ty = llvm::StructType::get(ctx_, { llvm::Type::getInt1Ty(ctx_), llvm::Type::getInt64Ty(ctx_) });
    llvm::Value* result = llvm::UndefValue::get(result_ty);
    auto* err_flag = llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_), 0);
    auto* err_val  = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), errcode);
    result = builder_->CreateInsertValue(result, err_flag, {0});
    result = builder_->CreateInsertValue(result, err_val,  {1});
    return result;
}

llvm::Value* CodeGen::gen_propagate(const ExprNode& expr) {
    if (expr.children.empty()) return poison();
    llvm::Value* result = gen_expr(expr.children[0]);
    if (!result) return poison();

    llvm::Value* ok_flag = builder_->CreateExtractValue(result, {0});

    auto* ok_bb   = llvm::BasicBlock::Create(ctx_, "prop_ok",  current_fn_);
    auto* err_bb  = llvm::BasicBlock::Create(ctx_, "prop_err", current_fn_);

    builder_->CreateCondBr(ok_flag, ok_bb, err_bb);

    builder_->SetInsertPoint(err_bb);
    current_block_ = err_bb;
    llvm::Value* err_val = builder_->CreateExtractValue(result, {1});
    llvm::Value* propagated = make_result_err(0);
    propagated = builder_->CreateInsertValue(propagated, err_val, {1});
    builder_->CreateRet(propagated);

    builder_->SetInsertPoint(ok_bb);
    current_block_ = ok_bb;
    return builder_->CreateExtractValue(result, {1});
}

llvm::Value* CodeGen::gen_return_ok(const ExprNode& expr) {
    llvm::Value* val = nullptr;
    if (!expr.children.empty()) val = gen_expr(expr.children[0]);

    llvm::Type* ret_ty = current_fn_->getReturnType();

    if (ret_ty->isVoidTy()) {
        builder_->CreateRetVoid();
    } else if (is_result_llvm_type(ret_ty)) {
        auto* result = val ? make_result_ok(val) : make_result_ok(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0));
        builder_->CreateRet(result);
    } else {
        builder_->CreateRet(val ? val : llvm::UndefValue::get(ret_ty));
    }

    auto* dead = llvm::BasicBlock::Create(ctx_, "dead", current_fn_);
    builder_->SetInsertPoint(dead);
    current_block_ = dead;

    return nullptr;
}

llvm::Value* CodeGen::gen_return_err(const ExprNode& expr) {
    (void)expr;
    auto* result = make_result_err(1);
    builder_->CreateRet(result);

    auto* dead = llvm::BasicBlock::Create(ctx_, "dead", current_fn_);
    builder_->SetInsertPoint(dead);
    current_block_ = dead;

    return nullptr;
}

bool CodeGen::emit_ir(const std::string& path) {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec);
    if (ec) {
        errors_.push_back({"Cannot open: " + path, {}});
        return false;
    }
    module_->print(out, nullptr);
    return true;
}

bool CodeGen::emit_executable(const std::string& output) {
    emit_ir("/tmp/agentc_out.ll");
    // Compile directly linking the runtime source natively into the final LLVM executable output via Clang
    std::string cmd = "clang -O2 /tmp/agentc_out.ll runtime/agentc_runtime.cpp runtime/modules/io.cpp runtime/modules/net.cpp runtime/modules/ctx.cpp runtime/modules/val.cpp runtime/modules/mem.cpp runtime/modules/trc.cpp -o " + output + " -lstdc++ -lcurl 2>&1";
    int ret = system(cmd.c_str());
    return ret == 0;
}

} // namespace agentc
