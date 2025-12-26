# ptr-add Compilation Bug Investigation

## Problem
`ptr-add` expressions are being compiled as function calls (`call i32 @ptr-add`) instead of GEP instructions, despite multiple checks added to catch and handle them correctly.

## Generated IR Evidence
```
%t389 = ptrtoint i8* %t388 to i32
%t390 = load i32, i32* %v_idx_2
%t391 = call i32 @ptr-add(i32 0, i32 %t389, i32 %t390)
```

The first argument `i32 0` indicates the type node is being compiled to `0`, and the pointer is being converted to an integer via `ptrtoint` before the call.

## Checks Added (All Not Working)

### 1. Early Check in compile-expr (lines 23-52)
- Checks if node is a list with exactly 3 children (ty, ptr, idx) and no fifth child
- Should compile as `ptr-add` and return early
- **Status**: Not working - execution continues to function call handler

### 2. Check in lookup-fn-ret-type (lines 208-216)
- Returns "" for any 7-character function name starting with "ptr"
- Should trigger ptr-add handling in compile-expr
- **Status**: Not working - function call handler still generates call with "i32" return type

### 3. Check in Function Call Handler (lines 974-983)
- Checks if `ret-type` is empty after calling `lookup-fn-ret-type`
- Should compile as `ptr-add` if empty
- **Status**: Not working - function call is still generated

## Root Cause Hypothesis

The fact that NONE of these checks are working suggests one of:

1. **Checks aren't being reached**: The code path for `ptr-add` might bypass all these checks
2. **String comparison failure**: `string-eq` or character checks might not be working correctly for "ptr-add"
3. **AST structure mismatch**: The actual AST structure might be different than expected
4. **Compiler bug**: There might be a fundamental bug in the Weave compiler preventing these checks from working

## Next Steps

1. Verify the actual AST structure of `ptr-add` expressions
2. Add runtime debugging to see if checks are being reached
3. Check if there's a different code path handling `ptr-add`
4. Consider if the issue is in the parser/type checker rather than the IR generator

## Files Modified

- `stage1/src/ir/core/expr_compile.weave`: Added multiple checks for `ptr-add`
- `stage1/src/ir/ir_util.weave`: Modified `lookup-fn-ret-type` to return "" for `ptr-add`
- `stage1/src/ir/core/expr_core_types.weave`: Added type inference for `ptr-add`

## Current Status

**11 instances** of `call i32 @ptr-add` still present in generated IR. All attempts to fix have failed.

