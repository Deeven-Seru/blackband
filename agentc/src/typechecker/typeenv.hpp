#pragma once
#include "../parser/ast.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>

namespace agentc {

enum class MemTier { Work, Episodic, Semantic, External };
enum class TrustLevel { Untrusted, Trusted, System };

struct Binding {
    std::string  name;
    TypeNode     type;
    bool         is_mutable  = false;
    bool         is_moved    = false;
    SourceLoc    defined_at;
    MemTier      mem_tier    = MemTier::Work;
};

struct EffectSet {
    bool io=false, net=false, mem=false,
         sys=false, agt=false, llm=false;
    bool pure() const { return !io&&!net&&!mem&&!sys&&!agt&&!llm; }
    EffectSet merge(const EffectSet& o) const {
        return {io||o.io,net||o.net,mem||o.mem,
                sys||o.sys,agt||o.agt,llm||o.llm};
    }
};

struct CapSet {
    bool fs=false,net=false,llm=false,agt=false,sys=false;
    bool satisfies(const CapSet& req) const {
        if(req.fs  && !fs)  return false;
        if(req.net && !net) return false;
        if(req.llm && !llm) return false;
        if(req.agt && !agt) return false;
        if(req.sys && !sys) return false;
        return true;
    }
};

struct Scope {
    std::unordered_map<std::string, Binding> bindings;
    std::optional<TypeNode>  return_type;
    const AnnotationBlock*   fn_annotations = nullptr;
    uint32_t                 budget_cap  = 0;
    uint32_t                 budget_used = 0;
    EffectSet                declared_effects;
    CapSet                   declared_caps;
};

class TypeEnv {
public:
    void        push_scope(std::optional<TypeNode> ret={},
                           const AnnotationBlock* ann = nullptr,
                           uint32_t budget=0);
    void        pop_scope();
    void        define(Binding b);
    Binding*    lookup(const std::string& name);
    bool        exists(const std::string& name) const;
    void        mark_moved(const std::string& name);
    bool        is_moved(const std::string& name) const;

    std::optional<TypeNode> current_return_type() const;
    const AnnotationBlock*  current_annotations() const;
    uint32_t                current_budget_cap()  const;

    TrustLevel  trust_of(const TypeNode& t) const;
    bool        is_trusted(const TypeNode& t) const;
    bool        is_untrusted(const TypeNode& t) const;
    bool        types_equal(const TypeNode& a, const TypeNode& b) const;
    bool        is_assignable(const TypeNode& from, const TypeNode& to) const;
    const TypeNode& inner_type(const TypeNode& t) const;
    uint32_t    estimate_tokens(const TypeNode& t) const;
    void        charge_budget(uint32_t tokens);
    uint32_t    remaining_budget() const;

    // Global registries — filled in Pass 1
    std::unordered_map<std::string, const FnNode*>  fn_reg;
    std::unordered_map<std::string, const DatNode*> dat_reg;
    std::unordered_map<std::string, const EnmNode*> enm_reg;
    std::unordered_map<std::string, const AgtNode*> agt_reg;

    // Stdlib hardcoded return types
    std::unordered_map<std::string, TypeNode> stdlib_sigs;
    void init_stdlib_sigs();

private:
    std::vector<Scope> scopes_;
};

} // namespace agentc
