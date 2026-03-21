#pragma once
#include "typeenv.hpp"

namespace agentc {

EffectSet effects_from_annotations(const AnnotationBlock* ann);
CapSet caps_from_annotations(const AnnotationBlock* ann);
uint32_t budget_cap_from_annotations(const AnnotationBlock* ann);

std::vector<std::pair<std::string,std::string>>
check_annotation_conflicts(const AnnotationBlock* ann);

bool has_intent_annotation(const AnnotationBlock* ann);
EffectSet collect_stmt_effects(const StmtNode& stmt);

bool is_result_type(const TypeNode& t);
bool is_owned(const TypeNode& t);

TypeNode make_utr(TypeNode inner);
TypeNode make_tru(TypeNode inner);
TypeNode make_unit();
TypeNode make_named(std::string name);
TypeNode make_result(TypeNode ok, TypeNode err);
std::string type_name(const TypeNode& t);

TypeNode clone_type(const TypeNode& t);

} // namespace agentc
