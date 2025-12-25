#include "codegen.h"
#include "diagnostics.h"

#include "fn_table.h"
#include "type_env.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int is_number_atom(Node *n) {
    const char *s;
    char *endp;
    if (!n || n->kind != N_ATOM) return 0;
    s = n->text;
    if (!s || !*s) return 0;
    (void)strtol(s, &endp, 10);
    return endp && *endp == '\0';
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

static const char *type_debug_name(TypeRef *t, char *buf, size_t n) {
    if (!t) return "<null>";
    switch (t->kind) {
    case TY_I32: return "i32";
    case TY_I8PTR: return "i8*";
    case TY_VOID: return "void";
    case TY_STRUCT:
        if (t->name) {
            snprintf(buf, n, "struct %s", t->name);
            return buf;
        }
        return "struct";
    case TY_PTR:
        if (t->pointee) {
            /* Use separate buffer for inner type to avoid buffer reuse bug */
            char inner_buf[64];
            const char *inner = type_debug_name(t->pointee, inner_buf, sizeof(inner_buf));
            snprintf(buf, n, "ptr(%s)", inner);
            return buf;
        }
        return "ptr";
    default:
        return "<unknown>";
    }
}

Value ensure_type_ctx_at(IrCtx *ir, Value v, TypeRef *target, const char *ctx, Node *location) {
    char expected_buf[64];
    char got_buf[64];
    /* Defensive: ensure v always has a valid type with valid kind */
    if (!v.type || v.type->kind < TY_I32 || v.type->kind > TY_PTR) {
        v.type = type_i32();
    }
    if (type_eq(v.type, target)) return v;
    /* Minimal conversions:
     * - allow i32 const 0 as null i8*
     * - allow ptr -> i8* via bitcast
     * - allow ptr/i8* -> i32 via ptrtoint (for bootstrap flexibility)
     */
    if (target && target->kind == TY_I8PTR && v.type && v.type->kind == TY_I32 && v.kind == 0 && v.const_i32 == 0) {
        int t = ir_fresh_temp(ir);
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, t);
        sb_append(ir->out, " = inttoptr i32 0 to i8*\n");
        return value_temp(type_i8ptr(), t);
    }
    if (target && target->kind == TY_I8PTR && v.type && v.type->kind == TY_PTR) {
        int t = ir_fresh_temp(ir);
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, t);
        sb_append(ir->out, " = bitcast ");
        emit_llvm_type(ir->out, v.type);
        sb_append(ir->out, " ");
        emit_value(ir->out, v);
        sb_append(ir->out, " to i8*\n");
        return value_temp(type_i8ptr(), t);
    }
    if (target && target->kind == TY_I32 && v.type && (v.type->kind == TY_PTR || v.type->kind == TY_I8PTR)) {
        int t = ir_fresh_temp(ir);
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, t);
        sb_append(ir->out, " = ptrtoint ");
        emit_llvm_type(ir->out, v.type);
        sb_append(ir->out, " ");
        emit_value(ir->out, v);
        sb_append(ir->out, " to i32\n");
        return value_temp(type_i32(), t);
    }
    if (location && location->filename) {
        char details[256];
        const char *expected_str = type_debug_name(target, expected_buf, sizeof(expected_buf));
        const char *got_str = type_debug_name(v.type, got_buf, sizeof(got_buf));
        /* Debug: verify types when error occurs */
        if (getenv("WEAVEC0_DEBUG_SIGS") && target && target->kind == TY_PTR) {
            fprintf(stderr, "[dbg] ERROR: target kind=%d, pointee_kind=%d, expected_str='%s'\n",
                    target->kind,
                    target->pointee ? target->pointee->kind : -1,
                    expected_str);
        }
        snprintf(details, sizeof(details), "in function '%s', context '%s': wanted %s, got %s",
                 ir->current_fn ? ir->current_fn : "<none>",
                 ctx ? ctx : "-",
                 expected_str,
                 got_str);
        diag_fatal(location->filename, location->line, location->col,
                   "type-mismatch", "type mismatch in expression", details);
    } else {
        char details[256];
        snprintf(details, sizeof(details), "in function '%s', context '%s': wanted %s, got %s",
                 ir->current_fn ? ir->current_fn : "<none>",
                 ctx ? ctx : "-",
                 type_debug_name(target, expected_buf, sizeof(expected_buf)),
                 type_debug_name(v.type, got_buf, sizeof(got_buf)));
        diag_fatal(NULL, 0, 0, "type-mismatch", "type mismatch in expression", details);
    }
    return value_const_i32(0);
}

Value ensure_type_ctx(IrCtx *ir, Value v, TypeRef *target, const char *ctx) {
    return ensure_type_ctx_at(ir, v, target, ctx, NULL);
}

