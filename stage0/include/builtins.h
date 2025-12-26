#ifndef WEAVE_BOOTSTRAP_STAGE0_BUILTINS_H
#define WEAVE_BOOTSTRAP_STAGE0_BUILTINS_H

#include "codegen.h"
#include "types.h"

/* Builtin IDs - Julia-style enum for all builtins */
typedef enum {
    BUILTIN_ID_PTR_ADD,
    BUILTIN_ID_GET_FIELD,
    BUILTIN_ID_BITCAST,
    BUILTIN_ID_NONE  /* Sentinel */
} BuiltinId;

/* Builtin function code generation strategies */
typedef enum {
    BUILTIN_GEP,      /* Generates getelementptr (ptr-add, get-field) */
    BUILTIN_CALL,     /* Regular function call */
    BUILTIN_INTRINSIC, /* LLVM intrinsic */
    BUILTIN_SPECIAL   /* Special form (let, if, return, etc.) */
} BuiltinKind;

/* Builtin function definition */
typedef struct {
    const char *name;
    BuiltinKind kind;
    /* Type signature - NULL means computed from arguments */
    TypeRef *ret_type;  /* NULL = computed, or specific type */
    int param_count;    /* -1 = variable, or fixed count */
    TypeRef **param_types; /* NULL = flexible types */
    /* Code generation function */
    Value (*codegen)(IrCtx *ir, VarEnv *env, Node *expr);
} BuiltinDef;

/* Find a builtin by name */
BuiltinDef *find_builtin(const char *name);

/* Get builtin ID by name */
BuiltinId builtin_id(const char *name);

/* Central dispatch function - Julia-style switch statement */
Value cg_builtin(IrCtx *ir, VarEnv *env, BuiltinId id, Node *expr);

/* Initialize builtin registry */
void builtins_init(void);

/* Check if a name is a builtin */
int is_builtin(const char *name);

/* Get the kind of a builtin */
BuiltinKind builtin_kind(const char *name);

#endif

