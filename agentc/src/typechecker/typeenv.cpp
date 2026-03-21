#include "typeenv.hpp"
#include "rules.hpp"

namespace agentc {

void TypeEnv::push_scope(std::optional<TypeNode> ret, const AnnotationBlock* ann, uint32_t budget) {
    Scope s;
    s.return_type = std::move(ret);
    s.fn_annotations = ann;
    s.budget_cap = budget;
    scopes_.push_back(std::move(s));
}

void TypeEnv::pop_scope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

void TypeEnv::define(Binding b) {
    if (scopes_.empty()) return;
    std::string name = b.name;
    scopes_.back().bindings[name] = std::move(b);
}

Binding* TypeEnv::lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->bindings.count(name)) return &(it->bindings[name]);
    }
    return nullptr;
}

bool TypeEnv::exists(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->bindings.count(name)) return true;
    }
    return false;
}

void TypeEnv::mark_moved(const std::string& name) {
    Binding* b = lookup(name);
    if (b) b->is_moved = true;
}

bool TypeEnv::is_moved(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->bindings.count(name)) return it->bindings.at(name).is_moved;
    }
    return false;
}

std::optional<TypeNode> TypeEnv::current_return_type() const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->return_type.has_value()) {
            return clone_type(it->return_type.value());
        }
    }
    return std::nullopt;
}

const AnnotationBlock* TypeEnv::current_annotations() const {
    if (!scopes_.empty()) return scopes_.front().fn_annotations;
    return nullptr;
}

uint32_t TypeEnv::current_budget_cap() const {
    if (!scopes_.empty()) return scopes_.front().budget_cap; // Assuming top level pushes fn scope budget
    return 0;
}

const TypeNode& TypeEnv::inner_type(const TypeNode& t) const {
    if (t.kind == TypeNode::Kind::Tru || t.kind == TypeNode::Kind::Utr || t.kind == TypeNode::Kind::Own || t.kind == TypeNode::Kind::Ref || t.kind == TypeNode::Kind::Shr) {
        if (!t.params.empty()) return t.params[0];
    }
    return t;
}

TrustLevel TypeEnv::trust_of(const TypeNode& t) const {
    if (t.kind == TypeNode::Kind::Tru) return TrustLevel::Trusted;
    if (t.kind == TypeNode::Kind::Utr) return TrustLevel::Untrusted;
    if (!t.params.empty()) return trust_of(t.params[0]);
    return TrustLevel::Untrusted;
}

bool TypeEnv::is_trusted(const TypeNode& t) const {
    return trust_of(t) == TrustLevel::Trusted;
}

bool TypeEnv::is_untrusted(const TypeNode& t) const {
    return trust_of(t) == TrustLevel::Untrusted;
}

bool TypeEnv::types_equal(const TypeNode& a, const TypeNode& b) const {
    if (a.kind != b.kind) return false;
    if (a.name != b.name) return false;
    if (a.params.size() != b.params.size()) return false;
    for (size_t i=0; i<a.params.size(); ++i) {
        if (!types_equal(a.params[i], b.params[i])) return false;
    }
    return true;
}

bool TypeEnv::is_assignable(const TypeNode& from, const TypeNode& to) const {
    if (types_equal(from, to)) return true;
    if (to.kind == TypeNode::Kind::Tru && from.kind == TypeNode::Kind::Utr) return false; // explicit violation
    
    // Handle named types, checking their underlying structure if registered
    if (to.kind == TypeNode::Kind::Named && from.kind == TypeNode::Kind::Named) {
        if (to.name == from.name) return true; // Same named type
        
        // Check if both are registered and compatible structurally
        auto it_to = dat_reg.find(to.name);
        auto it_from = dat_reg.find(from.name);

        if (it_to != dat_reg.end() && it_from != dat_reg.end()) {
            // If both are registered, compare their fields
            const auto& to_data = it_to->second;
            const auto& from_data = it_from->second;

            if (to_data->fields.size() != from_data->fields.size()) return false;

            for (size_t i = 0; i < to_data->fields.size(); ++i) {
                // Recursively check assignability of fields
                if (!is_assignable(from_data->fields[i].type, to_data->fields[i].type)) {
                    return false;
                }
            }
            return true; // All fields are assignable
        }
    }
    return false;
}