static Value cg_addr(IrCtx *ir, VarEnv *env, Node *n) {
    (void)ir;
    if (!n || n->kind != N_ATOM) return value_const_i32(0);
    if (!env_has(env, n->text)) return value_const_i32(0);
    return value_ssa(type_ptr(env_type(env, n->text)), env_ssa_name(env, n->text));
}

static Value cg_load(IrCtx *ir, VarEnv *env, Node *list) {
    TypeEnv *tenv = (TypeEnv *)ir->type_env;
    TypeRef *ty = parse_type_node(tenv, list_nth(list, 1));
    Value ptrv = cg_expr(ir, env, list_nth(list, 2));
    int t;
    if (!ptrv.type || ptrv.type->kind != TY_PTR) die("load expects ptr");
    t = ir_fresh_temp(ir);
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, t);
    sb_append(ir->out, " = load ");
    emit_llvm_type(ir->out, ty);
    sb_append(ir->out, ", ");
    emit_llvm_type(ir->out, ptrv.type);
    sb_append(ir->out, " ");
    emit_value(ir->out, ptrv);
    sb_append(ir->out, "\n");
    return value_temp(ty, t);
}

static Value cg_make_struct(IrCtx *ir, VarEnv *env, Node *list) {
    TypeEnv *tenv = (TypeEnv *)ir->type_env;
    TypeRef *ty = parse_type_node(tenv, list_nth(list, 1));
    StructDef *sd = NULL;
    int i;
    int size_ptr = ir_fresh_temp(ir);
    int size_i64 = ir_fresh_temp(ir);
    int malloc_result = ir_fresh_temp(ir);
    int ptr = ir_fresh_temp(ir);
    if (!ty || ty->kind != TY_STRUCT) die("make expects struct type");
    sd = type_env_find_struct(tenv, ty->name);
    
    /* Calculate struct size using GEP null trick */
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, size_ptr);
    sb_append(ir->out, " = getelementptr ");
    emit_llvm_type(ir->out, ty);
    sb_append(ir->out, ", ");
    emit_llvm_type(ir->out, ty);
    sb_append(ir->out, "* null, i32 1\n");
    
    /* Convert pointer to integer (gives us the size) */
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, size_i64);
    sb_append(ir->out, " = ptrtoint ");
    emit_llvm_type(ir->out, ty);
    sb_append(ir->out, "* ");
    ir_emit_temp(ir->out, size_ptr);
    sb_append(ir->out, " to i32\n");
    
    /* Call malloc */
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, malloc_result);
    sb_append(ir->out, " = call i8* @malloc(i32 ");
    ir_emit_temp(ir->out, size_i64);
    sb_append(ir->out, ")\n");
    
    /* Bitcast to struct pointer */
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, ptr);
    sb_append(ir->out, " = bitcast i8* ");
    ir_emit_temp(ir->out, malloc_result);
    sb_append(ir->out, " to ");
    emit_llvm_type(ir->out, ty);
    sb_append(ir->out, "*\n");
    for (i = 2; i < list->count; i++) {
        Node *field = list_nth(list, i);
        const char *fname = atom_text(list_nth(field, 0));
        int fi = struct_field_index(sd, fname);
        TypeRef *fty = (fi >= 0 && sd) ? sd->field_types[fi] : type_i32();
        Value fv = cg_expr(ir, env, list_nth(field, 1));
        int pfi = ir_fresh_temp(ir);
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, pfi);
        sb_append(ir->out, " = getelementptr inbounds ");
        emit_llvm_type(ir->out, ty);
        sb_append(ir->out, ", ");
        emit_llvm_type(ir->out, ty);
        sb_append(ir->out, "* ");
        ir_emit_temp(ir->out, ptr);
        sb_append(ir->out, ", i32 0, i32 ");
        sb_printf_i32(ir->out, fi >= 0 ? fi : 0);
        sb_append(ir->out, "\n");
        sb_append(ir->out, "  store ");
        emit_llvm_type(ir->out, fty);
        sb_append(ir->out, " ");
        emit_value(ir->out, fv);
        sb_append(ir->out, ", ");
        emit_llvm_type(ir->out, fty);
        sb_append(ir->out, "* ");
        ir_emit_temp(ir->out, pfi);
        sb_append(ir->out, "\n");
    }
    /* Return pointer to the allocated struct instead of loading the value */
    return value_temp(type_ptr(ty), ptr);
}

