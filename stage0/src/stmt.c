#include "codegen.h"
#include "type_env.h"

static void emit_i32_value(StrBuf *out, Value v) {
    if (v.kind == 0) sb_printf_i32(out, v.const_i32);
    else if (v.kind == 1) ir_emit_temp(out, v.temp);
    else {
        sb_append(out, "%");
        sb_append(out, v.ssa_name ? v.ssa_name : "");
    }
}

static void emit_value(StrBuf *out, Value v) {
    if (v.kind == 0) sb_printf_i32(out, v.const_i32);
    else if (v.kind == 1) ir_emit_temp(out, v.temp);
    else {
        sb_append(out, "%");
        sb_append(out, v.ssa_name ? v.ssa_name : "");
    }
}

int cg_stmt(IrCtx *ir, VarEnv *env, Node *stmt, TypeRef *ret_type, Value *out_last) {
    Node *head;
    Value none = {0};
    none.type = NULL;
    if (out_last) *out_last = none;

    if (!stmt || stmt->kind != N_LIST) return 0;
    head = list_nth(stmt, 0);
    if (!head || head->kind != N_ATOM) return 0;

    if (is_atom(head, "doc")) {
        return 0;
    }

    if (is_atom(head, "return")) {
        Value v = cg_expr(ir, env, list_nth(stmt, 1));
        if (out_last) *out_last = v;
        sb_append(ir->out, "  ret ");
        emit_llvm_type(ir->out, ret_type);
        sb_append(ir->out, " ");
        emit_value(ir->out, v);
        sb_append(ir->out, "\n");
        return 1;
    }

    if (is_atom(head, "store")) {
        TypeEnv *tenv = (TypeEnv *)ir->type_env;
        TypeRef *ty = parse_type_node(tenv, list_nth(stmt, 1));
        Value ptrv = cg_expr(ir, env, list_nth(stmt, 2));
        Value vv = ensure_type_ctx(ir, cg_expr(ir, env, list_nth(stmt, 3)), ty, "store");
        sb_append(ir->out, "  store ");
        emit_llvm_type(ir->out, ty);
        sb_append(ir->out, " ");
        emit_value(ir->out, vv);
        sb_append(ir->out, ", ");
        emit_llvm_type(ir->out, ptrv.type);
        sb_append(ir->out, " ");
        emit_value(ir->out, ptrv);
        sb_append(ir->out, "\n");
        return 0;
    }

    if (is_atom(head, "set-field")) {
        TypeEnv *tenv = (TypeEnv *)ir->type_env;
        Value base = cg_expr(ir, env, list_nth(stmt, 1));
        const char *fname = atom_text(list_nth(stmt, 2));
        Value vv;
        StructDef *sd;
        TypeRef *sty;
        int fi;
        int pfield;
        if (!base.type) return 0;
        if (base.type->kind == TY_STRUCT) sty = base.type;
        else if (base.type->kind == TY_PTR) sty = base.type->pointee;
        else return 0;
        sd = type_env_find_struct(tenv, sty->name);
        fi = struct_field_index(sd, fname);
        if (fi < 0) return 0;
        vv = ensure_type_ctx(ir, cg_expr(ir, env, list_nth(stmt, 3)), sd->field_types[fi], "set-field");
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
        sb_append(ir->out, "  store ");
        emit_llvm_type(ir->out, sd->field_types[fi]);
        sb_append(ir->out, " ");
        emit_value(ir->out, vv);
        sb_append(ir->out, ", ");
        emit_llvm_type(ir->out, sd->field_types[fi]);
        sb_append(ir->out, "* ");
        ir_emit_temp(ir->out, pfield);
        sb_append(ir->out, "\n");
        return 0;
    }

    if (is_atom(head, "let")) {
        Node *name_node = list_nth(stmt, 1);
        Node *type_node = list_nth(stmt, 2);
        Node *init_node = list_nth(stmt, 3);
        const char *name = atom_text(name_node);
        Value initv = cg_expr(ir, env, init_node);
        TypeEnv *tenv = (TypeEnv *)ir->type_env;
        TypeRef *ty = parse_type_node(tenv, type_node);
        Value nested_last = none;
        const char *ssa = NULL;

        env_add_local(env, name, ty);
        ssa = env_ssa_name(env, name);
        sb_append(ir->out, "  %");
        sb_append(ir->out, ssa ? ssa : name);
        sb_append(ir->out, " = alloca ");
        emit_llvm_type(ir->out, ty);
        sb_append(ir->out, "\n");
        sb_append(ir->out, "  store ");
        emit_llvm_type(ir->out, ty);
        sb_append(ir->out, " ");
        emit_value(ir->out, initv);
        sb_append(ir->out, ", ");
        emit_llvm_type(ir->out, ty);
        sb_append(ir->out, "* %");
        sb_append(ir->out, ssa ? ssa : name);
        sb_append(ir->out, "\n");
        if (stmt->count > 4) {
            int i;
            for (i = 4; i < stmt->count; i++) {
                Value tmp = none;
                if (cg_stmt(ir, env, list_nth(stmt, i), ret_type, &tmp)) return 1;
                if (tmp.type) nested_last = tmp;
            }
        }
        if (out_last && nested_last.type) *out_last = nested_last;
        return 0;
    }

    if (is_atom(head, "set")) {
        Node *name_node = list_nth(stmt, 1);
        Node *expr_node = list_nth(stmt, 2);
        const char *name = atom_text(name_node);
        Value v = cg_expr(ir, env, expr_node);
        TypeRef *ty = env_type(env, name);
        const char *ssa = env_ssa_name(env, name);
        sb_append(ir->out, "  store ");
        emit_llvm_type(ir->out, ty);
        sb_append(ir->out, " ");
        emit_value(ir->out, v);
        sb_append(ir->out, ", ");
        emit_llvm_type(ir->out, ty);
        sb_append(ir->out, "* %");
        sb_append(ir->out, ssa ? ssa : name);
        sb_append(ir->out, "\n");
        return 0;
    }

    if (is_atom(head, "do")) {
        int i;
        Value nested_last = none;
        for (i = 1; i < stmt->count; i++) {
            Value tmp = none;
            if (cg_stmt(ir, env, list_nth(stmt, i), ret_type, &tmp)) return 1;
            if (tmp.type) nested_last = tmp;
        }
        if (out_last && nested_last.type) *out_last = nested_last;
        return 0;
    }

    if (is_atom(head, "if-stmt")) {
        Node *cond = list_nth(stmt, 1);
        Node *then_s = list_nth(stmt, 2);
        Node *else_s = list_nth(stmt, 3);
        Value cv = ensure_type_ctx(ir, cg_expr(ir, env, cond), type_i32(), "if-cond");
        int tcond = ir_fresh_temp(ir);
        int then_l = ir_fresh_label(ir);
        int else_l = ir_fresh_label(ir);
        int end_l = ir_fresh_label(ir);
        int then_ret, else_ret;

        sb_append(ir->out, "  ");
        ir_emit_temp(ir->out, tcond);
        sb_append(ir->out, " = icmp ne i32 ");
        emit_i32_value(ir->out, cv);
        sb_append(ir->out, ", 0\n");
        sb_append(ir->out, "  br i1 ");
        ir_emit_temp(ir->out, tcond);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, then_l);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, else_l);
        sb_append(ir->out, "\n");

        ir_emit_label_def(ir->out, then_l);
        then_ret = cg_stmt(ir, env, then_s, ret_type, NULL);
        if (!then_ret) {
            sb_append(ir->out, "  br label ");
            ir_emit_label_ref(ir->out, end_l);
            sb_append(ir->out, "\n");
        }

        ir_emit_label_def(ir->out, else_l);
        else_ret = cg_stmt(ir, env, else_s, ret_type, NULL);
        if (!else_ret) {
            sb_append(ir->out, "  br label ");
            ir_emit_label_ref(ir->out, end_l);
            sb_append(ir->out, "\n");
        }

        if (then_ret && else_ret) return 1;
        ir_emit_label_def(ir->out, end_l);
        return 0;
    }

    if (is_atom(head, "while")) {
        Node *cond = list_nth(stmt, 1);
        Node *body = list_nth(stmt, 2);
        int cond_l = ir_fresh_label(ir);
        int body_l = ir_fresh_label(ir);
        int end_l = ir_fresh_label(ir);
        int tcond;

        sb_append(ir->out, "  br label ");
        ir_emit_label_ref(ir->out, cond_l);
        sb_append(ir->out, "\n");

        ir_emit_label_def(ir->out, cond_l);
        {
            Value cv = ensure_type_ctx(ir, cg_expr(ir, env, cond), type_i32(), "while-cond");
            tcond = ir_fresh_temp(ir);
            sb_append(ir->out, "  ");
            ir_emit_temp(ir->out, tcond);
            sb_append(ir->out, " = icmp ne i32 ");
            emit_i32_value(ir->out, cv);
            sb_append(ir->out, ", 0\n");
        }
        sb_append(ir->out, "  br i1 ");
        ir_emit_temp(ir->out, tcond);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, body_l);
        sb_append(ir->out, ", label ");
        ir_emit_label_ref(ir->out, end_l);
        sb_append(ir->out, "\n");

        ir_emit_label_def(ir->out, body_l);
        if (!cg_stmt(ir, env, body, ret_type, NULL)) {
            sb_append(ir->out, "  br label ");
            ir_emit_label_ref(ir->out, cond_l);
            sb_append(ir->out, "\n");
        } else {
            /* return in body: still emit end label for validity */
        }

        ir_emit_label_def(ir->out, end_l);
        return 0;
    }

    /* Expression as statement: allow (ccall ...) etc. */
    {
        Value exprv = cg_expr(ir, env, stmt);
        if (out_last) *out_last = exprv;
    }
    return 0;
}
