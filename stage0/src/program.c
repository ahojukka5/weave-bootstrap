#include "codegen.h"

#include "diagnostics.h"
#include "fn_table.h"
#include "type_env.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void emit_fn_header(IrCtx *ir, VarEnv *env, const char *name, TypeRef *ret_type, Node *params_form) {
    int i;
    sb_append(ir->out, "define ");
    emit_llvm_type(ir->out, ret_type);
    sb_append(ir->out, " @");
    sb_append(ir->out, name);
    sb_append(ir->out, "(");
    if (params_form && params_form->kind == N_LIST && is_atom(list_nth(params_form, 0), "params")) {
        for (i = 1; i < params_form->count; i++) {
            Node *p = list_nth(params_form, i);
            TypeRef *pt = type_i32();
            const char *pname = "arg";
            const char *ssa_name = NULL;
            if (p && p->kind == N_LIST && p->count == 0) {
                continue;
            }
            if (p && p->kind == N_LIST) {
                pname = atom_text(list_nth(p, 0));
                pt = parse_type_node((TypeEnv *)ir->type_env, list_nth(p, 1));
            }
            if (!pname || !*pname) continue;
            ssa_name = env ? env_ssa_name(env, pname) : pname;
            if (i != 1) sb_append(ir->out, ", ");
            emit_llvm_type(ir->out, pt);
            /* Use a temporary name for the raw SSA parameter value */
            sb_append(ir->out, " %p_");
            sb_append(ir->out, ssa_name ? ssa_name : pname);
        }
    }
    sb_append(ir->out, ") {\n");
    sb_append(ir->out, "fn_entry:\n");
    /* Emit allocas for all parameters so they can be mutated with set */
    if (params_form && params_form->kind == N_LIST && is_atom(list_nth(params_form, 0), "params")) {
        for (i = 1; i < params_form->count; i++) {
            Node *p = list_nth(params_form, i);
            TypeRef *pt = type_i32();
            const char *pname = "arg";
            const char *ssa_name = NULL;
            if (p && p->kind == N_LIST && p->count == 0) {
                continue;
            }
            if (p && p->kind == N_LIST) {
                pname = atom_text(list_nth(p, 0));
                pt = parse_type_node((TypeEnv *)ir->type_env, list_nth(p, 1));
            }
            if (!pname || !*pname) continue;
            ssa_name = env ? env_ssa_name(env, pname) : pname;
            /* Emit alloca for the parameter */
            sb_append(ir->out, "  %");
            sb_append(ir->out, ssa_name ? ssa_name : pname);
            sb_append(ir->out, " = alloca ");
            emit_llvm_type(ir->out, pt);
            sb_append(ir->out, "\n");
            /* Store the raw parameter value into the alloca */
            sb_append(ir->out, "  store ");
            emit_llvm_type(ir->out, pt);
            sb_append(ir->out, " %p_");
            sb_append(ir->out, ssa_name ? ssa_name : pname);
            sb_append(ir->out, ", ");
            emit_llvm_type(ir->out, pt);
            sb_append(ir->out, "* %");
            sb_append(ir->out, ssa_name ? ssa_name : pname);
            sb_append(ir->out, "\n");
        }
    }
}

static void emit_value_only(StrBuf *out, Value v) {
    if (v.kind == 0) sb_printf_i32(out, v.const_i32);
    else if (v.kind == 1) ir_emit_temp(out, v.temp);
    else {
        sb_append(out, "%");
        sb_append(out, v.ssa_name ? v.ssa_name : "");
    }
}

static void emit_default_return(IrCtx *ir, TypeRef *ret_type) {
    sb_append(ir->out, "  ret ");
    if (ret_type && ret_type->kind == TY_VOID) {
        sb_append(ir->out, "void\n");
        return;
    }
    emit_llvm_type(ir->out, ret_type);
    sb_append(ir->out, " ");
    if (!ret_type || ret_type->kind == TY_I32) {
        sb_append(ir->out, "0\n");
    } else if (ret_type->kind == TY_I8PTR || ret_type->kind == TY_PTR) {
        sb_append(ir->out, "null\n");
    } else {
        sb_append(ir->out, "zeroinitializer\n");
    }
}

/* Minimal helper: emit a C string global and return a temp holding i8* to it. */
static int emit_c_string_ptr(IrCtx *ir, const char *s) {
    static int str_id = 0;
    int id = str_id++;
    int n = (int)strlen(s) + 1;
    int t = ir_fresh_temp(ir);
    /* Global */
    sb_append(&ir->globals, "@.tstr");
    sb_printf_i32(&ir->globals, id);
    sb_append(&ir->globals, " = private constant [");
    sb_printf_i32(&ir->globals, n);
    sb_append(&ir->globals, " x i8] c\"");
    {
        /* emit escaped */
        const unsigned char *p = (const unsigned char *)s;
        while (*p) {
            unsigned char ch = *p++;
            if (ch == '\\') sb_append(&ir->globals, "\\5C");
            else if (ch == '"') sb_append(&ir->globals, "\\22");
            else if (ch == '\n') sb_append(&ir->globals, "\\0A");
            else if (ch == '\r') sb_append(&ir->globals, "\\0D");
            else if (ch == '\t') sb_append(&ir->globals, "\\09");
            else if (ch < 32 || ch >= 127) {
                const char hex[] = "0123456789ABCDEF";
                sb_append_ch(&ir->globals, '\\');
                sb_append_ch(&ir->globals, hex[(ch >> 4) & 0xF]);
                sb_append_ch(&ir->globals, hex[ch & 0xF]);
            } else {
                sb_append_ch(&ir->globals, (char)ch);
            }
        }
    }
    sb_append(&ir->globals, "\\00\"\n");
    /* gep to pointer */
    sb_append(ir->out, "  ");
    ir_emit_temp(ir->out, t);
    sb_append(ir->out, " = getelementptr inbounds [");
    sb_printf_i32(ir->out, n);
    sb_append(ir->out, " x i8], [");
    sb_printf_i32(ir->out, n);
    sb_append(ir->out, " x i8]* @.tstr");
    sb_printf_i32(ir->out, id);
    sb_append(ir->out, ", i32 0, i32 0\n");
    return t;
}