static Value cg_get_field(IrCtx *ir, VarEnv *env, Node *list) {
    Value base = cg_expr(ir, env, list_nth(list, 1));
    const char *fname = atom_text(list_nth(list, 2));
    TypeEnv *tenv = (TypeEnv *)ir->type_env;
    StructDef *sd = NULL;
    int fi;
    TypeRef *sty;
    int pfield, loadt;
    if (!base.type) return value_const_i32(0);
    if (base.type->kind == TY_STRUCT) sty = base.type;
    else if (base.type->kind == TY_PTR) sty = base.type->pointee;
    else return value_const_i32(0);
    sd = type_env_find_struct(tenv, sty->name);
    fi = struct_field_index(sd, fname);
    if (fi < 0) return value_const_i32(0);
    pfield = ir_fresh_temp(ir);
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, pfield);
    sb_append(ir->out, " = getelementptr inbounds ");
    emit_llvm_type(ir->out, sty);
    sb_append(ir->out, ", ");
    emit_llvm_type(ir->out, sty);
    sb_append(ir->out, "* ");
    emit_value(ir->out, base);
    sb_append(ir->out, ", i32 0, i32 ");
    sb_printf_i32(ir->out, fi);
    sb_append(ir->out, "\n");
    loadt = ir_fresh_temp(ir);
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, loadt);
    sb_append(ir->out, " = load ");
    emit_llvm_type(ir->out, sd->field_types[fi]);
    sb_append(ir->out, ", ");
    emit_llvm_type(ir->out, sd->field_types[fi]);
    sb_append(ir->out, "* ");
    ir_emit_temp(ir->out, pfield);
    sb_append(ir->out, "\n");
    return value_temp(sd->field_types[fi], loadt);
}


static void emit_escaped_c_string(StrBuf *out, const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char ch = *p++;
        if (ch == '\\') sb_append(out, "\\5C");
        else if (ch == '"') sb_append(out, "\\22");
        else if (ch == '\n') sb_append(out, "\\0A");
        else if (ch == '\r') sb_append(out, "\\0D");
        else if (ch == '\t') sb_append(out, "\\09");
        else if (ch < 32 || ch >= 127) {
            const char hex[] = "0123456789ABCDEF";
            sb_append_ch(out, '\\');
            sb_append_ch(out, hex[(ch >> 4) & 0xF]);
            sb_append_ch(out, hex[ch & 0xF]);
        } else {
            sb_append_ch(out, (char)ch);
        }
    }
}

static Value cg_string_lit(IrCtx *ir, Node *str_node) {
    static int str_id = 0;
    int id = str_id++;
    const char *s = atom_text(str_node);
    int n = (int)strlen(s) + 1;
    int t = ir_fresh_temp(ir);

    sb_append(&ir->globals, "@.str");
    sb_printf_i32(&ir->globals, id);
    sb_append(&ir->globals, " = private constant [");
    sb_printf_i32(&ir->globals, n);
    sb_append(&ir->globals, " x i8] c\"");
    emit_escaped_c_string(&ir->globals, s);
    sb_append(&ir->globals, "\\00\"\n");

    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, t);
    sb_append(ir->out, " = getelementptr inbounds [");
    sb_printf_i32(ir->out, n);
    sb_append(ir->out, " x i8], [");
    sb_printf_i32(ir->out, n);
    sb_append(ir->out, " x i8]* @.str");
    sb_printf_i32(ir->out, id);
    sb_append(ir->out, ", i32 0, i32 0\n");

    return value_temp(type_i8ptr(), t);
}

