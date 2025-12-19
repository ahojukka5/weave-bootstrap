#ifndef WEAVE_BOOTSTRAP_STAGE0_TYPES_H
#define WEAVE_BOOTSTRAP_STAGE0_TYPES_H

#include "common.h"
#include "sexpr.h"

typedef struct TypeRef TypeRef;

typedef enum {
    TY_I32,
    TY_I8PTR,
    TY_VOID,
    TY_STRUCT,
    TY_PTR
} TypeKind;

struct TypeRef {
    TypeKind kind;
    const char *name;   /* for TY_STRUCT */
    TypeRef *pointee;   /* for TY_PTR */
};

TypeRef *type_i32(void);
TypeRef *type_i8ptr(void);
TypeRef *type_void(void);
TypeRef *type_struct(const char *name);
TypeRef *type_ptr(TypeRef *pointee);

int type_eq(TypeRef *a, TypeRef *b);
void emit_llvm_type(StrBuf *out, TypeRef *t);

/* Forward-declared to avoid header cycles. */
struct TypeEnv;

TypeRef *parse_type_node(struct TypeEnv *tenv, Node *n);

#endif