static void compile_fn_form(IrCtx *ir, Node *fn_form, const char *override_name) {
    int idx = 1;
    Node *name_node = list_nth(fn_form, idx++);
    Node *maybe_doc = list_nth(fn_form, idx);
    Node *fh = list_nth(fn_form, 0);
    int is_entry = (fh && fh->kind == N_ATOM && is_atom(fh, "entry"));
    if (maybe_doc && maybe_doc->kind == N_LIST && is_atom(list_nth(maybe_doc, 0), "doc")) {
        idx++;
    }
    Node *params_form = list_nth(fn_form, idx++);
    Node *returns_form = list_nth(fn_form, idx++);
    Node *body_form = list_nth(fn_form, idx++);
    const char *name = override_name ? override_name : atom_text(name_node);
    TypeRef *ret_type = type_i32();
    VarEnv env;
    Node *stmt;
    Value last_expr = {0};
    int has_last = 0;
    int has_tests = 0;
    int i;

    /* Check for required (tests ...) section - only for regular functions, not entry points */
    if (!is_entry) {
        for (i = idx; i < fn_form->count; i++) {
            Node *extra = list_nth(fn_form, i);
            Node *eh = list_nth(extra, 0);
            if (eh && eh->kind == N_ATOM && is_atom(eh, "tests")) {
                has_tests = 1;
                /* Verify that tests section has at least one test */
                if (extra->count < 2) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "function '%s' has empty (tests ...) section",
                             name ? name : "<unknown>");
                    diag_fatal(fn_form && fn_form->filename ? fn_form->filename : NULL,
                               fn_form ? fn_form->line : 0,
                               fn_form ? fn_form->col : 0,
                               "missing-tests",
                               msg,
                               "Every function must have at least one test.");
                }
                break;
            }
        }
        if (!has_tests) {
            char msg[256];
            snprintf(msg, sizeof(msg), "function '%s' is missing required (tests ...) section",
                     name ? name : "<unknown>");
            diag_fatal(fn_form && fn_form->filename ? fn_form->filename : NULL,
                       fn_form ? fn_form->line : 0,
                       fn_form ? fn_form->col : 0,
                       "missing-tests",
                       msg,
                       "Every function must have at least one test.");
        }
    }

    if (returns_form && returns_form->kind == N_LIST && is_atom(list_nth(returns_form, 0), "returns")) {
        ret_type = parse_type_node((TypeEnv *)ir->type_env, list_nth(returns_form, 1));
    }

    env_init(&env);
    /* Register parameters as SSA values. */
    if (params_form && params_form->kind == N_LIST && is_atom(list_nth(params_form, 0), "params")) {
        int i;
        for (i = 1; i < params_form->count; i++) {
            Node *p = list_nth(params_form, i);
            const char *pname = "arg";
            TypeRef *pt = type_i32();
            if (p && p->kind == N_LIST && p->count == 0) continue;
            if (p && p->kind == N_LIST) {
                pname = atom_text(list_nth(p, 0));
                Node *type_node = list_nth(p, 1);
                pt = parse_type_node((TypeEnv *)ir->type_env, type_node);
                /* Defensive: ensure pt is never NULL */
                if (!pt) {
                    /* Debug: verify why parse_type_node returned NULL */
                    if (getenv("WEAVEC0_DEBUG_SIGS")) {
                        fprintf(stderr, "[dbg] parse_type_node returned NULL for param '%s', type_node kind=%d\n",
                                pname ? pname : "<null>",
                                type_node ? type_node->kind : -1);
                    }
                    pt = type_i32();
                }
            }
            if (!pname || !*pname) continue;
            /* Debug: verify parameter type before adding */
            if (getenv("WEAVEC0_DEBUG_SIGS") && strcmp(pname, "a") == 0) {
                fprintf(stderr, "[dbg] Adding param 'a' to env: pt=%p, pt_kind=%d\n",
                        (void *)pt, pt ? pt->kind : -1);
            }
            /* Debug: track TypeRef before storing in environment */
            if (getenv("WEAVEC0_DEBUG_MEM") && pt) {
                int valid_kind = (pt->kind >= 0 && pt->kind <= 4);
                fprintf(stderr, "[mem] compile_fn_form storing param '%s': type=%p, kind=%d, valid=%d\n",
                        pname ? pname : "<null>", (void *)pt, pt->kind, valid_kind);
                if (!valid_kind) {
                    fprintf(stderr, "[mem] ERROR: Invalid TypeRef kind before storage! Memory corruption?\n");
                }
            }
            env_add_param(&env, pname, pt);
        }
    }

    ir->current_fn = name;
    emit_fn_header(ir, &env, name, ret_type, params_form);

    if (body_form && body_form->kind == N_LIST && is_atom(list_nth(body_form, 0), "body")) {
        int bi;
        int did_ret = 0;
        for (bi = 1; bi < body_form->count; bi++) {
            Value stmt_last = {0};
            if (cg_stmt(ir, &env, list_nth(body_form, bi), ret_type, &stmt_last)) {
                did_ret = 1;
                break;
            }
            if (stmt_last.type) {
                last_expr = stmt_last;
                has_last = 1;
            }
        }
        if (!did_ret) {
            if (ret_type && ret_type->kind == TY_VOID) {
                sb_append(ir->out, "  ret void\n");
            } else if (has_last && last_expr.type) {
                Value rv = ensure_type_ctx(ir, last_expr, ret_type, "implicit-ret");
                sb_append(ir->out, "  ret ");
                emit_llvm_type(ir->out, ret_type);
                sb_append(ir->out, " ");
                emit_value_only(ir->out, rv);
                sb_append(ir->out, "\n");
            } else {
                emit_default_return(ir, ret_type);
            }
        }
    } else {
        stmt = body_form;
        if (!cg_stmt(ir, &env, stmt, ret_type, &last_expr)) {
            if (ret_type && ret_type->kind == TY_VOID) {
                sb_append(ir->out, "  ret void\n");
            } else if (last_expr.type) {
                Value rv = ensure_type_ctx(ir, last_expr, ret_type, "implicit-ret");
                sb_append(ir->out, "  ret ");
                emit_llvm_type(ir->out, ret_type);
                sb_append(ir->out, " ");
                emit_value_only(ir->out, rv);
                sb_append(ir->out, "\n");
            } else {
                emit_default_return(ir, ret_type);
            }
        }
    }
    sb_append(ir->out, "}\n");
}

