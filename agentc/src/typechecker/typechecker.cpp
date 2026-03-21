#include "typechecker.hpp"
#include <iostream>
#include <sstream>
#include <optional>
#include <variant>

namespace agentc {

TypeChecker::TypeChecker(std::string_view filename) : filename_(filename) {}

bool TypeChecker::has_errors() const { return !errors_.empty(); }
bool TypeChecker::has_warnings() const { return !warnings_.empty(); }

std::string TypeChecker::errors_as_json() const {
    std::ostringstream o; o << "[\n";
    for (size_t i = 0; i < errors_.size(); ++i) {
        o << "  " << error_to_json(errors_[i]);
        if (i < errors_.size() - 1) o << ",";
        o << "\n";
    }
    o << "\n]"; return o.str();
}

std::string TypeChecker::error_to_json(const TypeError& e) const {
    std::ostringstream o;
    o << "{\"err\":\"" << e.code << "\",\"cat\":\"" << e.category << "\",\"loc\":[" << e.loc.line << "," << e.loc.col << "]";
    if (!e.why_exp.empty() || !e.why_got.empty()) {
        o << ",\"why\":{\"exp\":\"" << e.why_exp << "\",\"got\":\"" << e.why_got << "\"}";
    }
    if (!e.fix_act.empty() || !e.fix_via.empty()) {
        o << ",\"fix\":{\"act\":\"" << e.fix_act << "\",\"via\":\"" << e.fix_via << "\"}";
    }
    if (!e.alts.empty()) {
        o << ",\"alt\":[";
        for (size_t i=0; i<e.alts.size(); ++i) { o << "\"" << e.alts[i] << "\""; if(i<e.alts.size()-1) o<<","; }
        o << "]";
    } else {
        o << ",\"alt\":[\"review specs\"]";
    }
    if (!e.patches.empty()) {
        o << ",\"patch\":[";
        for (size_t i=0; i<e.patches.size(); ++i) {
            auto& p = e.patches[i];
            o << "{\"op\":\"" << p.op << "\",\"loc\":[" << p.line << "," << p.col << "],\"old\":\"" << p.old_text << "\",\"new\":\"" << p.new_text << "\"}";
            if (i<e.patches.size()-1) o<<",";
        }
        o << "]";
    }
    if (!e.trace.empty()) {
        o << ",\"trace\":[";
        for (size_t i=0; i<e.trace.size(); ++i) {
            auto& t = e.trace[i];
            o << "{\"loc\":[" << t.line << "," << t.col << "],\"ctx\":\"" << t.ctx << "\"}";
            if (i<e.trace.size()-1) o<<",";
        }
        o << "]";
    }
    o << "}";
    return o.str();
}

void TypeChecker::error(std::string code, SourceLoc loc, std::string cat, std::string why_exp, std::string why_got, std::string fix_act, std::string fix_via, std::vector<std::string> alts, std::vector<PatchOp> patches, std::vector<TraceEntry> trace) {
    if (alts.empty()) alts.push_back("review specs");
    if (trace.size() > 2) trace.resize(2);
    errors_.push_back({code, cat, loc, why_exp, why_got, fix_act, fix_via, alts, patches, trace});
}

void TypeChecker::warning(std::string code, SourceLoc loc, std::string msg) {
    warnings_.push_back({code, loc, msg});
}

ProgramNode TypeChecker::check(ProgramNode ast) {
    env_.init_stdlib_sigs();
    register_program(ast);
    check_program(ast);
    return ast;
}

void TypeChecker::register_program(const ProgramNode& prog) {
    for (const auto& decl : prog.decls) {
        if (std::holds_alternative<FnNode>(decl)) register_fn(std::get<FnNode>(decl));
        else if (std::holds_alternative<AgtNode>(decl)) register_agent(std::get<AgtNode>(decl));
        else if (std::holds_alternative<DatNode>(decl)) register_dat(std::get<DatNode>(decl));
        else if (std::holds_alternative<EnmNode>(decl)) register_enm(std::get<EnmNode>(decl));
    }
}

void TypeChecker::register_fn(const FnNode& fn) { env_.fn_reg[fn.name] = &fn; }
void TypeChecker::register_agent(const AgtNode& agt) { env_.agt_reg[agt.name] = &agt; }
void TypeChecker::register_dat(const DatNode& dat) { env_.dat_reg[dat.name] = &dat; }
void TypeChecker::register_enm(const EnmNode& enm) { env_.enm_reg[enm.name] = &enm; }

void TypeChecker::check_program(ProgramNode& prog) {
    for (auto& decl : prog.decls) {
        if (std::holds_alternative<FnNode>(decl)) check_fn(std::get<FnNode>(decl));
        else if (std::holds_alternative<AgtNode>(decl)) check_agent(std::get<AgtNode>(decl));
    }
}

void TypeChecker::check_fn(FnNode& fn) {
    check_annotations(fn.annotations, fn.loc);
    check_intent_present(&fn.annotations, fn.loc);
    check_annotation_conflict(&fn.annotations, fn.loc);
    check_capability_consistency(fn.annotations, fn.loc);

    uint32_t budget = budget_cap_from_annotations(&fn.annotations);
    std::optional<TypeNode> opt_ret = fn.return_type.kind == TypeNode::Kind::Unit ? std::optional<TypeNode>() : clone_type(fn.return_type);
    env_.push_scope(std::move(opt_ret), &fn.annotations, budget);

    for (auto& p : fn.params) {
        env_.define({p.name, clone_type(p.type), p.is_mutable, false, p.loc, MemTier::Work});
    }

    check_block(fn.body);

    EffectSet actual = collect_stmt_effects(fn.body);
    enforce_effects(actual, &fn.annotations, fn.loc);
    check_budget(&fn.annotations, fn.body, fn.loc);

    env_.pop_scope();
}

void TypeChecker::check_agent(AgtNode& agt) {
    check_annotations(agt.annotations, agt.loc);
    check_fn(agt.run_fn);
}

void TypeChecker::check_annotations(const AnnotationBlock& ann, const SourceLoc& loc) { (void)ann; (void)loc; }

void TypeChecker::check_intent_present(const AnnotationBlock* ann, const SourceLoc& loc) {
    if (!has_intent_annotation(ann)) {
        error("E801", loc, "Semantic", "Intent #> required", "No intent", "Add #> intent", "manual", {"#>\"...\""}, {}, {});
    }
}

void TypeChecker::check_annotation_conflict(const AnnotationBlock* ann, const SourceLoc& loc) {
    auto errs = check_annotation_conflicts(ann);
    for (auto& e : errs) {
        error(e.first, loc, "Semantic", "", e.second, "Resolve conflict", "manual", {"remove macro"}, {}, {});
    }
}

void TypeChecker::check_capability_consistency(const AnnotationBlock& ann, const SourceLoc& loc) { (void)ann; (void)loc; }

void TypeChecker::check_block(StmtNode& stmt) {
    for (auto& s : stmt.block_stmts) {
        check_stmt(s);
    }
    if (stmt.block_tail) {
        TypeNode tail_ty = infer(*stmt.block_tail);
        if (auto current_ret = env_.current_return_type()) {
            if (!env_.is_assignable(tail_ty, *current_ret)) {
                error("E301", stmt.block_tail->loc, "Return", type_name(*current_ret), type_name(tail_ty), "Return exact type", "match sig", {"fix tail"}, {}, {});
            }
        }
    }
}

void TypeChecker::check_stmt(StmtNode& stmt) {
    if (stmt.kind == StmtNode::Kind::LetBind || stmt.kind == StmtNode::Kind::MutBind) check_binding(stmt);
    else if (stmt.kind == StmtNode::Kind::Assign) {
        TypeNode rhs = infer(*stmt.bind_expr);
        Binding* b = env_.lookup(stmt.bind_name);
        if (!b) { error("E209", stmt.loc, "Scope", "Declared binding", "Undefined", "Declare first", "letbind", {"$ " + stmt.bind_name}, {}, {}); return; }
        if (!b->is_mutable) {
            error("E201", stmt.loc, "Mutation", "Mutable binding", "Immutable", "Use ~ to mutate", "mutbind", {"~ " + stmt.bind_name}, {}, {});
        }
        enforce_trust(rhs, b->type, stmt.bind_expr->loc, b->defined_at);
        check_sem_insert(rhs, stmt.loc);
    }
    else if (stmt.kind == StmtNode::Kind::If) check_if(stmt);
    else if (stmt.kind == StmtNode::Kind::ForIn) check_for_in(stmt);
    else if (stmt.kind == StmtNode::Kind::Match) check_match(stmt);
    else if (stmt.kind == StmtNode::Kind::Expr) infer(*stmt.expr);
    else if (stmt.kind == StmtNode::Kind::Block) { 
        std::optional<TypeNode> old_ret;
        if (auto ret = env_.current_return_type()) old_ret = clone_type(*ret);
        env_.push_scope(std::move(old_ret), env_.current_annotations(), env_.current_budget_cap()); 
        check_block(stmt); 
        env_.pop_scope(); 
    }
}

void TypeChecker::check_binding(StmtNode& stmt) {
    TypeNode rhs = infer(*stmt.bind_expr);
    if (stmt.bind_type) {
        if (stmt.bind_type->kind == TypeNode::Kind::Str && !stmt.bind_type->str_bound.has_value()) {
            error("E208", stmt.loc, "Type", "Str<N>", "Str", "Add bound", "Str<256>", {"Str<4096>"}, {}, {});
        }
        enforce_trust(rhs, *stmt.bind_type, stmt.bind_expr->loc, stmt.loc);
    }
    
    MemTier tier = MemTier::Work;
    if (stmt.mem_scope_tier == "sem") {
        tier = MemTier::Semantic; 
        check_sem_insert(rhs, stmt.loc);
    }
    env_.define({stmt.bind_name, stmt.bind_type ? clone_type(*stmt.bind_type) : clone_type(rhs), stmt.kind == StmtNode::Kind::MutBind, false, stmt.loc, tier});
}

void TypeChecker::check_if(StmtNode& stmt) { infer(*stmt.condition); check_block(*stmt.then_branch); if(stmt.else_branch) check_block(*stmt.else_branch); }
void TypeChecker::check_loop(StmtNode& stmt) { check_block(*stmt.then_branch); }
void TypeChecker::check_for_in(StmtNode& stmt) { infer(*stmt.iter_expr); env_.push_scope(); env_.define({stmt.iter_var, make_unit(), false, false, stmt.loc, MemTier::Work}); check_block(*stmt.then_branch); env_.pop_scope(); }
void TypeChecker::check_match(StmtNode& stmt) { infer(*stmt.match_expr); for(auto& arm : stmt.match_arms) { env_.push_scope(); check_block(arm.second); env_.pop_scope(); } }

TypeNode TypeChecker::infer(ExprNode& expr) {
    switch(expr.kind) {
        case ExprNode::Kind::IntLit: { TypeNode t; t.kind = TypeNode::Kind::I32; return t; }
        case ExprNode::Kind::UintLit: { TypeNode t; t.kind = TypeNode::Kind::U32; return t; }
        case ExprNode::Kind::FloatLit: { TypeNode t; t.kind = TypeNode::Kind::F32; return t; }
        case ExprNode::Kind::BoolLit: { TypeNode t; t.kind = TypeNode::Kind::Bool; return t; }
        case ExprNode::Kind::StrLit: { TypeNode t; t.kind = TypeNode::Kind::Str; return t; }
        case ExprNode::Kind::UnitLit: return make_unit();
        case ExprNode::Kind::Ident: return infer_ident(expr);
        case ExprNode::Kind::Call: return infer_call(expr);
        case ExprNode::Kind::Propagate: return infer_propagate(expr);
        case ExprNode::Kind::ReturnOk: return infer_return_ok(expr);
        case ExprNode::Kind::ReturnErr: return infer_return_err(expr);
        case ExprNode::Kind::Spawn: case ExprNode::Kind::Kill: case ExprNode::Kind::Send: case ExprNode::Kind::Recv:
            return infer_at_primitive(expr);
        case ExprNode::Kind::MethodCall: return infer_field(expr);
        default: return make_unit();
    }
}

TypeNode TypeChecker::infer_ident(ExprNode& expr) {
    if (auto b = env_.lookup(expr.str_val)) {
        check_not_moved(expr.str_val, expr.loc);
        if (is_owned(b->type)) {
            env_.mark_moved(expr.str_val);
        }
        return clone_type(b->type);
    }
    return make_unit();
}

TypeNode TypeChecker::infer_call(ExprNode& expr) {
    // Call node: str_val = base name (e.g. "io"), field_name = method (e.g. "rd")
    // For simple calls: str_val = fn name, field_name empty
    std::string fn_name;
    if (!expr.str_val.empty()) {
        fn_name = expr.str_val;
        if (!expr.field_name.empty()) fn_name += "::" + expr.field_name;
    } else if (expr.children.size() > 0 && expr.children[0].kind == ExprNode::Kind::Ident) {
        fn_name = expr.children[0].str_val;
        if (!expr.children[0].field_name.empty()) fn_name += "::" + expr.children[0].field_name;
    }

    // Infer argument types to trigger move tracking for Own<T> values
    for (auto& child : expr.children) {
        infer(child);
    }

    if (env_.stdlib_sigs.count(fn_name)) {
        if (fn_name.find("io::") == 0) enforce_caps(CapSet{.fs=true}, env_.current_annotations(), expr.loc);
        if (fn_name.find("net::") == 0) enforce_caps(CapSet{.net=true}, env_.current_annotations(), expr.loc);
        if (fn_name.find("ctx::") == 0) enforce_caps(CapSet{.llm=true}, env_.current_annotations(), expr.loc);
        return clone_type(env_.stdlib_sigs[fn_name]);
    }
    if (env_.fn_reg.count(fn_name)) {
        return clone_type(env_.fn_reg[fn_name]->return_type);
    }

    // If fn_name could not be resolved, don't error for empty names (anonymous exprs)
    if (!fn_name.empty()) {
        error("E209", expr.loc, "Call", "Declared func", "Unknown fn: " + fn_name, "Link or declare", "import", {fn_name}, {}, {});
    }
    return make_unit();
}

TypeNode TypeChecker::infer_field(ExprNode& expr) { (void)expr; return make_unit(); }
TypeNode TypeChecker::infer_binary(ExprNode& expr) { (void)expr; return make_unit(); }
TypeNode TypeChecker::infer_unary(ExprNode& expr) { (void)expr; return make_unit(); }
TypeNode TypeChecker::infer_at_primitive(ExprNode& expr) { (void)expr; return make_unit(); }
TypeNode TypeChecker::infer_propagate(ExprNode& expr) { return infer(expr.children[0]); }
TypeNode TypeChecker::infer_return_ok(ExprNode& expr) { 
    TypeNode ok_ty = infer(expr.children[0]);
    if (auto current_ret = env_.current_return_type()) {
        if (current_ret->kind == TypeNode::Kind::Result) {
            if (current_ret->ok_type && env_.is_assignable(ok_ty, *current_ret->ok_type)) return clone_type(*current_ret);
        } else if (current_ret->kind == TypeNode::Kind::AgtResult) {
            if (current_ret->params.size() > 0 && env_.is_assignable(ok_ty, current_ret->params[0])) return clone_type(*current_ret);
            else if (current_ret->params.empty() && ok_ty.kind == TypeNode::Kind::Unit) return clone_type(*current_ret);
        }
    }
    return ok_ty; 
}
TypeNode TypeChecker::infer_return_err(ExprNode& expr) { 
    TypeNode err_ty = infer(expr.children[0]);
    if (auto current_ret = env_.current_return_type()) {
        if (current_ret->kind == TypeNode::Kind::Result) {
            if (current_ret->err_type && env_.is_assignable(err_ty, *current_ret->err_type)) return clone_type(*current_ret);
        } else if (current_ret->kind == TypeNode::Kind::AgtResult) {
            if (err_ty.kind == TypeNode::Kind::Named && err_ty.name == "AgtE") return clone_type(*current_ret);
        }
    }
    return err_ty; 
}

void TypeChecker::enforce_trust(const TypeNode& from, const TypeNode& to, const SourceLoc& use_loc, const SourceLoc& origin_loc) {
    if (to.kind == TypeNode::Kind::Tru && from.kind == TypeNode::Kind::Utr) {
        error("E301", use_loc, "Trust", "Tru<" + type_name(env_.inner_type(to)) + ">", "Utr<" + type_name(env_.inner_type(from)) + ">", "elevate_trust", "val::tru(raw)?", {"val::san(raw)?"}, {{"wrap", use_loc.line, use_loc.col, "", "val::tru($)?"}}, {{origin_loc.line, origin_loc.col, "Utr created here"}, {use_loc.line, use_loc.col, "Tru required here"}});
    }
}

void TypeChecker::check_sem_insert(const TypeNode& val_type, const SourceLoc& loc) {
    if (val_type.kind == TypeNode::Kind::Utr) {
        error("E703", loc, "Semantic", "Tru", "Utr", "Trust elevate", "val::tru", {}, {}, {});
    }
}

void TypeChecker::enforce_effects(const EffectSet& actual, const AnnotationBlock* ann, const SourceLoc& loc) {
    EffectSet declared = effects_from_annotations(ann);
    if (declared.pure() && !actual.pure()) {
        error("E601", loc, "Effect", "Pure", "Impure", "Declare effect", "#!(io)", {"#!(net)"}, {{"wrap", loc.line, loc.col, "#!(none)", "#!(FIXME)"}}, {});
    }
    if (actual.io && !declared.io) error("E602", loc, "Effect", "io", "none", "Add io effect", "!", {"#!(io)"}, {}, {});
    if (actual.net && !declared.net) error("E602", loc, "Effect", "net", "none", "Add net effect", "!", {"#!(net)"}, {}, {});
}

void TypeChecker::enforce_caps(const CapSet& required, const AnnotationBlock* ctx_ann, const SourceLoc& loc) {
    CapSet declared = caps_from_annotations(ctx_ann);
    if (required.fs && !declared.fs) error("E503", loc, "Cap", "fs", "none", "add #&(fs)", "", {"#&(fs)"}, {}, {});
    if (required.net && !declared.net) error("E504", loc, "Cap", "net", "none", "add #&(net)", "", {"#&(net)"}, {}, {});
    if (required.llm && !declared.llm) error("E502", loc, "Cap", "llm", "none", "add #&(llm)", "", {"#&(llm)"}, {}, {});
}

void TypeChecker::check_budget(const AnnotationBlock* ann, const StmtNode& body, const SourceLoc& loc) {
    uint32_t cap = budget_cap_from_annotations(ann);
    if (cap == 0) return;
    uint32_t estimated = estimate_block_cost(body);
    if (cap > 0) {
        float pct = (float)estimated / (float)cap;
        if (pct >= 1.0f) error("E701", loc, "Budget", "N/A", "N/A", "inc cap", "increase #$(llm)", {"#$(llm:2000)"}, {}, {});
        else if (pct >= 0.8f) warning("W701", loc, "Budget >= 80%");
    }
}

void TypeChecker::check_not_moved(const std::string& name, const SourceLoc& loc) {
    if (env_.is_moved(name)) error("E204", loc, "Move", "unmoved", "moved", "do not reuse", "", {"use refs"}, {}, {});
}

uint32_t TypeChecker::estimate_block_cost(const StmtNode& stmt) {
    uint32_t cost = 10; 
    for(auto& s : stmt.block_stmts) { (void)s; cost += 10; }
    return cost;
}

} // namespace agentc