static Value cg_call(IrCtx *ir, VarEnv *env, Node *list) {
    Node *head = list_nth(list, 0);
    int argc = list->count - 1;
    int i;
    int t;

    /* llvm-jit special form - JIT compile and execute LLVM IR */
    if (is_atom(head, "llvm-jit")) {
        Node *ir_node = list_nth(list, 1);
        Node *func_name_node = list_nth(list, 2);
        Node *args_list = list_nth(list, 3);
        
        if (!ir_node || !func_name_node) {
            diag_fatal(list && list->filename ? list->filename : NULL,
                       list ? list->line : 0,
                       list ? list->col : 0,
                       "syntax-error",
                       "llvm-jit requires IR string and function name",
                       "Usage: (llvm-jit \"IR code\" \"function_name\" (args ...))");
        }
        
        const char *ir_str = atom_text(ir_node);
        const char *func_name = atom_text(func_name_node);
        
        if (!ir_str || !func_name) {
            diag_fatal(list && list->filename ? list->filename : NULL,
                       list ? list->line : 0,
                       list ? list->col : 0,
                       "syntax-error",
                       "llvm-jit IR and function name must be string literals",
                       "Usage: (llvm-jit \"IR code\" \"function_name\" (args ...))");
        }
        
        /* For now, support simple case: function that takes 2 Int32s and returns Int32 */
        Value arg1_val = {0}, arg2_val = {0};
        if (args_list && args_list->kind == N_LIST) {
            /* Check if it's (args ...) form or just a list of arguments */
            Node *first = list_nth(args_list, 0);
            int start_idx = 0;
            if (first && first->kind == N_ATOM && is_atom(first, "args")) {
                start_idx = 1;  /* Skip "args" atom */
            }
            /* Need at least 2 arguments after start_idx */
            if (args_list->count >= start_idx + 2) {
                Node *arg1 = list_nth(args_list, start_idx);
                Node *arg2 = list_nth(args_list, start_idx + 1);
                if (arg1 && arg2) {
                    arg1_val = ensure_type_ctx(ir, cg_expr(ir, env, arg1), type_i32(), "llvm-jit-arg1");
                    arg2_val = ensure_type_ctx(ir, cg_expr(ir, env, arg2), type_i32(), "llvm-jit-arg2");
                }
            }
        }
        
        /* Declare JIT helper if not already declared */
        if (!sl_contains(&ir->declared_ccalls, "llvm_jit_call_i32_i32_i32")) {
            sl_push(&ir->declared_ccalls, "llvm_jit_call_i32_i32_i32");
            sb_append(&ir->decls, "declare i32 @llvm_jit_call_i32_i32_i32(i8*, i8*, i32, i32)\n");
        }
        
        /* Emit call to JIT helper */
        int t = ir_fresh_temp(ir);
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, t);
        sb_append(ir->out, " = call i32 @llvm_jit_call_i32_i32_i32(i8* getelementptr inbounds ([");
        /* Calculate IR string length - approximate for now */
        int ir_len = (int)strlen(ir_str);
        sb_printf_i32(ir->out, ir_len + 1);
        sb_append(ir->out, " x i8], [");
        sb_printf_i32(ir->out, ir_len + 1);
        sb_append(ir->out, " x i8]* @");
        
        /* Create a global string constant for IR */
        static int ir_counter = 0;
        ir_counter++;
        char ir_global[64];
        snprintf(ir_global, sizeof(ir_global), "llvm_jit_ir_%d", ir_counter);
        
        /* Emit global string constant */
        if (!sl_contains(&ir->declared_ccalls, ir_global)) {
            sl_push(&ir->declared_ccalls, ir_global);
            sb_append(&ir->globals, "@");
            sb_append(&ir->globals, ir_global);
            sb_append(&ir->globals, " = private unnamed_addr constant [");
            sb_printf_i32(&ir->globals, ir_len + 1);
            sb_append(&ir->globals, " x i8] c\"");
            /* Escape the string for LLVM IR */
            int i;
            for (i = 0; ir_str[i]; i++) {
                if (ir_str[i] == '\n') {
                    sb_append(&ir->globals, "\\0A");
                } else if (ir_str[i] == '"') {
                    sb_append(&ir->globals, "\\22");
                } else if (ir_str[i] == '\\') {
                    sb_append(&ir->globals, "\\5C");
                } else {
                    sb_append_ch(&ir->globals, ir_str[i]);
                }
            }
            sb_append(&ir->globals, "\\00\"\n");
        }
        
        sb_append(ir->out, ir_global);
        sb_append(ir->out, ", i32 0, i32 0), i8* getelementptr inbounds ([");
        
        /* Function name string */
        int func_name_len = (int)strlen(func_name);
        sb_printf_i32(ir->out, func_name_len + 1);
        sb_append(ir->out, " x i8], [");
        sb_printf_i32(ir->out, func_name_len + 1);
        sb_append(ir->out, " x i8]* @");
        
        char func_global[64];
        snprintf(func_global, sizeof(func_global), "llvm_jit_func_%d", ir_counter);
        
        if (!sl_contains(&ir->declared_ccalls, func_global)) {
            sl_push(&ir->declared_ccalls, func_global);
            sb_append(&ir->globals, "@");
            sb_append(&ir->globals, func_global);
            sb_append(&ir->globals, " = private unnamed_addr constant [");
            sb_printf_i32(&ir->globals, func_name_len + 1);
            sb_append(&ir->globals, " x i8] c\"");
            sb_append(&ir->globals, func_name);
            sb_append(&ir->globals, "\\00\"\n");
        }
        
        sb_append(ir->out, func_global);
        sb_append(ir->out, ", i32 0, i32 0), i32 ");
        emit_value_i32(ir->out, arg1_val);
        sb_append(ir->out, ", i32 ");
        emit_value_i32(ir->out, arg2_val);
        sb_append(ir->out, ")\n");
        
        Value v;
        v.temp = t;
        v.type = type_i32();
        return v;
    }
    
    /* ccall special form */
    if (is_atom(head, "ccall")) {
        Node *sym_node = list_nth(list, 1);
        Node *returns_form = list_nth(list, 2);
        Node *args_form = list_nth(list, 3);
        const char *sym = atom_text(sym_node);
        int is_printf = (sym && strcmp(sym, "printf") == 0);
        TypeRef *ret_ty;
        TypeEnv *tenv = (TypeEnv *)ir->type_env;
        int nargs = 0;
        TypeRef **arg_types = NULL;
        Value *arg_vals = NULL;

        if (!returns_form || returns_form->kind != N_LIST || !is_atom(list_nth(returns_form, 0), "returns")) {
            diag_fatal(list && list->filename ? list->filename : NULL,
                       list ? list->line : 0,
                       list ? list->col : 0,
                       "syntax-error",
                       "ccall missing (returns ...) form",
                       "ccall requires a (returns Type) specification");
        }
        ret_ty = parse_type_node(tenv, list_nth(returns_form, 1));

        /* Evaluate args first so we don't interleave instructions into the call line. */
        if (args_form && args_form->kind == N_LIST && is_atom(list_nth(args_form, 0), "args")) {
            nargs = args_form->count - 1;
            if (nargs < 0) nargs = 0;
            arg_types = (TypeRef **)xmalloc((size_t)nargs * sizeof(TypeRef *));
            arg_vals = (Value *)xmalloc((size_t)nargs * sizeof(Value));
            for (i = 0; i < nargs; i++) {
                Node *arg = list_nth(args_form, i + 1);
                TypeRef *aty = type_i32();
                Node *expr = NULL;
                if (arg && arg->kind == N_LIST) {
                    aty = parse_type_node(tenv, list_nth(arg, 0));
                    expr = list_nth(arg, 1);
                }
                arg_types[i] = aty;
                arg_vals[i] = ensure_type_ctx_at(ir, cg_expr(ir, env, expr), aty, "ccall-arg", arg);
            }
        }

        /* Emit declare only if not already declared */
        if (!sl_contains(&ir->declared_ccalls, sym)) {
            sl_push(&ir->declared_ccalls, sym);
            if (is_printf) {
                sb_append(&ir->decls, "declare i32 @printf(i8*, ...)\n");
            } else {
                sb_append(&ir->decls, "declare ");
                emit_llvm_type(&ir->decls, ret_ty);
                sb_append(&ir->decls, " @");
                sb_append(&ir->decls, sym);
                sb_append(&ir->decls, "(");
                for (i = 0; i < nargs; i++) {
                    if (i != 0) sb_append(&ir->decls, ", ");
                    emit_llvm_type(&ir->decls, arg_types[i]);
                }
                sb_append(&ir->decls, ")\n");
            }
        }

        if (is_printf) {
            t = ir_fresh_temp(ir);
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, t);
            sb_append(ir->out, " = call i32 (i8*, ...) @printf(");
        } else if (ret_ty->kind == TY_VOID) {
            sb_append(ir->out, "  call void @");
            sb_append(ir->out, sym);
            sb_append(ir->out, "(");
        } else {
            t = ir_fresh_temp(ir);
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, t);
            sb_append(ir->out, " = call ");
            emit_llvm_type(ir->out, ret_ty);
            sb_append(ir->out, " @");
            sb_append(ir->out, sym);
            sb_append(ir->out, "(");
        }

        for (i = 0; i < nargs; i++) {
            if (i != 0) sb_append(ir->out, ", ");
            emit_typed_value(ir->out, arg_types[i], arg_vals[i]);
        }
        sb_append(ir->out, ")\n");

        if (arg_types) free(arg_types);
        if (arg_vals) free(arg_vals);

        if (ret_ty->kind == TY_VOID) return value_const_i32(0);
        return value_temp(ret_ty, t);
    }

    {
        FnTable *fns = (FnTable *)ir->fn_table;
        TypeEnv *tenv = (TypeEnv *)ir->type_env;
        const char *fn_name = atom_text(head);
        int fn_idx = fns ? fn_table_find(fns, fn_name) : -1;
        const char *dbg_calls = getenv("WEAVEC0_DEBUG_CALLS");
        TypeRef *ret_ty = fns ? fn_table_ret_type(fns, fn_name, type_i32()) : type_i32();
        Value *arg_vals = NULL;
        TypeRef **arg_types = NULL;

        if (dbg_calls && fn_idx < 0) {
            fprintf(stderr, "[dbg] unknown fn: %s\n", fn_name);
        }

        if (argc > 0) {
            arg_vals = (Value *)xmalloc((size_t)argc * sizeof(Value));
            arg_types = (TypeRef **)xmalloc((size_t)argc * sizeof(TypeRef *));
            for (i = 0; i < argc; i++) {
                Node *arg_expr = list_nth(list, i + 1);
                TypeRef *expected = type_i32();
                if (fns) expected = fn_table_param_type(fns, fn_name, i, type_i32());
                arg_types[i] = expected;
                Value arg_val = cg_expr(ir, env, arg_expr);
                /* Debug: verify argument value */
                if (getenv("WEAVEC0_DEBUG_SIGS") && strcmp(fn_name, "arena-kind") == 0 && i == 0) {
                    fprintf(stderr, "[dbg] arena-kind arg[0]: arg_val.type=%p, arg_val.type_kind=%d, expected_kind=%d\n",
                            (void *)arg_val.type, arg_val.type ? arg_val.type->kind : -1, expected ? expected->kind : -1);
                }
                arg_vals[i] = ensure_type_ctx_at(ir, arg_val, expected, "fn-arg", arg_expr);
            }
        }

        if (ret_ty->kind == TY_VOID) {
            sb_append(ir->out, "  call void @");
            sb_append(ir->out, fn_name);
            sb_append(ir->out, "(");
        } else {
            t = ir_fresh_temp(ir);
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, t);
            sb_append(ir->out, " = call ");
            emit_llvm_type(ir->out, ret_ty);
            sb_append(ir->out, " @");
            sb_append(ir->out, fn_name);
            sb_append(ir->out, "(");
        }
        for (i = 0; i < argc; i++) {
            if (i != 0) sb_append(ir->out, ", ");
            emit_typed_value(ir->out, arg_types ? arg_types[i] : type_i32(), arg_vals[i]);
        }
        sb_append(ir->out, ")\n");

        if (arg_vals) free(arg_vals);
        if (arg_types) free(arg_types);
        if (ret_ty->kind == TY_VOID) return value_const_i32(0);
        return value_temp(ret_ty, t);
    }
}