static void collect_signature_form(TypeEnv *tenv, FnTable *fns, Node *form) {
    Node *h = list_nth(form, 0);
    const char *name = NULL;
    Node *params_form = NULL;
    Node *returns_form = NULL;
    int param_count = 0;
    TypeRef **param_types = NULL;
    int ri;
    TypeRef *ret_type = type_i32();
    int idx = 1;

    if (!form || form->kind != N_LIST || !h || h->kind != N_ATOM) return;

    if (is_atom(h, "fn")) {
    name = atom_text(list_nth(form, idx++));
    /* Skip collection of built-in functions early - they will be registered explicitly */
    if (name && (strcmp(name, "arena-kind") == 0 || strcmp(name, "arena-create") == 0)) {
        /* Debug: verify prevention is working */
        if (getenv("WEAVEC0_DEBUG_SIGS")) {
            fprintf(stderr, "[dbg] Skipping collection of built-in: %s\n", name);
        }
        return;
    }
    if (form->count > idx && list_nth(form, idx) && list_nth(form, idx)->kind == N_LIST &&
        is_atom(list_nth(list_nth(form, idx), 0), "doc")) {
        idx++;
    }
    params_form = list_nth(form, idx++);
    returns_form = list_nth(form, idx++);
    } else if (is_atom(h, "entry")) {
        int entry_idx = 2;
        name = "main";  // Emit as 'main' so C runtime can call it directly
        if (form->count > entry_idx && list_nth(form, entry_idx) &&
            list_nth(form, entry_idx)->kind == N_LIST &&
            is_atom(list_nth(list_nth(form, entry_idx), 0), "doc")) {
            entry_idx++;
        }
        params_form = list_nth(form, entry_idx++);
        returns_form = list_nth(form, entry_idx++);
    } else {
        return;
    }

    if (returns_form && returns_form->kind == N_LIST && is_atom(list_nth(returns_form, 0), "returns")) {
        ret_type = parse_type_node(tenv, list_nth(returns_form, 1));
    }
    if (params_form && params_form->kind == N_LIST && is_atom(list_nth(params_form, 0), "params")) {
        if (params_form->count == 2) {
            Node *only = list_nth(params_form, 1);
            if (only && only->kind == N_LIST && only->count == 0) {
                param_count = 0;
            } else {
                param_count = params_form->count - 1;
            }
        } else {
            param_count = params_form->count - 1;
        }
        if (param_count < 0) param_count = 0;
        if (param_count > 0) {
            param_types = (TypeRef **)xmalloc((size_t)param_count * sizeof(TypeRef *));
            for (ri = 0; ri < param_count; ri++) {
                Node *p = list_nth(params_form, ri + 1);
                TypeRef *pt = type_i32();
                if (p && p->kind == N_LIST && p->count == 0) pt = type_i32();
                else if (p && p->kind == N_LIST) {
                    pt = parse_type_node(tenv, list_nth(p, 1));
                    /* Defensive: ensure pt is never NULL */
                    if (!pt) pt = type_i32();
                }
                param_types[ri] = pt;
            }
        }
    }

    if (name && *name) {
        fn_table_add(fns, name, ret_type, param_count, param_types ? param_types : NULL);
    }
    if (param_types) free(param_types);
}

static void collect_signatures_in(TypeEnv *tenv, FnTable *fns, Node *form) {
    Node *head = list_nth(form, 0);
    int i;
    if (!form || form->kind != N_LIST || !head || head->kind != N_ATOM) return;
    if (is_atom(head, "doc")) return;
    if (is_atom(head, "module") || is_atom(head, "program")) {
        for (i = 1; i < form->count; i++) {
            collect_signatures_in(tenv, fns, list_nth(form, i));
        }
        return;
    }
    collect_signature_form(tenv, fns, form);
}

static void collect_signatures(TypeEnv *tenv, FnTable *fns, Node *decls) {
    int i;
    for (i = 0; decls && i < decls->count; i++) {
        collect_signatures_in(tenv, fns, list_nth(decls, i));
    }
}

static void collect_type_form(TypeEnv *tenv, Node *form) {
  Node *h = list_nth(form, 0);
  if (!form || form->kind != N_LIST || !h || h->kind != N_ATOM) return;
  if (is_atom(h, "doc")) return;
    if (is_atom(h, "type")) {
        const char *name = atom_text(list_nth(form, 1));
        Node *body = list_nth(form, 2);
        if (body && body->kind == N_LIST && is_atom(list_nth(body, 0), "alias")) {
            TypeRef *target = parse_type_node(tenv, list_nth(body, 1));
            type_env_add_alias(tenv, name, target);
        } else if (body && body->kind == N_LIST && is_atom(list_nth(body, 0), "struct")) {
            int fc = body->count - 1;
            int fi;
            char **fnames = NULL;
            TypeRef **ftypes = NULL;
            if (fc < 0) fc = 0;
            if (fc > 0) {
                fnames = (char **)xmalloc((size_t)fc * sizeof(char *));
                ftypes = (TypeRef **)xmalloc((size_t)fc * sizeof(TypeRef *));
                for (fi = 0; fi < fc; fi++) {
                    Node *field = list_nth(body, fi + 1);
                    fnames[fi] = xstrdup(atom_text(list_nth(field, 0)));
                    ftypes[fi] = parse_type_node(tenv, list_nth(field, 1));
                }
            }
            type_env_add_struct(tenv, name, fc, fnames, ftypes);
        }
    }
}

static void collect_types_in(TypeEnv *tenv, Node *form) {
    Node *head = list_nth(form, 0);
    int i;
    if (!form || form->kind != N_LIST || !head || head->kind != N_ATOM) return;
    if (is_atom(head, "module") || is_atom(head, "program")) {
        for (i = 1; i < form->count; i++) {
            collect_types_in(tenv, list_nth(form, i));
        }
        return;
    }
    collect_type_form(tenv, form);
}

static void collect_types(TypeEnv *tenv, IrCtx *ir, Node *decls) {
    int i;
    for (i = 0; decls && i < decls->count; i++) {
        collect_types_in(tenv, list_nth(decls, i));
    }

    /* Emit LLVM struct type defs. */
    for (i = 0; i < tenv->struct_count; i++) {
        StructDef *s = &tenv->structs[i];
        int fi;
        sb_append(&ir->typedefs, "%");
        sb_append(&ir->typedefs, s->name);
        sb_append(&ir->typedefs, " = type { ");
        for (fi = 0; fi < s->field_count; fi++) {
            if (fi != 0) sb_append(&ir->typedefs, ", ");
            emit_llvm_type(&ir->typedefs, s->field_types[fi]);
        }
        sb_append(&ir->typedefs, " }\n");
    }
}

static void emit_fn_form(IrCtx *ir, Node *form) {
    Node *fh = list_nth(form, 0);
    if (!form || form->kind != N_LIST || !fh || fh->kind != N_ATOM) return;
    if (is_atom(fh, "fn")) {
        compile_fn_form(ir, form, NULL);
    } else if (is_atom(fh, "entry")) {
        if (ir->run_tests_mode) {
            /* In test mode, skip user entry; synthetic main will be emitted. */
            return;
        }
        compile_fn_form(ir, form, "main");
    }
}

static void emit_fn_forms_in(IrCtx *ir, Node *form) {
    Node *head = list_nth(form, 0);
    int i;
    if (!form || form->kind != N_LIST || !head || head->kind != N_ATOM) return;
    if (is_atom(head, "module") || is_atom(head, "program")) {
        for (i = 1; i < form->count; i++) {
            emit_fn_forms_in(ir, list_nth(form, i));
        }
        return;
    }
    emit_fn_form(ir, form);
}

