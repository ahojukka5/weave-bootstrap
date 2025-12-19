#ifndef WEAVE_BOOTSTRAP_STAGE0_CODEGEN_H
#define WEAVE_BOOTSTRAP_STAGE0_CODEGEN_H

#include "common.h"
#include "sexpr.h"
#include "ir.h"
#include "types.h"
#include "env.h"

typedef struct {
    TypeRef *type;
    int kind; /* 0=const_i32, 1=temp, 2=ssa */
    int const_i32;
    int temp;
    const char *ssa_name;
} Value;

Value value_const_i32(int v);
Value value_temp(TypeRef *t, int temp);
Value value_ssa(TypeRef *t, const char *name);

Value ensure_type_ctx(IrCtx *ir, Value v, TypeRef *target, const char *ctx);
#define ensure_type(ir,v,t) ensure_type_ctx(ir,v,t,NULL)

Value cg_expr(IrCtx *ir, VarEnv *env, Node *expr);
int cg_stmt(IrCtx *ir, VarEnv *env, Node *stmt, TypeRef *ret_type, Value *out_last);

/* Compile top-level forms (program/module) to LLVM IR. */
void compile_to_llvm_ir(Node *top, StrBuf *out);

#endif
