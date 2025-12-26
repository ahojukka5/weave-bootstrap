# Analysis: How Julia Handles Intrinsics/Builtins

## Summary

I cloned the Julia repository and analyzed how they handle intrinsics. Here's what I found:

## Julia's Architecture

### 1. Macro-Based Intrinsic Registry

Julia uses a **macro-based system** in `src/intrinsics.h`:

```c
#define INTRINSICS \
    ADD_I(bitcast, 2) \
    ADD_I(add_int, 2) \
    ADD_I(sub_int, 2) \
    /* ... many more ... */ \
    ADD_I(add_ptr, 2) \
    ADD_I(sub_ptr, 2) \
    ADD_I(pointerref, 3) \
    ADD_I(pointerset, 4)

enum intrinsic {
#define ADD_I(func, nargs) func,
    INTRINSICS
#undef ADD_I
    num_intrinsics
};
```

**Key insight**: All intrinsics are defined in ONE place using macros, which generates:
- An enum of intrinsic IDs
- Function signature tables
- Runtime function tables

### 2. Switch-Based Dispatch

In `src/intrinsics.cpp`, there's a **single large switch statement** in `emit_intrinsic()`:

```cpp
static jl_cgval_t emit_intrinsic(jl_codectx_t &ctx, intrinsic f, 
                                 jl_value_t **args, size_t nargs) {
    // ... validate arguments ...
    
    switch (f) {
    case pointerref:
        return emit_pointerref(ctx, argv);
    case pointerset:
        return emit_pointerset(ctx, argv);
    case add_ptr:
    case sub_ptr:
        return emit_pointerarith(ctx, f, argv);
    // ... many more cases ...
    }
}
```

**Key insight**: One central dispatch point, not scattered `if` checks.

### 3. Pointer Arithmetic Implementation

For `add_ptr`/`sub_ptr`, Julia's `emit_pointerarith()` function:

```cpp
static jl_cgval_t emit_pointerarith(jl_codectx_t &ctx, intrinsic f,
                                    ArrayRef<jl_cgval_t> argv)
{
    // Type checking
    jl_value_t *ptrtyp = argv[0].typ;
    jl_value_t *offtyp = argv[1].typ;
    if (!jl_is_cpointer_type(ptrtyp) || offtyp != (jl_value_t *)jl_ulong_type)
        return emit_runtime_call(ctx, f, argv, argv.size());
    
    // Extract values
    Value *ptr = emit_unbox(ctx, ctx.types().T_ptr, argv[0]);
    Value *off = emit_unbox(ctx, ctx.types().T_size, argv[1]);
    
    // Handle subtraction
    if (f == sub_ptr)
        off = ctx.builder.CreateNeg(off);
    
    // Generate GEP!
    Value *ans = ctx.builder.CreateGEP(
        getInt8Ty(ctx.builder.getContext()), ptr, off);
    
    // Return typed value
    return mark_julia_type(ctx, ans, false, ptrtyp);
}
```

**Key insights**:
1. **Type-driven**: Checks types first, falls back to runtime if types don't match
2. **Direct LLVM API**: Uses `CreateGEP()` directly (they use LLVM C++ API)
3. **Always uses `i8*`**: They GEP with `i8` element type, then cast if needed
4. **Single function**: One function handles both `add_ptr` and `sub_ptr`

### 4. Comparison to Our Approach

| Aspect | Julia | Our Current Approach |
|--------|-------|---------------------|
| **Registry** | Macro-based enum + switch | Hardcoded `if` checks |
| **Dispatch** | Single switch statement | Scattered `if` checks in `expr.c` |
| **Type Checking** | Type-driven, runtime fallback | Compile-time only |
| **Code Generation** | Direct LLVM C++ API | String-based IR generation |
| **Extensibility** | Add to macro, add case | Modify multiple files |

## What We Should Adopt

### 1. Central Registry

Instead of:
```c
if (is_atom(head, "ptr-add")) { ... }
if (is_atom(head, "get-field")) { ... }
```

We should have:
```c
typedef enum {
    BUILTIN_PTR_ADD,
    BUILTIN_GET_FIELD,
    // ...
} BuiltinId;

BuiltinId find_builtin(const char *name);
```

### 2. Single Dispatch Point

Instead of scattered checks, one function:
```c
Value cg_builtin(IrCtx *ir, VarEnv *env, BuiltinId id, Node *expr) {
    switch (id) {
    case BUILTIN_PTR_ADD:
        return cg_ptr_add(ir, env, expr);
    case BUILTIN_GET_FIELD:
        return cg_get_field(ir, env, expr);
    // ...
    }
}
```

### 3. Type-Driven Code Generation

Like Julia, check types first:
```c
static Value cg_ptr_add(IrCtx *ir, VarEnv *env, Node *expr) {
    // Parse type
    TypeRef *elem_ty = parse_type_node(tenv, list_nth(expr, 1));
    
    // Compile arguments
    Value ptr = cg_expr(ir, env, list_nth(expr, 2));
    Value idx = cg_expr(ir, env, list_nth(expr, 3));
    
    // Type check
    if (!is_pointer_type(ptr.type)) {
        // Fall back to runtime or error
    }
    
    // Generate GEP
    // ...
}
```

## Key Takeaways

1. **One registry, one dispatch point**: All builtins in one place
2. **Type-driven**: Types determine code generation strategy
3. **Extensible**: Adding new builtins is just adding to registry + switch case
4. **Clean separation**: Builtin logic separate from general expression compilation

## Next Steps for Weave-Bootstrap

1. Create `builtins.h` with enum and registry
2. Create `builtins.c` with dispatch function
3. Move all builtin implementations to separate functions
4. Update `expr.c` to use registry instead of hardcoded checks
5. Gradually migrate all special forms to this system

This matches Julia's approach and will make our compiler much more maintainable.