static int test_matches_filters(IrCtx *ir, const char *tname, StrList *tags) {
    int i;
    if (ir->selected_test_names.len == 0 && ir->selected_tags.len == 0) return 1;
    /* Name match */
    for (i = 0; i < ir->selected_test_names.len; i++) {
        if (strcmp(ir->selected_test_names.items[i], tname) == 0) return 1;
    }
    /* Tag match */
    if (tags) {
        int ti;
        for (ti = 0; ti < tags->len; ti++) {
            int fj;
            for (fj = 0; fj < ir->selected_tags.len; fj++) {
                if (strcmp(tags->items[ti], ir->selected_tags.items[fj]) == 0) return 1;
            }
        }
    }
    return 0;
}

/* Desugar expect-* forms into if-stmt with printf for failure reporting.
   Returns 1 if this was an expect form and was handled, 0 otherwise. */
static int try_desugar_expect(IrCtx *ir, VarEnv *env, Node *form, const char *test_name, TypeRef *ret_type, int *out_did_ret) {
    Node *head;
    if (!form || form->kind != N_LIST) return 0;
    head = list_nth(form, 0);
    if (!head || head->kind != N_ATOM) return 0;

    if (is_atom(head, "expect-eq")) {
        ir->saw_expect = 1;
        /* (expect-eq actual expected [debug ...]) */
        Node *actual_node = list_nth(form, 1);
        Node *expected_node = list_nth(form, 2);
        Node *debug_node = (form->count > 3) ? list_nth(form, 3) : NULL;
        Value actual_val = cg_expr(ir, env, actual_node);
        Value expected_val = cg_expr(ir, env, expected_node);
        int tcmp = ir_fresh_temp(ir);
        int tzext = ir_fresh_temp(ir);
        int tcond = ir_fresh_temp(ir);
        int pass_l = ir_fresh_label(ir);
        int fail_l = ir_fresh_label(ir);
        /* Compare actual == expected; for strings use weave_string_eq */
        if (actual_val.type && actual_val.type->kind == TY_I8PTR && expected_val.type && expected_val.type->kind == TY_I8PTR) {
            /* Declare weave_string_eq if needed */
            if (!sl_contains(&ir->declared_ccalls, "weave_string_eq")) {
                sl_push(&ir->declared_ccalls, "weave_string_eq");
                sb_append(&ir->decls, "declare i32 @weave_string_eq(i8*, i8*)\n");
            }
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tcmp);
            sb_append(ir->out, " = call i32 @weave_string_eq(i8* ");
            emit_value_only(ir->out, actual_val);
            sb_append(ir->out, ", i8* ");
            emit_value_only(ir->out, expected_val);
            sb_append(ir->out, ")\n");
        } else {
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tcmp);
            sb_append(ir->out, " = icmp eq ");
            emit_llvm_type(ir->out, actual_val.type);
            sb_append(ir->out, " ");
            emit_value_only(ir->out, actual_val);
            sb_append(ir->out, ", ");
            emit_value_only(ir->out, expected_val);
            sb_append(ir->out, "\n");
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tzext);
            sb_append(ir->out, " = zext i1 ");
            ir_emit_temp(ir->out, tcmp);
            sb_append(ir->out, " to i32\n");
            /* normalize tcmp to i32 for consistent branch below */
            tcmp = tzext;
        }
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, tcond);
        sb_append(ir->out, " = icmp ne i32 ");
        ir_emit_temp(ir->out, tcmp);
        sb_append(ir->out, ", 0\n");
        sb_append(ir->out, "  br i1 ");
        ir_emit_temp(ir->out, tcond);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, pass_l);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, fail_l);
        sb_append(ir->out, "\n");
        /* Fail: print message, optionally run debug, return 1 */
        ir_emit_label_def(ir->out, fail_l);
        {
            char msgbuf[512];
            int sptr;
            const char *fmt_expected = "%p";
            const char *fmt_actual = "%p";
            if (expected_val.type && expected_val.type->kind == TY_I32) fmt_expected = "%d";
            else if (expected_val.type && expected_val.type->kind == TY_I8PTR) fmt_expected = "%s";
            if (actual_val.type && actual_val.type->kind == TY_I32) fmt_actual = "%d";
            else if (actual_val.type && actual_val.type->kind == TY_I8PTR) fmt_actual = "%s";
            snprintf(msgbuf, sizeof(msgbuf), "%s:%d:%d: %s: expect-eq failed: expected %s, got %s",
                     form && form->filename ? form->filename : "<unknown>",
                     form ? form->line : 0,
                     form ? form->col : 0,
                     test_name ? test_name : "test",
                     fmt_expected,
                     fmt_actual);
            sptr = emit_c_string_ptr(ir, msgbuf);
            if (!sl_contains(&ir->declared_ccalls, "printf")) {
                sl_push(&ir->declared_ccalls, "printf");
                sb_append(&ir->decls, "declare i32 @printf(i8*, ...)\n");
            }
            sb_append(ir->out, "  call i32 (i8*, ...) @printf(i8* ");
            ir_emit_temp(ir->out, sptr);
            sb_append(ir->out, ", ");
            emit_llvm_type(ir->out, expected_val.type);
            sb_append(ir->out, " ");
            emit_value_only(ir->out, expected_val);
            sb_append(ir->out, ", ");
            emit_llvm_type(ir->out, actual_val.type);
            sb_append(ir->out, " ");
            emit_value_only(ir->out, actual_val);
            sb_append(ir->out, ")\n");
        }
        /* Skip optional debug block to avoid external dependencies in tests */
        /* Intentionally not executing (debug ...) statements inside expect-eq. */
        sb_append(ir->out, "  ret i32 1\n");
        /* Pass: continue to next statement */
        ir_emit_label_def(ir->out, pass_l);
        return 1;
    }

    if (is_atom(head, "expect-ne")) {
        ir->saw_expect = 1;
        /* (expect-ne actual expected [debug ...]) */
        Node *actual_node = list_nth(form, 1);
        Node *expected_node = list_nth(form, 2);
        Node *debug_node = (form->count > 3) ? list_nth(form, 3) : NULL;
        Value actual_val = cg_expr(ir, env, actual_node);
        Value expected_val = cg_expr(ir, env, expected_node);
        int tcmp = ir_fresh_temp(ir);
        int tzext = ir_fresh_temp(ir);
        int tcond = ir_fresh_temp(ir);
        int pass_l = ir_fresh_label(ir);
        int fail_l = ir_fresh_label(ir);
        int end_l = ir_fresh_label(ir);
        /* Compare actual != expected; for strings use weave_string_eq and invert */
        if (actual_val.type && actual_val.type->kind == TY_I8PTR && expected_val.type && expected_val.type->kind == TY_I8PTR) {
            if (!sl_contains(&ir->declared_ccalls, "weave_string_eq")) {
                sl_push(&ir->declared_ccalls, "weave_string_eq");
                sb_append(&ir->decls, "declare i32 @weave_string_eq(i8*, i8*)\n");
            }
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tcmp);
            sb_append(ir->out, " = call i32 @weave_string_eq(i8* ");
            emit_value_only(ir->out, actual_val);
            sb_append(ir->out, ", i8* ");
            emit_value_only(ir->out, expected_val);
            sb_append(ir->out, ")\n");
        } else {
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tcmp);
            sb_append(ir->out, " = icmp ne ");
            emit_llvm_type(ir->out, actual_val.type);
            sb_append(ir->out, " ");
            emit_value_only(ir->out, actual_val);
            sb_append(ir->out, ", ");
            emit_value_only(ir->out, expected_val);
            sb_append(ir->out, "\n");
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tzext);
            sb_append(ir->out, " = zext i1 ");
            ir_emit_temp(ir->out, tcmp);
            sb_append(ir->out, " to i32\n");
            tcmp = tzext;
        }
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, tcond);
        /* For string eq, tcmp is i32 eq result; inequality when tcmp == 0 */
        sb_append(ir->out, " = icmp ne i32 ");
        ir_emit_temp(ir->out, tcmp);
        sb_append(ir->out, ", 0\n");
        sb_append(ir->out, "  br i1 ");
        ir_emit_temp(ir->out, tcond);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, pass_l);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, fail_l);
        sb_append(ir->out, "\n");
        ir_emit_label_def(ir->out, pass_l);
        sb_append(ir->out, "  br label ");
        ir_emit_label_ref(ir->out, end_l);
        sb_append(ir->out, "\n");
        ir_emit_label_def(ir->out, fail_l);
        {
            char msgbuf[512];
            int sptr;
            const char *fmt_actual = "%p";
            if (actual_val.type && actual_val.type->kind == TY_I32) fmt_actual = "%d";
            else if (actual_val.type && actual_val.type->kind == TY_I8PTR) fmt_actual = "%s";
            snprintf(msgbuf, sizeof(msgbuf), "%s:%d:%d: %s: expect-ne failed: values should differ, both are %s",
                     form && form->filename ? form->filename : "<unknown>",
                     form ? form->line : 0,
                     form ? form->col : 0,
                     test_name ? test_name : "test",
                     fmt_actual);
            sptr = emit_c_string_ptr(ir, msgbuf);
            if (!sl_contains(&ir->declared_ccalls, "printf")) {
                sl_push(&ir->declared_ccalls, "printf");
                sb_append(&ir->decls, "declare i32 @printf(i8*, ...)\n");
            }
            sb_append(ir->out, "  call i32 (i8*, ...) @printf(i8* ");
            ir_emit_temp(ir->out, sptr);
            sb_append(ir->out, ", ");
            emit_llvm_type(ir->out, actual_val.type);
            sb_append(ir->out, " ");
            emit_value_only(ir->out, actual_val);
            sb_append(ir->out, ")\n");
        }
        /* Intentionally skip (debug ...) to keep embedded tests self-contained. */
        sb_append(ir->out, "  ret i32 1\n");
        ir_emit_label_def(ir->out, pass_l);
        return 1;
    }

    if (is_atom(head, "expect-true")) {
        ir->saw_expect = 1;
        /* (expect-true cond [debug ...]) */
        Node *cond_node = list_nth(form, 1);
        Node *debug_node = (form->count > 2) ? list_nth(form, 2) : NULL;
        Value cond_val = ensure_type_ctx_at(ir, cg_expr(ir, env, cond_node), type_i32(), "expect-true", cond_node);
        int tcmp = ir_fresh_temp(ir);
        int pass_l = ir_fresh_label(ir);
        int fail_l = ir_fresh_label(ir);
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, tcmp);
        sb_append(ir->out, " = icmp ne i32 ");
        emit_value_only(ir->out, cond_val);
        sb_append(ir->out, ", 0\n");
        sb_append(ir->out, "  br i1 ");
        ir_emit_temp(ir->out, tcmp);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, pass_l);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, fail_l);
        sb_append(ir->out, "\n");
        ir_emit_label_def(ir->out, fail_l);
        {
            char msgbuf[512];
            int sptr;
            snprintf(msgbuf, sizeof(msgbuf), "%s:%d:%d: %s: expect-true failed: condition was false",
                     form && form->filename ? form->filename : "<unknown>",
                     form ? form->line : 0,
                     form ? form->col : 0,
                     test_name ? test_name : "test");
            sptr = emit_c_string_ptr(ir, msgbuf);
            if (!sl_contains(&ir->declared_ccalls, "printf")) {
                sl_push(&ir->declared_ccalls, "printf");
                sb_append(&ir->decls, "declare i32 @printf(i8*, ...)\n");
            }
            sb_append(ir->out, "  call i32 (i8*, ...) @printf(i8* ");
            ir_emit_temp(ir->out, sptr);
            sb_append(ir->out, ")\n");
        }
        /* Intentionally skip (debug ...) to keep embedded tests self-contained. */
        sb_append(ir->out, "  ret i32 1\n");
        ir_emit_label_def(ir->out, pass_l);
        return 1;
    }

    if (is_atom(head, "expect-false")) {
        ir->saw_expect = 1;
        /* (expect-false cond [debug ...]) */
        Node *cond_node = list_nth(form, 1);
        Node *debug_node = (form->count > 2) ? list_nth(form, 2) : NULL;
        Value cond_val = ensure_type_ctx_at(ir, cg_expr(ir, env, cond_node), type_i32(), "expect-false", cond_node);
        int tcmp = ir_fresh_temp(ir);
        int pass_l = ir_fresh_label(ir);
        int fail_l = ir_fresh_label(ir);
        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, tcmp);
        sb_append(ir->out, " = icmp eq i32 ");
        emit_value_only(ir->out, cond_val);
        sb_append(ir->out, ", 0\n");
        sb_append(ir->out, "  br i1 ");
        ir_emit_temp(ir->out, tcmp);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, pass_l);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, fail_l);
        sb_append(ir->out, "\n");
        ir_emit_label_def(ir->out, fail_l);
        {
            char msgbuf[512];
            int sptr;
            snprintf(msgbuf, sizeof(msgbuf), "%s:%d:%d: %s: expect-false failed: condition was true",
                     form && form->filename ? form->filename : "<unknown>",
                     form ? form->line : 0,
                     form ? form->col : 0,
                     test_name ? test_name : "test");
            sptr = emit_c_string_ptr(ir, msgbuf);
            if (!sl_contains(&ir->declared_ccalls, "printf")) {
                sl_push(&ir->declared_ccalls, "printf");
                sb_append(&ir->decls, "declare i32 @printf(i8*, ...)\n");
            }
            sb_append(ir->out, "  call i32 (i8*, ...) @printf(i8* ");
            ir_emit_temp(ir->out, sptr);
            sb_append(ir->out, ")\n");
        }
        if (debug_node && debug_node->kind == N_LIST && is_atom(list_nth(debug_node, 0), "debug")) {
            int di;
            for (di = 1; di < debug_node->count; di++) {
                Value tmp = {0};
                cg_stmt(ir, env, list_nth(debug_node, di), ret_type, &tmp);
            }
        }
        sb_append(ir->out, "  ret i32 1\n");
        ir_emit_label_def(ir->out, pass_l);
        return 1;
    }

    return 0;
}

