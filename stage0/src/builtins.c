#include "builtins.h"
#include "codegen.h"
#include "type_env.h"
#include "diagnostics.h"
#include "sexpr.h"
#include "ir.h"
#include "stats.h"
#include "cgutils.h"

#include <string.h>

/* Helper functions for emitting values (similar to expr.c) */
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

static void emit_typed_value(StrBuf *out, TypeRef *t, Value v) {
    emit_llvm_type(out, t);
    sb_append(out, " ");
    emit_value(out, v);
}

/* Builtin codegen functions - Julia-style separate functions */

static Value cg_ptr_add_impl(IrCtx *ir, VarEnv *env, Node *expr) {
    TypeEnv *tenv = (TypeEnv *)ir->type_env;
    TypeRef *elem_ty = parse_type_node(tenv, list_nth(expr, 1));
    Value ptr = cg_expr(ir, env, list_nth(expr, 2));
    Value idx = ensure_type_ctx_at(ir, cg_expr(ir, env, list_nth(expr, 3)), type_i32(), "ptr-add index", expr);
    
    /* Type-driven codegen: check types first, fall back if needed */
    if (!is_pointer_type(ptr.type)) {
        /* Could emit runtime call here, for now just continue */
    }
    
    STAT_INC_PTR_ADD();
    STAT_INC_GEP();
    int t = ir_fresh_temp(ir);
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, t);
    sb_append(ir->out, " = getelementptr inbounds ");
    emit_llvm_type(ir->out, elem_ty);
    sb_append(ir->out, ", ");
    emit_typed_value(ir->out, ptr.type, ptr);
    sb_append(ir->out, ", i32 ");
    emit_value_i32(ir->out, idx);
    sb_append(ir->out, "\n");
    Value result = value_temp(type_ptr(elem_ty), t);
    result.is_pointer = 1;
    return result;
}

static Value cg_bitcast_impl(IrCtx *ir, VarEnv *env, Node *expr) {
    TypeEnv *tenv = (TypeEnv *)ir->type_env;
    TypeRef *to_ty = parse_type_node(tenv, list_nth(expr, 1));
    Value src = cg_expr(ir, env, list_nth(expr, 2));
    
    STAT_INC_BITCAST();
    STAT_INC_INTRINSIC();
    return maybe_bitcast(ir, src, to_ty);
}

/* Builtin function registry - maps names to their kinds and metadata */
static BuiltinDef builtins[] = {
    {
        .name = "ptr-add",
        .kind = BUILTIN_GEP,
        .ret_type = NULL,  /* Computed from first argument (type) */
        .param_count = 3,  /* type, ptr, idx */
        .param_types = NULL, /* Flexible - type is parsed from AST */
        .codegen = cg_ptr_add_impl
    },
    {
        .name = "get-field",
        .kind = BUILTIN_GEP,
        .ret_type = NULL,  /* Computed from struct field type */
        .param_count = 2,  /* base-ptr, field-name */
        .param_types = NULL,
        .codegen = NULL  /* Still handled in expr.c via cg_get_field */
    },
    {
        .name = "bitcast",
        .kind = BUILTIN_INTRINSIC,
        .ret_type = NULL,  /* Computed from first argument (to-type) */
        .param_count = 2,  /* to-type, src */
        .param_types = NULL,
        .codegen = cg_bitcast_impl
    },
    /* Add more builtins here as needed */
    { .name = NULL }  /* Sentinel */
};

void builtins_init(void) {
    /* Could do initialization here if needed */
}

BuiltinDef *find_builtin(const char *name) {
    int i;
    if (!name) return NULL;
    for (i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return &builtins[i];
        }
    }
    return NULL;
}

int is_builtin(const char *name) {
    return find_builtin(name) != NULL;
}

/* Get the kind of a builtin */
BuiltinKind builtin_kind(const char *name) {
    BuiltinDef *b = find_builtin(name);
    return b ? b->kind : BUILTIN_CALL;
}

/* Get builtin ID by name - Julia-style enum lookup */
BuiltinId builtin_id(const char *name) {
    if (!name) return BUILTIN_ID_NONE;
    if (strcmp(name, "ptr-add") == 0) return BUILTIN_ID_PTR_ADD;
    if (strcmp(name, "get-field") == 0) return BUILTIN_ID_GET_FIELD;
    if (strcmp(name, "bitcast") == 0) return BUILTIN_ID_BITCAST;
    return BUILTIN_ID_NONE;
}

/* Central dispatch function - Julia-style switch statement */
Value cg_builtin(IrCtx *ir, VarEnv *env, BuiltinId id, Node *expr) {
    /* Julia-style switch-based dispatch - one central point for all builtins */
    switch (id) {
    case BUILTIN_ID_PTR_ADD:
        return cg_ptr_add_impl(ir, env, expr);
    case BUILTIN_ID_BITCAST:
        return cg_bitcast_impl(ir, env, expr);
    case BUILTIN_ID_GET_FIELD:
        /* get-field still handled in expr.c via cg_get_field for now */
        return value_const_i32(0);
    case BUILTIN_ID_NONE:
    default:
        return value_const_i32(0);  /* Not a builtin or not implemented */
    }
}

