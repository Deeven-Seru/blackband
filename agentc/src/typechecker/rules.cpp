#include "rules.hpp"
#include <variant>
#include <optional>

namespace agentc {

EffectSet effects_from_annotations(const AnnotationBlock* ann) {
    EffectSet ef;
    if (!ann) return ef;
    for (const auto& a : ann->annotations) {
        if (a.kind == AnnotationNode::Kind::Effect && std::holds_alternative<std::vector<std::string>>(a.value)) {
            for (const auto& f : std::get<std::vector<std::string>>(a.value)) {
                if (f == "io") ef.io = true;
                if (f == "net") ef.net = true;
                if (f == "mem") ef.mem = true;
                if (f == "sys") ef.sys = true;
                if (f == "agt") ef.agt = true;
                if (f == "llm") ef.llm = true;
            }
        }
    }
    return ef;
}

CapSet caps_from_annotations(const AnnotationBlock* ann) {
    CapSet cap;
    if (!ann) return cap;
    for (const auto& a : ann->annotations) {
        if (a.kind == AnnotationNode::Kind::Capability && std::holds_alternative<std::vector<std::string>>(a.value)) {
            for (const auto& f : std::get<std::vector<std::string>>(a.value)) {
                if (f == "fs") cap.fs = true;
                if (f == "net") cap.net = true;
                if (f == "llm") cap.llm = true;
                if (f == "agt") cap.agt = true;
                if (f == "sys") cap.sys = true;
            }
        }
    }
    return cap;
}

uint32_t budget_cap_from_annotations(const AnnotationBlock* ann) {
    if (!ann) return 0;
    for (const auto& a : ann->annotations) {
        if (a.kind == AnnotationNode::Kind::Cost && std::holds_alternative<std::vector<std::string>>(a.value)) {
            for (const auto& f : std::get<std::vector<std::string>>(a.value)) {
                size_t colon = f.find(':');
                if (colon != std::string::npos && f.substr(0, colon) == "llm") {
                    return std::stoul(f.substr(colon + 1));
                }
            }
        }
    }
    return 0;
}

std::vector<std::pair<std::string,std::string>> check_annotation_conflicts(const AnnotationBlock* ann) {
    std::vector<std::pair<std::string,std::string>> errs;
    if (!ann) return errs;
    
    bool has_none_effect = false;
    bool has_io_net_mem_agt_cost = false;
    bool has_llm_cost = false;
    bool has_agt_cost = false;
    
    bool has_llm_cap = false;
    bool has_agt_cap = false;
    
    bool has_tru_trust = false;
    bool has_utr_trust = false;
    
    for (const auto& a : ann->annotations) {
        if (a.kind == AnnotationNode::Kind::Effect && std::holds_alternative<std::vector<std::string>>(a.value)) {
            for (const auto& f : std::get<std::vector<std::string>>(a.value)) {
                if (f == "none") has_none_effect = true;
            }
        }
        if (a.kind == AnnotationNode::Kind::Cost && std::holds_alternative<std::vector<std::string>>(a.value)) {
            for (const auto& f : std::get<std::vector<std::string>>(a.value)) {
                size_t colon = f.find(':');
                std::string k = (colon != std::string::npos) ? f.substr(0, colon) : f;
                if (k == "io" || k == "net" || k == "mem" || k == "agt") has_io_net_mem_agt_cost = true;
                if (k == "llm") has_llm_cost = true;
                if (k == "agt") has_agt_cost = true;
            }
        }
        if (a.kind == AnnotationNode::Kind::Capability && std::holds_alternative<std::vector<std::string>>(a.value)) {
            for (const auto& f : std::get<std::vector<std::string>>(a.value)) {
                if (f == "llm") has_llm_cap = true;
                if (f == "agt") has_agt_cap = true;
            }
        }
        if (a.kind == AnnotationNode::Kind::Trust && std::holds_alternative<std::vector<std::string>>(a.value)) {
            for (const auto& f : std::get<std::vector<std::string>>(a.value)) {
                if (f == "tru") has_tru_trust = true;
                if (f == "utr") has_utr_trust = true;
            }
        }
        if (a.kind == AnnotationNode::Kind::Confidence && std::holds_alternative<double>(a.value)) {
            if (std::get<double>(a.value) > 1.0) errs.push_back({"E804", "Confidence score #?(N) must be <= 1.0"});
        }
    }
    
    if (has_none_effect && has_io_net_mem_agt_cost) errs.push_back({"E802", "#!(none) combined with #$(io|net|mem)"});
    if (has_tru_trust && has_utr_trust) errs.push_back({"E803", "#~(tru) combined with #~(utr)"});
    if (has_llm_cost && !has_llm_cap) errs.push_back({"E805", "#$(llm:N) without #&(llm)"});
    if (has_agt_cost && !has_agt_cap) errs.push_back({"E806", "#$(agt) without #&(agt)"});
    
    return errs;
}

bool has_intent_annotation(const AnnotationBlock* ann) {
    if (!ann) return false;
    for (const auto& a : ann->annotations) {
        if (a.kind == AnnotationNode::Kind::Intent) return true;
    }
    return false;
}

EffectSet collect_stmt_effects(const StmtNode& stmt) {
    EffectSet ef;
    // Walk AST properly
    if (stmt.kind == StmtNode::Kind::Expr || stmt.kind == StmtNode::Kind::LetBind || stmt.kind == StmtNode::Kind::MutBind || stmt.kind == StmtNode::Kind::Assign) {
        // Evaluate expression effects explicitly
        auto walk_expr = [&](const ExprNode* e, auto& ref) -> void {
            if (!e) return;
            if (e->kind == ExprNode::Kind::Spawn || e->kind == ExprNode::Kind::Send || e->kind == ExprNode::Kind::Recv || e->kind == ExprNode::Kind::Kill || e->kind == ExprNode::Kind::Sync) ef.agt = true;
            if (e->kind == ExprNode::Kind::Snapshot || e->kind == ExprNode::Kind::Restore) ef.sys = true;
            // Detect stdlib io/net/llm effects from Call nodes
            if (e->kind == ExprNode::Kind::Call) {
                // fn_name is stored in str_val + field_name on the Call node itself
                std::string fn;
                if (!e->str_val.empty()) {
                    fn = e->str_val;
                    if (!e->field_name.empty()) fn += "::" + e->field_name;
                }
                if (!fn.empty()) {
                    if (fn.find("io::") == 0 || fn.find("fs::") == 0) ef.io = true;
                    if (fn.find("net::") == 0) ef.net = true;
                    if (fn.find("ctx::") == 0) ef.llm = true;
                    if (fn.find("agt::") == 0) ef.agt = true;
                    if (fn.find("sys::") == 0) ef.sys = true;
                }
            }
            for (const auto& c : e->children) ref(&c, ref);
        };
        walk_expr(stmt.expr.get(), walk_expr);
        walk_expr(stmt.bind_expr.get(), walk_expr);
    }
    
    if (stmt.then_branch) ef = ef.merge(collect_stmt_effects(*stmt.then_branch));
    if (stmt.else_branch) ef = ef.merge(collect_stmt_effects(*stmt.else_branch));
    for (const auto& arm : stmt.match_arms) ef = ef.merge(collect_stmt_effects(arm.second));
    for (const auto& s : stmt.block_stmts) ef = ef.merge(collect_stmt_effects(s));
    
    return ef;
}

bool is_result_type(const TypeNode& t) {
    return t.kind == TypeNode::Kind::Result || t.kind == TypeNode::Kind::AgtResult;
}

bool is_owned(const TypeNode& t) {
    return t.kind == TypeNode::Kind::Own || t.kind == TypeNode::Kind::Ch || t.kind == TypeNode::Kind::Agt;
}

TypeNode make_utr(TypeNode inner) {
    TypeNode t; t.kind = TypeNode::Kind::Utr; t.params.push_back(std::move(inner)); return t;
}

TypeNode make_tru(TypeNode inner) {
    TypeNode t; t.kind = TypeNode::Kind::Tru; t.params.push_back(std::move(inner)); return t;
}

TypeNode make_unit() {
    TypeNode t; t.kind = TypeNode::Kind::Unit; return t;
}

TypeNode make_named(std::string name) {
    TypeNode t; t.kind = TypeNode::Kind::Named; t.name = std::move(name); return t;
}

TypeNode make_result(TypeNode ok, TypeNode err) {
    TypeNode t; t.kind = TypeNode::Kind::Result; 
    t.ok_type = std::make_unique<TypeNode>(std::move(ok)); 
    t.err_type = std::make_unique<TypeNode>(std::move(err));
    return t;
}

std::string type_name(const TypeNode& t) {
    switch (t.kind) {
        case TypeNode::Kind::I8: return "i8"; case TypeNode::Kind::U8: return "u8";
        case TypeNode::Kind::I16: return "i16"; case TypeNode::Kind::U16: return "u16";
        case TypeNode::Kind::I32: return "i32"; case TypeNode::Kind::U32: return "u32";
        case TypeNode::Kind::I64: return "i64"; case TypeNode::Kind::U64: return "u64";
        case TypeNode::Kind::I128: return "i128"; case TypeNode::Kind::U128: return "u128";
        case TypeNode::Kind::F32: return "f32"; case TypeNode::Kind::F64: return "f64";
        case TypeNode::Kind::Bool: return "B"; case TypeNode::Kind::Unit: return "()";
        case TypeNode::Kind::Str: return "Str"; case TypeNode::Kind::Tru: return "Tru<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Utr: return "Utr<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Aim: return "Aim<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Own: return "Own<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Ref: return "Ref<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Shr: return "Shr<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Cnf: return "Cnf<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Opt: return "Opt<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Lst: return "Lst<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Bnd: return "Bnd<...>";
        case TypeNode::Kind::Map: return "Map<...>";
        case TypeNode::Kind::Ch: return "Ch<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Result: return (t.ok_type ? type_name(*t.ok_type) : "()") + "?" + (t.err_type ? type_name(*t.err_type) : "E");
        case TypeNode::Kind::AgtResult: return "AgtR<" + (t.params.empty()?"":type_name(t.params[0])) + ">";
        case TypeNode::Kind::Named: return t.name;
        default: return "Unknown";
    }
}

TypeNode clone_type(const TypeNode& t) {
    TypeNode c; c.loc = t.loc; c.kind = t.kind; c.name = t.name;
    c.confidence = t.confidence; c.str_bound = t.str_bound;
    for (const auto& p : t.params) c.params.push_back(clone_type(p));
    for (const auto& p : t.pipe_layers) c.pipe_layers.push_back(clone_type(p));
    if (t.ok_type) c.ok_type = std::make_unique<TypeNode>(clone_type(*t.ok_type));
    if (t.err_type) c.err_type = std::make_unique<TypeNode>(clone_type(*t.err_type));
    return c;
}

} // namespace agentc