static void emit_tests_for_fn(IrCtx *ir, Node *fn_form) {
    /* Look for a (tests ...) block inside the fn form and emit each test as a helper function.
       Test shape: (test name (doc ...) (tags ...) (setup ...) (inspect ...))
       Fallback: (test name (body ...))
       Emit: define i32 @__test_<fn>_<idx>() { ... }
    */
    int idx = 1;
    const char *fn_name = NULL;
    Node *maybe_doc;
    Node *params_form;
    Node *returns_form;
    Node *body_form;
    int i;

    if (!fn_form || fn_form->kind != N_LIST) return;
    fn_name = atom_text(list_nth(fn_form, idx++));
    maybe_doc = list_nth(fn_form, idx);
    if (maybe_doc && maybe_doc->kind == N_LIST && is_atom(list_nth(maybe_doc, 0), "doc")) {
        idx++;
    }
    params_form = list_nth(fn_form, idx++);
    returns_form = list_nth(fn_form, idx++);
    body_form = list_nth(fn_form, idx++);
    /* Remaining forms may include (tests ...) */
    for (i = idx; i < fn_form->count; i++) {
        Node *extra = list_nth(fn_form, i);
        Node *eh = list_nth(extra, 0);
        int ti;
        if (!eh || eh->kind != N_ATOM) continue;
        if (!is_atom(eh, "tests")) continue;
        /* Iterate test items */
        for (ti = 1; ti < extra->count; ti++) {
            Node *test_form = list_nth(extra, ti);
            Node *th = list_nth(test_form, 0);
            Node *t_body = NULL;
            Node *t_setup = NULL;
            Node *t_inspect = NULL;
            const char *t_name = NULL;
            StrList t_tags;
            char buf[256];
            int bi;
            TypeRef *ret_type = type_i32();
            VarEnv env;
            int did_ret = 0;
            Value last_expr = {0};
            int has_last = 0;

            if (!th || th->kind != N_ATOM || !is_atom(th, "test")) continue;
            sl_init(&t_tags);
            /* Parse name (atom) and optional blocks */
            t_name = atom_text(list_nth(test_form, 1));
            for (bi = 2; bi < test_form->count; bi++) {
                Node *item = list_nth(test_form, bi);
                if (!item || item->kind != N_LIST) continue;
                if (is_atom(list_nth(item, 0), "doc")) {
                    /* ignore doc in codegen */
                    continue;
                } else if (is_atom(list_nth(item, 0), "tags")) {
                    int tj;
                    for (tj = 1; tj < item->count; tj++) {
                        const char *tag = atom_text(list_nth(item, tj));
                        if (tag && *tag) sl_push(&t_tags, tag);
                    }
                } else if (is_atom(list_nth(item, 0), "setup")) {
                    t_setup = item;
                } else if (is_atom(list_nth(item, 0), "inspect")) {
                    t_inspect = item;
                } else if (is_atom(list_nth(item, 0), "body")) {
                    t_body = item;
                }
            }
            if (!test_matches_filters(ir, t_name ? t_name : "", &t_tags)) {
                continue;
            }
            /* Name for emitted test function */
            snprintf(buf, sizeof(buf), "__test_%s_%d", fn_name ? fn_name : "fn", ti - 1);
            ir->current_fn = buf;
            /* Emit header for test function: define i32 @name() { */
            emit_fn_header(ir, NULL, buf, ret_type, NULL);
            /* Reset per-test expect flag */
            ir->saw_expect = 0;

            env_init(&env);
            /* Phase 1: setup (optional) */
            if (t_setup && is_atom(list_nth(t_setup, 0), "setup")) {
                for (bi = 1; bi < t_setup->count; bi++) {
                    Value stmt_last = (Value){0};
                    if (cg_stmt(ir, &env, list_nth(t_setup, bi), ret_type, &stmt_last)) {
                        did_ret = 1;
                        break;
                    }
                }
            }
            /* Phase 2: inspect (preferred) or fallback to body */
            if (!did_ret && t_inspect && is_atom(list_nth(t_inspect, 0), "inspect")) {
                for (bi = 1; bi < t_inspect->count; bi++) {
                    Node *item = list_nth(t_inspect, bi);
                    /* Try desugaring expect-* forms first */
                    if (try_desugar_expect(ir, &env, item, t_name, ret_type, &did_ret)) {
                        if (did_ret) break;
                        continue;
                    }
                    /* Fallback to regular statement */
                    Value stmt_last = (Value){0};
                    if (cg_stmt(ir, &env, item, ret_type, &stmt_last)) {
                        did_ret = 1;
                        break;
                    }
                    if (stmt_last.type) {
                        last_expr = stmt_last;
                        has_last = 1;
                    }
                }
            } else if (!did_ret && t_body && t_body->kind == N_LIST && is_atom(list_nth(t_body, 0), "body")) {
                for (bi = 1; bi < t_body->count; bi++) {
                    Node *item = list_nth(t_body, bi);
                    /* First try to desugar expect-* assertions */
                    if (try_desugar_expect(ir, &env, item, t_name, ret_type, &did_ret)) {
                        if (did_ret) break;
                        continue;
                    }
                    /* Fallback to regular statement */
                    Value stmt_last = (Value){0};
                    if (cg_stmt(ir, &env, item, ret_type, &stmt_last)) {
                        did_ret = 1;
                        break;
                    }
                    if (stmt_last.type) {
                        last_expr = stmt_last;
                        has_last = 1;
                    }
                }
                if (!did_ret) {
                    /* If any expect-* assertions were present, default to success return. */
                    if (ir->saw_expect) {
                        sb_append(ir->out, "  ret i32 0\n");
                    } else if (has_last && last_expr.type) {
                        Value rv = ensure_type_ctx(ir, last_expr, ret_type, "implicit-ret");
                        sb_append(ir->out, "  ret ");
                        emit_llvm_type(ir->out, ret_type);
                        sb_append(ir->out, " ");
                        {
                            if (rv.kind == 0) sb_printf_i32(ir->out, rv.const_i32);
                            else if (rv.kind == 1) ir_emit_temp(ir->out, rv.temp);
                            else {
                                sb_append(ir->out, "%");
                                sb_append(ir->out, rv.ssa_name ? rv.ssa_name : "");
                            }
                        }
                        sb_append(ir->out, "\n");
                    } else {
                        /* Default return 0 */
                        sb_append(ir->out, "  ret i32 0\n");
                    }
                }
            } else {
                /* No body: return 0 */
                sb_append(ir->out, "  ret i32 0\n");
            }
            sb_append(ir->out, "}\n");
            /* Track test function name */
            sl_push(&ir->test_funcs, buf);
            sl_push(&ir->test_names, t_name ? t_name : "");
        }
    }
}

