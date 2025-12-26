#include "cgutils.h"
#include "codegen.h"
#include "diagnostics.h"
#include "stats.h"
#include "ir.h"
#include "types.h"
#include "common.h"
#include <string.h>

/* Helper to emit value (from expr.c pattern) */
static void emit_value(StrBuf *out, Value v) {
    if (v.kind == 0) {
        sb_printf_i32(out, v.const_i32);
    } else if (v.kind == 1) {
        ir_emit_temp(out, v.temp);
    } else {
        sb_append(out, "%");
        sb_append(out, v.ssa_name ? v.ssa_name : "");
    }
}

static void emit_value_i32(StrBuf *out, Value v) {
    if (v.kind == 0) {
        sb_printf_i32(out, v.const_i32);
    } else if (v.kind == 1) {
        ir_emit_temp(out, v.temp);
    } else {
        sb_append(out, "%");
        sb_append(out, v.ssa_name ? v.ssa_name : "");
    }
}

static void emit_typed_value(StrBuf *out, TypeRef *t, Value v) {
    emit_llvm_type(out, t);
    sb_append(out, " ");
    emit_value(out, v);
}

int is_pointer_type(TypeRef *t) {
    return t && (t->kind == TY_PTR || t->kind == TY_I8PTR);
}

int value_is_pointer(Value v) {
    return is_pointer_type(v.type) || v.is_pointer;
}

int value_is_const(Value v) {
    return v.is_const || (v.kind == 0);
}

Value value_to_pointer(IrCtx *ir, Value v) {
    if (is_pointer_type(v.type)) {
        return v;
    }
    /* Convert to pointer if needed - for now just return as-is */
    return v;
}

Value ensure_pointer_type(IrCtx *ir, Value v, TypeRef *target) {
    if (type_eq(v.type, target)) {
        return v;
    }
    /* Type conversion logic would go here */
    return ensure_type_ctx(ir, v, target, "pointer-type");
}

Value maybe_bitcast(IrCtx *ir, Value v, TypeRef *to_type) {
    if (type_eq(v.type, to_type)) {
        return v;
    }
    
    STAT_INC(emitted_type_conversions);
    int t = ir_fresh_temp(ir);
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, t);
    sb_append(ir->out, " = bitcast ");
    emit_llvm_type(ir->out, v.type);
    sb_append(ir->out, " ");
    emit_value(ir->out, v);
    sb_append(ir->out, " to ");
    emit_llvm_type(ir->out, to_type);
    sb_append(ir->out, "\n");
    Value result = value_temp(to_type, t);
    result.is_pointer = is_pointer_type(to_type);
    return result;
}

TypeRef *get_pointer_element_type(TypeRef *ptr_type) {
    if (!ptr_type) return NULL;
    if (ptr_type->kind == TY_PTR) {
        return ptr_type->pointee;
    }
    if (ptr_type->kind == TY_I8PTR) {
        return type_i32(); /* i8* element type is i8, but we use i32 for simplicity */
    }
    return NULL;
}

Value emit_gep(IrCtx *ir, TypeRef *elem_type, Value ptr, Value idx) {
    STAT_INC_GEP();
    int t = ir_fresh_temp(ir);
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, t);
    sb_append(ir->out, " = getelementptr inbounds ");
    emit_llvm_type(ir->out, elem_type);
    sb_append(ir->out, ", ");
    emit_typed_value(ir->out, ptr.type, ptr);
    sb_append(ir->out, ", i32 ");
    emit_value_i32(ir->out, idx);
    sb_append(ir->out, "\n");
    TypeRef *result_type = type_ptr(elem_type);
    Value result = value_temp(result_type, t);
    result.is_pointer = 1;
    return result;
}

Value emit_load(IrCtx *ir, TypeRef *load_type, Value ptr) {
    STAT_INC_LOAD();
    int t = ir_fresh_temp(ir);
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, t);
    sb_append(ir->out, " = load ");
    emit_llvm_type(ir->out, load_type);
    sb_append(ir->out, ", ");
    emit_llvm_type(ir->out, ptr.type);
    sb_append(ir->out, " ");
    emit_value(ir->out, ptr);
    sb_append(ir->out, "\n");
    Value result = value_temp(load_type, t);
    result.is_pointer = is_pointer_type(load_type);
    return result;
}

void emit_store(IrCtx *ir, Value val, Value ptr) {
    STAT_INC_STORE();
    sb_append(ir->out, "  store ");
    emit_llvm_type(ir->out, val.type);
    sb_append(ir->out, " ");
    emit_value(ir->out, val);
    sb_append(ir->out, ", ");
    emit_llvm_type(ir->out, ptr.type);
    sb_append(ir->out, " ");
    emit_value(ir->out, ptr);
    sb_append(ir->out, "\n");
}

