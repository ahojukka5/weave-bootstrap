# Type System Improvements: Moving Away from Hardcoded Special Forms

## Problem

Currently, special forms like `ptr-add`, `get-field`, `bitcast` are hardcoded in `expr.c` with `if (is_atom(head, "ptr-add"))` checks. This is brittle and doesn't scale.

## Julia's Approach

Julia uses a sophisticated type system with:

1. **Multiple Dispatch**: Functions are defined with type signatures, compiler selects method based on argument types
2. **Builtin Registry**: Builtin functions are registered with their type signatures and code generation strategies
3. **Type Inference**: Rich type system that infers types and selects appropriate implementations
4. **Intrinsics**: Low-level operations are defined as intrinsics that map directly to LLVM IR

## Proposed Solution for Weave-Bootstrap

### 1. Builtin Function Registry

Create a registry of builtin functions with:
- Type signatures (parameter types, return type)
- Code generation strategy (GEP, call, intrinsic, etc.)
- Whether it's a special form (needs special handling)

```c
typedef enum {
    BUILTIN_GEP,      // Generates getelementptr (ptr-add, get-field)
    BUILTIN_CALL,     // Regular function call
    BUILTIN_INTRINSIC, // LLVM intrinsic
    BUILTIN_SPECIAL   // Special form (let, if, etc.)
} BuiltinKind;

typedef struct {
    const char *name;
    BuiltinKind kind;
    TypeRef *ret_type;
    int param_count;
    TypeRef **param_types;
    void (*codegen)(IrCtx *ir, VarEnv *env, Node *expr);
} BuiltinDef;
```

### 2. Register Builtins

```c
static BuiltinDef builtins[] = {
    {
        .name = "ptr-add",
        .kind = BUILTIN_GEP,
        .ret_type = NULL, // Computed from first arg
        .param_count = 3, // type, ptr, idx
        .param_types = NULL, // Flexible
        .codegen = cg_ptr_add
    },
    // ... other builtins
};
```

### 3. Dispatch System

In `cg_expr`, instead of hardcoded checks:

```c
// Check builtin registry first
BuiltinDef *builtin = find_builtin(head_name);
if (builtin) {
    return builtin->codegen(ir, env, expr);
}

// Fall back to function table lookup
// Then to regular function call
```

### 4. Type-Driven Code Generation

For `ptr-add`, the codegen function would:
1. Parse type from first argument
2. Compile pointer and index arguments
3. Generate GEP based on inferred types

This makes it extensible - new builtins just need to be registered.

## Benefits

1. **Extensibility**: Add new builtins without modifying core compiler code
2. **Type Safety**: Type signatures ensure correct usage
3. **Maintainability**: All builtin definitions in one place
4. **Testability**: Can test builtin registry independently

## Migration Path (Incremental Approach)

### Phase 1: Registry for Identification (Current)
1. ✅ Create builtin registry system (`builtins.h`/`builtins.c`)
2. ✅ Register builtin names and their kinds
3. Update `cg_expr` to check registry first, then fall back to hardcoded checks
4. This allows us to identify builtins without breaking existing code

### Phase 2: Move Implementations
1. Extract builtin implementations to separate functions
2. Register function pointers in builtin registry
3. Update `cg_expr` to dispatch through registry
4. Remove hardcoded checks

### Phase 3: Type-Driven Dispatch
1. Add type signatures to builtin registry
2. Implement type checking/inference for builtins
3. Support polymorphic builtins (e.g., `ptr-add` works with any type)

## Current Status

I've created the initial registry system. The next step is to update `expr.c` to check the registry before hardcoded checks, making it easy to gradually migrate builtins.

## Example: ptr-add Implementation

```c
static Value cg_ptr_add(IrCtx *ir, VarEnv *env, Node *expr) {
    TypeEnv *tenv = (TypeEnv *)ir->type_env;
    TypeRef *elem_ty = parse_type_node(tenv, list_nth(expr, 1));
    Value ptr = cg_expr(ir, env, list_nth(expr, 2));
    Value idx = ensure_type_ctx_at(ir, cg_expr(ir, env, list_nth(expr, 3)), 
                                   type_i32(), "ptr-add index", expr);
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
    return value_temp(type_ptr(elem_ty), t);
}
```

This function is registered, not hardcoded in the main expression compiler.

