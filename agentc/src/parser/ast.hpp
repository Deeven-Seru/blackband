#pragma once
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <memory>
#include <any>
#include "../lexer/token.hpp"

namespace agentc {

struct SourceLoc { size_t line = 0; size_t col = 0; size_t offset = 0; };
struct NodeBase { SourceLoc loc; virtual ~NodeBase() = default; };

struct TypeNode;
struct ExprNode;
struct StmtNode;

struct LiteralNode {
    Token::Kind kind;
    std::string value;
};

struct PatternNode : NodeBase {
    enum class Kind { Wildcard, Literal, Binding, EnumVariant, Tuple };
    Kind kind;
    std::string name;
    std::vector<PatternNode> sub_patterns;
    std::optional<LiteralNode> literal;
};

struct ExprNode : NodeBase {
    enum class Kind {
        IntLit, UintLit, FloatLit, BoolLit, StrLit, UnitLit,
        Ident, Not, Neg,
        Add, Sub, Mul, Div, Eq, Neq, Lt, Gt, Leq, Geq, And, Or,
        Field, Call, MethodCall, Propagate,
        ReturnOk, ReturnErr, TruWrap, UtrWrap, OkWrap, ErrWrap,
        Budget, Trace, Correct, Par, Snapshot, Restore, Prune, Summarize,
        Spawn, Kill, Send, Recv, Sync, Race, Vouch, Pool, Chunk, Slide, Prof, Compress,
        BudgetCost, BudgetInspect, Walrus
    };
    Kind kind;
    std::string str_val;
    int64_t int_val = 0;
    uint64_t uint_val = 0;
    double float_val = 0.0;
    bool bool_val = false;

    std::vector<ExprNode> children; 
    std::vector<std::string> named_keys; 
    std::string field_name;
    std::unique_ptr<TypeNode> type_annotation;
    std::unique_ptr<ExprNode> repair_closure_expr; 
    std::vector<std::string> repair_closure_args;
};

struct TypeNode : NodeBase {
    enum class Kind {
        I8, I16, I32, I64, I128, U8, U16, U32, U64, U128, F32, F64,
        Bool, Str, Unit, Never,
        Tru, Utr, Aim, Cst, Cnf, Bnd, Own, Ref, Shr, Opt,
        Lst, Map, Tup, Dat, Enm, Ch, Agt, AgtResult, Result, Named, Pipe
    };
    Kind kind;
    std::vector<TypeNode> params;
    std::string name;
    std::unique_ptr<ExprNode> bound_lo;
    std::unique_ptr<ExprNode> bound_hi;
    std::optional<double> confidence;
    std::optional<size_t> str_bound;
    std::unique_ptr<TypeNode> ok_type;
    std::unique_ptr<TypeNode> err_type;
    std::vector<TypeNode> pipe_layers;
};

struct AnnotationNode : NodeBase {
    enum class Kind { Intent, Cost, Effect, Verify, Trust, Confidence, Retry, Goal, Capability, Trace, Protocol, FFI };
    Kind kind;
    std::any value; 
};

struct AnnotationBlock : NodeBase {
    std::vector<AnnotationNode> annotations;
};

struct StmtNode : NodeBase {
    enum class Kind {
        LetBind, MutBind, LocalInfer, Assign,
        If, Loop, ForIn, Match, Break, Continue,
        Expr, Block, MemScope
    };
    Kind kind;

    std::string bind_name;
    std::unique_ptr<TypeNode> bind_type;
    std::unique_ptr<ExprNode> bind_expr;
    bool is_mutable = false;
    std::string mem_scope_tier; 

    std::unique_ptr<ExprNode> condition;
    std::unique_ptr<StmtNode> then_branch;
    std::unique_ptr<StmtNode> else_branch;

    std::unique_ptr<ExprNode> match_expr;
    std::vector<std::pair<PatternNode, StmtNode>> match_arms;

    std::string iter_var;
    std::unique_ptr<ExprNode> iter_expr;

    std::vector<StmtNode> block_stmts;
    std::unique_ptr<ExprNode> block_tail;

    std::unique_ptr<ExprNode> expr;
};

struct ParamNode : NodeBase {
    bool is_mutable = false;
    std::string name;
    TypeNode type;
};

struct FnNode : NodeBase {
    AnnotationBlock annotations;
    std::string name;
    std::vector<ParamNode> params;
    TypeNode return_type;
    StmtNode body;
    bool is_external = false;
};

struct AgtNode : NodeBase {
    AnnotationBlock annotations;
    std::string name;
    FnNode run_fn;
};

struct DatFieldNode : NodeBase {
    std::string name;
    TypeNode type;
};

struct DatNode : NodeBase {
    std::string name;
    std::vector<DatFieldNode> fields;
};

struct EnmVariantNode : NodeBase {
    std::string name;
    std::unique_ptr<TypeNode> payload;
};

struct EnmNode : NodeBase {
    std::string name;
    std::vector<EnmVariantNode> variants;
};

struct ImportNode : NodeBase {
    std::string module_path;
    bool is_remote = false;
    std::string alias;
};

using TopLevelNode = std::variant<FnNode, AgtNode, DatNode, EnmNode>;

struct ProgramNode : NodeBase {
    std::vector<ImportNode> imports;
    std::vector<TopLevelNode> decls;
};

// Serializers
std::string ast_to_json(const ProgramNode& prog);
std::string node_to_json(const FnNode& fn);
std::string node_to_json(const StmtNode& stmt);
std::string node_to_json(const ExprNode& expr);
std::string node_to_json(const TypeNode& type);

} // namespace agentc