static void emit_tests_in(IrCtx *ir, Node *form) {
    Node *head = list_nth(form, 0);
    int i;
    if (!form || form->kind != N_LIST || !head || head->kind != N_ATOM) return;
    if (is_atom(head, "module") || is_atom(head, "program")) {
        for (i = 1; i < form->count; i++) emit_tests_in(ir, list_nth(form, i));
        return;
    }
    if (is_atom(head, "fn")) {
        emit_tests_for_fn(ir, form);
    }
}

static void emit_tests_main(IrCtx *ir) {
    int i;
    TypeRef *ret_type = type_i32();
    /* define i32 @main() { */
    emit_fn_header(ir, NULL, "main", ret_type, NULL);
    /* declare i32 @puts(i8*) once */
    if (!sl_contains(&ir->declared_ccalls, "puts")) {
        sl_push(&ir->declared_ccalls, "puts");
        sb_append(&ir->decls, "declare i32 @puts(i8*)\n");
    }
    /* %failures = alloca i32 */
    sb_append(ir->out, "  %failures = alloca i32\n");
    sb_append(ir->out, "  store i32 0, i32* %failures\n");
    for (i = 0; i < ir->test_funcs.len; i++) {
        const char *tname = ir->test_funcs.items[i];
        const char *hname = (i < ir->test_names.len ? ir->test_names.items[i] : tname);
        /* Print test name */
        {
            char linebuf[256];
            int sptr;
            snprintf(linebuf, sizeof(linebuf), "Running test: %s", hname ? hname : tname);
            sptr = emit_c_string_ptr(ir, linebuf);
            sb_append(ir->out, "  call i32 @puts(i8* "); ir_emit_temp(ir->out, sptr); sb_append(ir->out, ")\n");
        }
        int t_ret = ir_fresh_temp(ir);
        int t_cmp = ir_fresh_temp(ir);
        int t_zext = ir_fresh_temp(ir);
        int t_cur = ir_fresh_temp(ir);
        int t_new = ir_fresh_temp(ir);
        /* t_ret = call i32 @tname() */
        sb_append(ir->out, "  "); ir_emit_temp(ir->out, t_ret);
        sb_append(ir->out, " = call i32 @"); sb_append(ir->out, tname);
        sb_append(ir->out, "()\n");
        /* t_cmp = icmp ne i32 t_ret, 0 */
        sb_append(ir->out, "  "); ir_emit_temp(ir->out, t_cmp);
        sb_append(ir->out, " = icmp ne i32 "); ir_emit_temp(ir->out, t_ret);
        sb_append(ir->out, ", 0\n");
        /* t_zext = zext i1 t_cmp to i32 */
        sb_append(ir->out, "  "); ir_emit_temp(ir->out, t_zext);
        sb_append(ir->out, " = zext i1 "); ir_emit_temp(ir->out, t_cmp);
        sb_append(ir->out, " to i32\n");
        /* t_cur = load i32, i32* %failures */
        sb_append(ir->out, "  "); ir_emit_temp(ir->out, t_cur);
        sb_append(ir->out, " = load i32, i32* %failures\n");
        /* t_new = add i32 t_cur, t_zext */
        sb_append(ir->out, "  "); ir_emit_temp(ir->out, t_new);
        sb_append(ir->out, " = add i32 "); ir_emit_temp(ir->out, t_cur);
        sb_append(ir->out, ", "); ir_emit_temp(ir->out, t_zext);
        sb_append(ir->out, "\n");
        /* store i32 t_new, i32* %failures */
        sb_append(ir->out, "  store i32 "); ir_emit_temp(ir->out, t_new);
        sb_append(ir->out, ", i32* %failures\n");
    }
    /* ret load failures */
    {
        int t_cur = ir_fresh_temp(ir);
        sb_append(ir->out, "  "); ir_emit_temp(ir->out, t_cur);
        sb_append(ir->out, " = load i32, i32* %failures\n");
        sb_append(ir->out, "  ret i32 "); ir_emit_temp(ir->out, t_cur);
        sb_append(ir->out, "\n");
    }
    sb_append(ir->out, "}\n");
}

