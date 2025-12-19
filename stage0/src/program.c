#include "codegen.h"

#include "fn_table.h"
#include "type_env.h"

#include <stdlib.h>
#include <string.h>

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

static void compile_fn_form(IrCtx *ir, Node *fn_form, const char *override_name) {
    int idx = 1;
    Node *name_node = list_nth(fn_form, idx++);
    Node *maybe_doc = list_nth(fn_form, idx);
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
                pt = parse_type_node((TypeEnv *)ir->type_env, list_nth(p, 1));
            }
            if (!pname || !*pname) continue;
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
    if (form->count > idx && list_nth(form, idx) && list_nth(form, idx)->kind == N_LIST &&
        is_atom(list_nth(list_nth(form, idx), 0), "doc")) {
        idx++;
    }
    params_form = list_nth(form, idx++);
    returns_form = list_nth(form, idx++);
    } else if (is_atom(h, "entry")) {
        int entry_idx = 2;
        name = "weave_main";
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
                else if (p && p->kind == N_LIST) pt = parse_type_node(tenv, list_nth(p, 1));
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
        compile_fn_form(ir, form, "weave_main");
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

void compile_to_llvm_ir(Node *top, StrBuf *out) {
    int i;
    IrCtx ir;
    StrBuf funcs;
    FnTable fns;
    TypeEnv tenv;

    sb_init(&funcs);
    ir_init(&ir, &funcs);
    fn_table_init(&fns);
    ir.fn_table = &fns;
    type_env_init(&tenv);
    ir.type_env = &tenv;

    {
        Node *decls = top;

        collect_types(&tenv, &ir, decls);
        collect_signatures(&tenv, &fns, decls);

        /* Emit function declarations first (order doesn't matter in LLVM). */
        for (i = 0; decls && i < decls->count; i++) {
            emit_fn_forms_in(&ir, list_nth(decls, i));
        }
    }

    sb_init(out);
    if (ir.typedefs.data && ir.typedefs.len) sb_append_n(out, ir.typedefs.data, ir.typedefs.len);
    if (ir.globals.data && ir.globals.len) sb_append_n(out, ir.globals.data, ir.globals.len);
    if (ir.decls.data && ir.decls.len) sb_append_n(out, ir.decls.data, ir.decls.len);
    if (funcs.data && funcs.len) sb_append_n(out, funcs.data, funcs.len);
}
