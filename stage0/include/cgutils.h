#ifndef WEAVE_BOOTSTRAP_STAGE0_CGUTILS_H
#define WEAVE_BOOTSTRAP_STAGE0_CGUTILS_H

#include "codegen.h"
#include "types.h"
#include "ir.h"

/* Utility functions for code generation - Julia-style helper library
 * 
 * These provide reusable building blocks for common codegen operations,
 * reducing duplication and ensuring consistency.
 */

/* Check if a type is a pointer type */
int is_pointer_type(TypeRef *t);

/* Check if a value is a pointer */
int value_is_pointer(Value v);

/* Check if a value is constant */
int value_is_const(Value v);

/* Extract pointer from value (for pointer operations) */
Value value_to_pointer(IrCtx *ir, Value v);

/* Ensure value has pointer type, converting if needed */
Value ensure_pointer_type(IrCtx *ir, Value v, TypeRef *target);

/* Maybe bitcast value to target type if needed */
Value maybe_bitcast(IrCtx *ir, Value v, TypeRef *to_type);

/* Get pointer element type (for GEP operations) */
TypeRef *get_pointer_element_type(TypeRef *ptr_type);

/* Emit GEP instruction - centralized GEP generation */
Value emit_gep(IrCtx *ir, TypeRef *elem_type, Value ptr, Value idx);

/* Emit load instruction - centralized load generation */
Value emit_load(IrCtx *ir, TypeRef *load_type, Value ptr);

/* Emit store instruction - centralized store generation */
void emit_store(IrCtx *ir, Value val, Value ptr);

#endif

