# Investigation: arena-kind Function Table Type Issue

## Problem
The function table stores `ptr(ptr(Arena))` for `arena-kind`'s first parameter instead of `ptr(Arena)`, causing type mismatch errors.

## Error Message
```
stage1/src/ir/pointers.weave:195:32: error: [type-mismatch]: type mismatch in expression
  note: in function 'compile-get-field-node', context 'fn-arg': wanted ptr(ptr(ct Arena), got <unknown>
```

## What I've Tried

1. **Registered `arena-kind` as built-in function** with correct type `ptr(Arena)`
   - Location: `stage0/src/program.c` lines 1073-1078 and 1167-1172
   - Code: `type_ptr(type_struct("Arena"))` which should create `ptr(struct Arena)`

2. **Added prevention check** to skip collection from source files
   - Location: `stage0/src/program.c` line 297
   - Checks for `"arena-kind"` and `"arena-create"` early in `collect_signature_form`

3. **Fixed memory leak** in `fn_table_add` when overwriting entries
   - Location: `stage0/src/fn_table.c` line 49
   - Frees old `param_types` array before overwriting

4. **Added duplicate registration** before function compilation
   - Ensures correct type is set even if something overwrites it

5. **Verified type creation functions**
   - `type_struct("Arena")` creates `TY_STRUCT` with `name="Arena"`, `pointee=NULL`
   - `type_ptr(...)` creates `TY_PTR` with `pointee=the struct type`
   - So `type_ptr(type_struct("Arena"))` should create `TY_PTR -> TY_STRUCT`

## Current State

- The function table still has `ptr(ptr(Arena))` stored
- Error persists despite all fixes
- Registration code looks correct
- Prevention check should work
- Type creation should be correct

## Possible Root Causes

1. **`arena-kind` is being collected from source files** with wrong type `ptr(ptr(Arena))`
   - Prevention check might not be working
   - Or collection happens through different code path

2. **Built-in registration isn't executing** or isn't overwriting correctly
   - But code is unconditional and should execute

3. **Type structure is wrong** despite verification
   - But `type_ptr(type_struct("Arena"))` should create correct structure

4. **Bug in `parse_type_node`** when parsing `(ptr Arena)` from source files
   - Might incorrectly create `ptr(ptr(Arena))` instead of `ptr(Arena)`

## Next Steps

1. **Add debug output** to trace:
   - When `arena-kind` is collected (if at all)
   - What type is stored during collection
   - What type is stored during built-in registration
   - What type is retrieved during function call

2. **Verify actual TypeRef structure** after creation
   - Check if `type_ptr(type_struct("Arena"))` actually creates correct structure
   - Verify the structure isn't being modified after creation

3. **Check if `parse_type_node` has bug** when parsing `(ptr Arena)`
   - Might be incorrectly wrapping pointer types

4. **Investigate function table storage/retrieval**
   - Verify `fn_table_add` actually overwrites correctly
   - Check if there's any caching or shared structure issues

## Files Modified

- `stage0/src/program.c`: Added built-in registration and prevention check
- `stage0/src/fn_table.c`: Fixed memory leak in overwrite logic

