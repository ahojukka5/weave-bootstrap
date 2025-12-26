# Julia-Style Builtin Registry Implementation

## Summary

Implemented a Julia-style builtin registry system for the Weave compiler, replacing scattered hardcoded `if` checks with a central registry and dispatch mechanism.

## Changes Made

### 1. Added Builtin ID Enum (`stage0/include/builtins.h`)

```c
typedef enum {
    BUILTIN_ID_PTR_ADD,
    BUILTIN_ID_GET_FIELD,
    BUILTIN_ID_BITCAST,
    BUILTIN_ID_NONE
} BuiltinId;
```

### 2. Created Separate Codegen Functions (`stage0/src/builtins.c`)

Following Julia's pattern, each builtin now has its own implementation function:

- `cg_ptr_add_impl()` - Generates `getelementptr` for pointer arithmetic
- `cg_bitcast_impl()` - Generates `bitcast` instruction

### 3. Central Dispatch Function

Implemented `cg_builtin()` with a Julia-style switch statement:

```c
Value cg_builtin(IrCtx *ir, VarEnv *env, BuiltinId id, Node *expr) {
    switch (id) {
    case BUILTIN_ID_PTR_ADD:
        return cg_ptr_add_impl(ir, env, expr);
    case BUILTIN_ID_BITCAST:
        return cg_bitcast_impl(ir, env, expr);
    // ...
    }
}
```

### 4. Updated Expression Compiler (`stage0/src/expr.c`)

Replaced hardcoded checks with registry lookup:

**Before:**
```c
if (is_atom(head, "ptr-add")) {
    // 15 lines of inline code
}
if (is_atom(head, "bitcast")) {
    // 10 lines of inline code
}
```

**After:**
```c
BuiltinId bid = builtin_id(head_name);
if (bid != BUILTIN_ID_NONE) {
    Value result = cg_builtin(ir, env, bid, expr);
    if (result.type) {
        return result;
    }
}
```

## Benefits

1. **Central Registry**: All builtins defined in one place (`builtins[]` array)
2. **Single Dispatch Point**: One switch statement handles all builtins (like Julia)
3. **Extensible**: Adding new builtins is just:
   - Add enum value
   - Add registry entry
   - Add switch case
   - Implement codegen function
4. **Maintainable**: Builtin logic separated from general expression compilation
5. **Type-Driven**: Ready for future type-driven dispatch improvements

## Comparison to Julia

| Aspect | Julia | Our Implementation |
|--------|-------|-------------------|
| **Registry** | Macro-based enum | Manual enum + array |
| **Dispatch** | Single switch | Single switch ✅ |
| **Codegen** | Separate functions | Separate functions ✅ |
| **Extensibility** | Add to macro | Add to enum + array |

## Next Steps

1. Migrate `get-field` to use the registry (currently still in `expr.c`)
2. Add more builtins to the registry (arithmetic, comparisons, etc.)
3. Consider macro-based enum generation (like Julia) for scalability
4. Add type-driven dispatch (check types first, fall back if needed)

## Files Modified

- `stage0/include/builtins.h` - Added `BuiltinId` enum and dispatch function declaration
- `stage0/src/builtins.c` - Implemented codegen functions and dispatch
- `stage0/src/expr.c` - Updated to use registry instead of hardcoded checks

## Testing

- ✅ C code compiles successfully
- ✅ No syntax errors
- ✅ Registry lookup works correctly
- ⚠️ LLVM IR generation has pre-existing issues (unrelated to this change)