uint32_t TypeEnv::estimate_tokens(const TypeNode& t) const {
    switch(t.kind) {
        case TypeNode::Kind::I8: case TypeNode::Kind::I16: case TypeNode::Kind::I32: case TypeNode::Kind::I64: case TypeNode::Kind::I128:
        case TypeNode::Kind::U8: case TypeNode::Kind::U16: case TypeNode::Kind::U32: case TypeNode::Kind::U64: case TypeNode::Kind::U128:
        case TypeNode::Kind::F32: case TypeNode::Kind::F64: case TypeNode::Kind::Bool: case TypeNode::Kind::Unit:
            return 1;
        case TypeNode::Kind::Str: 
            if (t.str_bound.has_value()) return t.str_bound.value() / 4;
            return 4; // unknown bound
        case TypeNode::Kind::Tru: case TypeNode::Kind::Utr: case TypeNode::Kind::Aim: case TypeNode::Kind::Own: case TypeNode::Kind::Ref:
        case TypeNode::Kind::Cnf: case TypeNode::Kind::Shr:
            if (!t.params.empty()) return 1 + estimate_tokens(t.params[0]);
            return 1;
        case TypeNode::Kind::Bnd:
            if (!t.params.empty()) return 2 + estimate_tokens(t.params[0]);
            return 2;
        case TypeNode::Kind::Lst:
            if (!t.params.empty()) return 16 * estimate_tokens(t.params[0]);
            return 16;
        case TypeNode::Kind::Map:
            if (t.params.size() >= 2) return 16 * (estimate_tokens(t.params[0]) + estimate_tokens(t.params[1]));
            return 32;
        case TypeNode::Kind::Result:
            if (t.ok_type) return 1 + estimate_tokens(*t.ok_type);
            return 1;
        case TypeNode::Kind::Named: {
            if (dat_reg.count(t.name)) {
                uint32_t c = 0;
                for (const auto& field : dat_reg.at(t.name)->fields) {
                    c += estimate_tokens(field.type);
                }
                return c;
            }
            return 4;
        }
        default: return 4;
    }
}

void TypeEnv::charge_budget(uint32_t tokens) {
    if (!scopes_.empty()) scopes_.front().budget_used += tokens;
}

uint32_t TypeEnv::remaining_budget() const {
    if (scopes_.empty()) return 0;
    uint32_t cap = scopes_.front().budget_cap;
    uint32_t used = scopes_.front().budget_used;
    return cap > used ? cap - used : 0;
}

void TypeEnv::init_stdlib_sigs() {
    auto mk_str_bound = [](size_t b) { TypeNode t; t.kind = TypeNode::Kind::Str; t.str_bound = b; return t; };
    auto mk_utr = [](TypeNode inner) { TypeNode t; t.kind = TypeNode::Kind::Utr; t.params.push_back(std::move(inner)); return t; };
    auto mk_tru = [](TypeNode inner) { TypeNode t; t.kind = TypeNode::Kind::Tru; t.params.push_back(std::move(inner)); return t; };
    auto mk_u32 = []() { TypeNode t; t.kind = TypeNode::Kind::U32; return t; };
    auto mk_unit = []() { TypeNode t; t.kind = TypeNode::Kind::Unit; return t; };

    stdlib_sigs["io::rd"] = mk_utr(mk_str_bound(4096));
    stdlib_sigs["io::wr"] = mk_unit();
    stdlib_sigs["io::ls"] = mk_utr(mk_str_bound(4096));
    stdlib_sigs["net::get"] = mk_utr(mk_str_bound(4096));
    stdlib_sigs["net::post"]= mk_utr(mk_str_bound(4096));
    stdlib_sigs["val::tru"] = mk_tru(mk_str_bound(4096)); // will be verified strictly dynamically
    stdlib_sigs["val::san"] = mk_tru(mk_str_bound(4096));
    stdlib_sigs["val::vch"] = mk_tru(mk_str_bound(4096));
    stdlib_sigs["ctx::infer"]=mk_utr(mk_str_bound(4096));
    stdlib_sigs["ctx::embed"]=mk_utr(mk_str_bound(4096));
    stdlib_sigs["mem::budget"]=mk_u32();
    // std builtins — always available
    stdlib_sigs["agc_print"]  = mk_unit();
    stdlib_sigs["std::print"] = mk_unit();
    stdlib_sigs["std::panic"] = mk_unit();
}

} // namespace agentc
