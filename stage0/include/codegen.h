#ifndef WEAVE_BOOTSTRAP_STAGE0_CODEGEN_H
#define WEAVE_BOOTSTRAP_STAGE0_CODEGEN_H

#include "common.h"
#include "sexpr.h"
#include "ir.h"
#include "types.h"
#include "env.h"

/* Operation enums - Julia-style type-safe operation selection */
typedef enum {
    ARITH_ADD,
    ARITH_SUB,
    ARITH_MUL,
    ARITH_DIV
} ArithOp;

typedef enum {
    CMP_EQ,
    CMP_NE,
    CMP_LT,
    CMP_LE,
    CMP_GT,
    CMP_GE
} CmpOp;

typedef enum {
    LOGIC_AND,
    LOGIC_OR
} LogicOp;

/* Enhanced Value representation - Julia-style with metadata */
typedef struct {
    TypeRef *type;
    int kind; /* 0=const_i32, 1=temp, 2=ssa */
    int const_i32;
    int temp;
    const char *ssa_name;
    /* Metadata flags for optimization and type tracking */
    int is_pointer;      /* Is this a pointer type? */
    int is_const;        /* Is this a compile-time constant? */
    int is_boxed;        /* Is this a boxed value? (for future GC support) */
} Value;

Value value_const_i32(int v);
Value value_temp(TypeRef *t, int temp);
Value value_ssa(TypeRef *t, const char *name);

/* Helper to check if a type is a pointer */
int is_pointer_type(TypeRef *t);

/* Arithmetic operation codegen */
Value cg_arith(IrCtx *ir, VarEnv *env, Node *expr, ArithOp op);

/* Comparison operation codegen */
Value cg_cmp(IrCtx *ir, VarEnv *env, Node *expr, CmpOp op);

/* Logical operation codegen */
Value cg_logic(IrCtx *ir, VarEnv *env, Node *expr, LogicOp op);

Value ensure_type_ctx_at(IrCtx *ir, Value v, TypeRef *target, const char *ctx, Node *location);
Value ensure_type_ctx(IrCtx *ir, Value v, TypeRef *target, const char *ctx);
#define ensure_type(ir,v,t) ensure_type_ctx(ir,v,t,NULL)

Value cg_expr(IrCtx *ir, VarEnv *env, Node *expr);
int cg_stmt(IrCtx *ir, VarEnv *env, Node *stmt, TypeRef *ret_type, Value *out_last);

/* Compile top-level forms (program/module) to LLVM IR. */
void compile_to_llvm_ir(Node *top, StrBuf *out, int generate_tests_mode, StrList *selected_test_names, StrList *selected_tags);

#endif