Value cg_expr(IrCtx *ir, VarEnv *env, Node *expr) {
    if (!expr) return value_const_i32(0);

    if (expr->kind == N_ATOM) {
        if (is_number_atom(expr)) return value_const_i32(atoi(expr->text));

        /* variable reference */
        if (env_has(env, expr->text)) {
            int kind = env_kind(env, expr->text);
            TypeRef *ty = env_type(env, expr->text);
            const char *ssa = env_ssa_name(env, expr->text);
            /* Debug: verify variable lookup */
            if (getenv("WEAVEC0_DEBUG_SIGS") && strcmp(expr->text, "a") == 0) {
                fprintf(stderr, "[dbg] Variable 'a' lookup: kind=%d, ty=%p, ty_kind=%d, ssa='%s'\n",
                        kind, (void *)ty, ty ? ty->kind : -1, ssa ? ssa : "<null>");
            }
            /* Both locals (kind==0) and parameters (kind==1) are in allocas now */
            /* Defensive: handle kind==-1 case (shouldn't happen but handle gracefully) */
            if (kind == 0 || kind == 1) {
                /* Defensive: if type is NULL (shouldn't happen but handle gracefully) */
                if (!ty) {
                    /* Debug: verify why type is NULL */
                    if (getenv("WEAVEC0_DEBUG_SIGS")) {
                        fprintf(stderr, "[dbg] Variable '%s' has NULL type, defaulting to i32\n", expr->text);
                    }
                    /* This indicates a bug in environment setup, but default to i32 to avoid crash */
                    ty = type_i32();
                }
                int t = ir_fresh_temp(ir);
                sb_append(ir->out, "  ");
                ir_emit_temp(ir->out, t);
                sb_append(ir->out, " = load ");
                emit_llvm_type(ir->out, ty);
                sb_append(ir->out, ", ");
                emit_llvm_type(ir->out, ty);
                sb_append(ir->out, "* %");
                sb_append(ir->out, ssa);
                sb_append(ir->out, "\n");
                /* Debug: verify Value creation */
                if (getenv("WEAVEC0_DEBUG_SIGS") && strcmp(expr->text, "a") == 0) {
                    fprintf(stderr, "[dbg] Creating Value for 'a': ty=%p, ty_kind=%d\n",
                            (void *)ty, ty ? ty->kind : -1);
                }
                return value_temp(ty, t);
            }
            /* Variable exists but kind is invalid - defensive fallback */
            if (!ty) ty = type_i32();
            int t = ir_fresh_temp(ir);
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, t);
            sb_append(ir->out, " = load ");
            emit_llvm_type(ir->out, ty);
            sb_append(ir->out, ", ");
            emit_llvm_type(ir->out, ty);
            sb_append(ir->out, "* %");
            sb_append(ir->out, ssa ? ssa : expr->text);
            sb_append(ir->out, "\n");
            return value_temp(ty, t);
        }

        /* Unknown atom: treat as 0 */
        return value_const_i32(0);
    }

    if (expr->kind == N_STRING) {
        return cg_string_lit(ir, expr);
    }

    if (expr->kind == N_LIST) {
        Node *head = list_nth(expr, 0);
        Value lhs;
        Value rhs;
        int t;

        if (!head || head->kind != N_ATOM) return value_const_i32(0);

        if (is_atom(head, "doc")) {
            return value_const_i32(0);
        }

        if (is_atom(head, "block")) {
            Value last = {0};
            int i;
            int has_last = 0;
            for (i = 1; i < expr->count; i++) {
                Node *item = list_nth(expr, i);
                Node *ih = list_nth(item, 0);
                if (item && item->kind == N_LIST && ih && ih->kind == N_ATOM &&
                    (is_atom(ih, "let") || is_atom(ih, "set") || is_atom(ih, "store") ||
                     is_atom(ih, "set-field") || is_atom(ih, "do") || is_atom(ih, "if-stmt") ||
                     is_atom(ih, "while"))) {
                    Value tmp = {0};
                    if (cg_stmt(ir, env, item, type_i32(), &tmp)) return value_const_i32(0);
                    if (tmp.type) {
                        last = tmp;
                        has_last = 1;
                    }
                    continue;
                }
                last = cg_expr(ir, env, item);
                has_last = 1;
            }
            if (has_last) return last;
            return value_const_i32(0);
        }

        if (is_atom(head, "addr")) {
            return cg_addr(ir, env, list_nth(expr, 1));
        }

        if (is_atom(head, "addr-of")) {
            TypeEnv *tenv = (TypeEnv *)ir->type_env;
            Node *type_node = list_nth(expr, 1);
            Node *name_node = list_nth(expr, 2);
            const char *name = atom_text(name_node);
            TypeRef *decl_ty = env_type(env, name);
            TypeRef *arg_ty = parse_type_node(tenv, type_node);
            TypeRef *ptr_ty = NULL;
            if (decl_ty && arg_ty && !type_eq(decl_ty, arg_ty)) {
                /* Prefer declared type, but warn via diagnostics if mismatch */
                diag_warn(name_node && name_node->filename ? name_node->filename : NULL,
                          name_node ? name_node->line : 0,
                          name_node ? name_node->col : 0,
                          "addr-of-type-mismatch",
                          "addr-of type does not match variable declared type",
                          NULL);
            }
            ptr_ty = type_ptr(decl_ty ? decl_ty : arg_ty);
            return value_ssa(ptr_ty, env_ssa_name(env, name));
        }

        if (is_atom(head, "load")) {
            return cg_load(ir, env, expr);
        }

        if (is_atom(head, "make")) {
            return cg_make_struct(ir, env, expr);
        }

        if (is_atom(head, "get-field")) {
            return cg_get_field(ir, env, expr);
        }

        if (is_atom(head, "bitcast")) {
            TypeEnv *tenv = (TypeEnv *)ir->type_env;
            TypeRef *to_ty = parse_type_node(tenv, list_nth(expr, 1));
            Value src = cg_expr(ir, env, list_nth(expr, 2));
            int t = ir_fresh_temp(ir);
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, t);
            sb_append(ir->out, " = bitcast ");
            emit_llvm_type(ir->out, src.type);
            sb_append(ir->out, " ");
            emit_value(ir->out, src);
            sb_append(ir->out, " to ");
            emit_llvm_type(ir->out, to_ty);
            sb_append(ir->out, "\n");
            return value_temp(to_ty, t);
        }

        if (is_atom(head, "+") || is_atom(head, "-") || is_atom(head, "*") || is_atom(head, "/")) {
            lhs = ensure_type_ctx_at(ir, cg_expr(ir, env, list_nth(expr, 1)), type_i32(), "arith", expr);
            rhs = ensure_type_ctx_at(ir, cg_expr(ir, env, list_nth(expr, 2)), type_i32(), "arith", expr);
            t = ir_fresh_temp(ir);
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, t);
            sb_append(ir->out, " = ");
            if (is_atom(head, "+")) sb_append(ir->out, "add");
            else if (is_atom(head, "-")) sb_append(ir->out, "sub");
            else if (is_atom(head, "*")) sb_append(ir->out, "mul");
            else sb_append(ir->out, "sdiv");
            sb_append(ir->out, " i32 ");
            emit_value_i32(ir->out, lhs);
            sb_append(ir->out, ", ");
            emit_value_i32(ir->out, rhs);
            sb_append(ir->out, "\n");
            return value_temp(type_i32(), t);
        }

        /* Comparisons return i32 0/1 */
        if (is_atom(head, "==") || is_atom(head, "!=") || is_atom(head, "<") ||
            is_atom(head, "<=") || is_atom(head, ">") || is_atom(head, ">=")) {
            const char *pred = "eq";
            int tcmp, tout;
            Value raw_lhs = cg_expr(ir, env, list_nth(expr, 1));
            Value raw_rhs = cg_expr(ir, env, list_nth(expr, 2));

            /* Determine comparison mode: integer vs pointer */
            int lhs_is_i32 = (raw_lhs.type && raw_lhs.type->kind == TY_I32);
            int rhs_is_i32 = (raw_rhs.type && raw_rhs.type->kind == TY_I32);
            int lhs_is_ptr = (raw_lhs.type && (raw_lhs.type->kind == TY_PTR || raw_lhs.type->kind == TY_I8PTR));
            int rhs_is_ptr = (raw_rhs.type && (raw_rhs.type->kind == TY_PTR || raw_rhs.type->kind == TY_I8PTR));

            if (is_atom(head, "==")) pred = "eq";
            else if (is_atom(head, "!=")) pred = "ne";
            else if (is_atom(head, "<")) pred = "slt";
            else if (is_atom(head, "<=")) pred = "sle";
            else if (is_atom(head, ">")) pred = "sgt";
            else pred = "sge";

            tcmp = ir_fresh_temp(ir);
            tout = ir_fresh_temp(ir);

            /* Pointer-aware equality: bitcast both to i8* when needed */
            if ((is_atom(head, "==") || is_atom(head, "!=")) && (lhs_is_ptr || rhs_is_ptr)) {
                lhs = ensure_type_ctx_at(ir, raw_lhs, type_i8ptr(), "cmp", expr);
                rhs = ensure_type_ctx_at(ir, raw_rhs, type_i8ptr(), "cmp", expr);
                sb_append(ir->out, "  ");
                ir_emit_temp(ir->out, tcmp);
                sb_append(ir->out, " = icmp ");
                sb_append(ir->out, pred);
                sb_append(ir->out, " i8* ");
                emit_value(ir->out, lhs);
                sb_append(ir->out, ", ");
                emit_value(ir->out, rhs);
                sb_append(ir->out, "\n");
            } else {
                /* Fallback to integer comparison; ptrs become i32 via ptrtoint */
                lhs = ensure_type_ctx_at(ir, raw_lhs, type_i32(), "cmp", expr);
                rhs = ensure_type_ctx_at(ir, raw_rhs, type_i32(), "cmp", expr);
                sb_append(ir->out, "  ");
                ir_emit_temp(ir->out, tcmp);
                sb_append(ir->out, " = icmp ");
                sb_append(ir->out, pred);
                sb_append(ir->out, " i32 ");
                emit_value_i32(ir->out, lhs);
                sb_append(ir->out, ", ");
                emit_value_i32(ir->out, rhs);
                sb_append(ir->out, "\n");
            }

            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tout);
            sb_append(ir->out, " = zext i1 ");
            ir_emit_temp(ir->out, tcmp);
            sb_append(ir->out, " to i32\n");
            return value_temp(type_i32(), tout);
        }

        if (is_atom(head, "&&") || is_atom(head, "||")) {
            int tb1, tb2, tb3, tout;
            lhs = ensure_type_ctx_at(ir, cg_expr(ir, env, list_nth(expr, 1)), type_i32(), "logic", expr);
            rhs = ensure_type_ctx_at(ir, cg_expr(ir, env, list_nth(expr, 2)), type_i32(), "logic", expr);
            tb1 = ir_fresh_temp(ir);
            tb2 = ir_fresh_temp(ir);
            tb3 = ir_fresh_temp(ir);
            tout = ir_fresh_temp(ir);
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tb1);
            sb_append(ir->out, " = icmp ne i32 ");
            emit_value_i32(ir->out, lhs);
            sb_append(ir->out, ", 0\n");
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tb2);
            sb_append(ir->out, " = icmp ne i32 ");
            emit_value_i32(ir->out, rhs);
            sb_append(ir->out, ", 0\n");
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tb3);
            sb_append(ir->out, " = ");
            if (is_atom(head, "&&")) sb_append(ir->out, "and");
            else sb_append(ir->out, "or");
            sb_append(ir->out, " i1 ");
            ir_emit_temp(ir->out, tb1);
            sb_append(ir->out, ", ");
            ir_emit_temp(ir->out, tb2);
            sb_append(ir->out, "\n");
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tout);
            sb_append(ir->out, " = zext i1 ");
            ir_emit_temp(ir->out, tb3);
            sb_append(ir->out, " to i32\n");
            return value_temp(type_i32(), tout);
        }

        Value result = cg_call(ir, env, expr);
        /* Defensive: ensure result always has a valid type */
        if (!result.type) {
            result.type = type_i32();
        }
        return result;
    }

    Value result = value_const_i32(0);
    /* Defensive: ensure result always has a valid type */
    if (!result.type) {
        result.type = type_i32();
    }
    return result;
}