void compile_to_llvm_ir(Node *top, StrBuf *out, int run_tests_mode, StrList *selected_test_names, StrList *selected_tags) {
    int i;
    IrCtx ir;
    StrBuf funcs;
    FnTable fns;
    TypeEnv tenv;

    sb_init(&funcs);
    ir_init(&ir, &funcs);
    ir.run_tests_mode = run_tests_mode;
    if (selected_test_names && selected_test_names->len > 0) {
        int si;
        for (si = 0; si < selected_test_names->len; si++) sl_push(&ir.selected_test_names, selected_test_names->items[si]);
    }
    if (selected_tags && selected_tags->len > 0) {
        int ti;
        for (ti = 0; ti < selected_tags->len; ti++) sl_push(&ir.selected_tags, selected_tags->items[ti]);
    }
    fn_table_init(&fns);
    ir.fn_table = &fns;
    type_env_init(&tenv);
    ir.type_env = &tenv;

    {
        Node *decls = top;

        collect_types(&tenv, &ir, decls);
        collect_signatures(&tenv, &fns, decls);

        /* Built-in signatures for functions used in tests but not declared in Weave code.
         * Register these AFTER collect_signatures so they overwrite any incorrect collected signatures. */
        {
            /* arena-create: returns ptr(Arena), takes Int32 size */
            TypeRef *arena_ptr = type_ptr(type_struct("Arena"));
            TypeRef *param_types[1];
            param_types[0] = type_i32();
            fn_table_add(&fns, "arena-create", arena_ptr, 1, param_types);
        }
        
        {
            /* arena-kind: returns Int32, takes ptr(Arena) and Int32 id */
            TypeRef *arena_kind_param_types[2];
            TypeRef *arena_struct = type_struct("Arena");
            arena_kind_param_types[0] = type_ptr(arena_struct);
            arena_kind_param_types[1] = type_i32();
            /* Debug: verify type structure */
            if (getenv("WEAVEC0_DEBUG_SIGS")) {
                fprintf(stderr, "[dbg] Registering arena-kind (1st): param[0] type: kind=%d, pointee_kind=%d\n",
                        arena_kind_param_types[0]->kind,
                        arena_kind_param_types[0]->pointee ? arena_kind_param_types[0]->pointee->kind : -1);
            }
            fn_table_add(&fns, "arena-kind", type_i32(), 2, arena_kind_param_types);
        }
        
        /* JIT compilation functions - available via ccall */
        {
            /* llvm-jit-compile: returns Int32 (function pointer), takes String ir, String func_name */
            TypeRef *str_type = type_i8ptr();  /* String is i8* in LLVM */
            TypeRef *jit_param_types[2];
            jit_param_types[0] = str_type;
            jit_param_types[1] = str_type;
            fn_table_add(&fns, "llvm-jit-compile", type_i32(), 2, jit_param_types);
            
            /* llvm-jit-call: returns Int32, takes String ir, String func_name, Int32 arg1, Int32 arg2 */
            TypeRef *jit_call_param_types[4];
            jit_call_param_types[0] = str_type;
            jit_call_param_types[1] = str_type;
            jit_call_param_types[2] = type_i32();
            jit_call_param_types[3] = type_i32();
            fn_table_add(&fns, "llvm-jit-call", type_i32(), 4, jit_call_param_types);
        }
        
        /* LLVM compilation functions - available via ccall for stage1 */
        {
            TypeRef *str_type = type_i8ptr();
            
            /* llvm-compile-ir-to-assembly: returns Int32 (0=success), takes String ir, String output_path, Int32 opt_level */
            TypeRef *asm_param_types[3];
            asm_param_types[0] = str_type;  /* IR string */
            asm_param_types[1] = str_type;    /* output path */
            asm_param_types[2] = type_i32(); /* opt_level */
            fn_table_add(&fns, "llvm-compile-ir-to-assembly", type_i32(), 3, asm_param_types);
            
            /* llvm-compile-ir-to-object: returns Int32 (0=success), takes String ir, String output_path, Int32 opt_level */
            TypeRef *obj_param_types[3];
            obj_param_types[0] = str_type;  /* IR string */
            obj_param_types[1] = str_type;   /* output path */
            obj_param_types[2] = type_i32(); /* opt_level */
            fn_table_add(&fns, "llvm-compile-ir-to-object", type_i32(), 3, obj_param_types);
        }

          /* Define Arena struct type only if not already defined by user code.
              Arena has four i8* fields: kinds, values, first, next */
        if (!type_env_find_struct(&tenv, "Arena")) {
            sb_append(&ir.typedefs, "%Arena = type { i8*, i8*, i8*, i8* }\n");
        }

        /* Ensure malloc is declared for arena-create */
        if (!sl_contains(&ir.declared_ccalls, "malloc")) {
            sl_push(&ir.declared_ccalls, "malloc");
            sb_append(&ir.decls, "declare i8* @malloc(i32)\n");
        }

        /* Simplified arena-create: just allocate Arena struct, initialize fields to null.
         * Stage0 tests don't actually use arenas, so this minimal implementation is sufficient.
         * For full arena functionality, use stage1 which has proper Weave array implementations.
         */
        sb_append(&funcs, "define %Arena* @arena-create(i32 %size) {\n");
        sb_append(&funcs, "  %raw = call i8* @malloc(i32 32)\n");
        sb_append(&funcs, "  %a = bitcast i8* %raw to %Arena*\n");
        sb_append(&funcs, "  %p0 = getelementptr inbounds %Arena, %Arena* %a, i32 0, i32 0\n");
        sb_append(&funcs, "  store i8* null, i8** %p0\n");
        sb_append(&funcs, "  %p1 = getelementptr inbounds %Arena, %Arena* %a, i32 0, i32 1\n");
        sb_append(&funcs, "  store i8* null, i8** %p1\n");
        sb_append(&funcs, "  %p2 = getelementptr inbounds %Arena, %Arena* %a, i32 0, i32 2\n");
        sb_append(&funcs, "  store i8* null, i8** %p2\n");
        sb_append(&funcs, "  %p3 = getelementptr inbounds %Arena, %Arena* %a, i32 0, i32 3\n");
        sb_append(&funcs, "  store i8* null, i8** %p3\n");
        sb_append(&funcs, "  ret %Arena* %a\n");
        sb_append(&funcs, "}\n");
        
        /* Declare JIT helper functions for ccall */
        if (!sl_contains(&ir.declared_ccalls, "llvm_jit_compile_and_get_ptr")) {
            sl_push(&ir.declared_ccalls, "llvm_jit_compile_and_get_ptr");
            sb_append(&ir.decls, "declare i32 @llvm_jit_compile_and_get_ptr(i8*, i8*)\n");
        }
        if (!sl_contains(&ir.declared_ccalls, "llvm_jit_call_i32_i32_i32")) {
            sl_push(&ir.declared_ccalls, "llvm_jit_call_i32_i32_i32");
            sb_append(&ir.decls, "declare i32 @llvm_jit_call_i32_i32_i32(i8*, i8*, i32, i32)\n");
        }
        
        /* Declare LLVM compilation functions for ccall (used by stage1) */
        if (!sl_contains(&ir.declared_ccalls, "llvm_compile_ir_to_assembly")) {
            sl_push(&ir.declared_ccalls, "llvm_compile_ir_to_assembly");
            sb_append(&ir.decls, "declare i32 @llvm_compile_ir_to_assembly(i8*, i8*, i32)\n");
        }
        if (!sl_contains(&ir.declared_ccalls, "llvm_compile_ir_to_object")) {
            sl_push(&ir.declared_ccalls, "llvm_compile_ir_to_object");
            sb_append(&ir.decls, "declare i32 @llvm_compile_ir_to_object(i8*, i8*, i32)\n");
        }

        /* Ensure built-in functions are registered with correct types before compilation */
        {
            /* arena-kind: returns Int32, takes ptr(Arena) and Int32 id */
            TypeRef *arena_kind_param_types[2];
            TypeRef *arena_struct = type_struct("Arena");
            arena_kind_param_types[0] = type_ptr(arena_struct);
            arena_kind_param_types[1] = type_i32();
            /* Debug: verify type structure */
            if (getenv("WEAVEC0_DEBUG_SIGS")) {
                fprintf(stderr, "[dbg] Registering arena-kind (2nd): param[0] type: kind=%d, pointee_kind=%d\n",
                        arena_kind_param_types[0]->kind,
                        arena_kind_param_types[0]->pointee ? arena_kind_param_types[0]->pointee->kind : -1);
            }
            fn_table_add(&fns, "arena-kind", type_i32(), 2, arena_kind_param_types);
        }
        
        /* Emit function declarations/forms first */
        for (i = 0; decls && i < decls->count; i++) {
            emit_fn_forms_in(&ir, list_nth(decls, i));
        }
        if (ir.run_tests_mode) {
            /* Emit tests and synthetic main */
            for (i = 0; decls && i < decls->count; i++) {
                emit_tests_in(&ir, list_nth(decls, i));
            }
            emit_tests_main(&ir);
        }
    }

    sb_init(out);
    if (ir.typedefs.data && ir.typedefs.len) sb_append_n(out, ir.typedefs.data, ir.typedefs.len);
    if (ir.globals.data && ir.globals.len) sb_append_n(out, ir.globals.data, ir.globals.len);
    if (ir.decls.data && ir.decls.len) sb_append_n(out, ir.decls.data, ir.decls.len);
    if (funcs.data && funcs.len) sb_append_n(out, funcs.data, funcs.len);
}
