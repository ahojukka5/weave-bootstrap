# Proposed High-Impact Pull Requests

This document outlines 3 high-impact pull requests that would significantly improve the Weave bootstrap compiler's correctness and test pass rate.

## Current Status
- **Test Pass Rate**: 402/444 tests passing (91%)
- **Failing Tests**: 42 tests (9 in stage1, 22 in stage2, plus embedded test failures)
- **Main Issues**: IR generation bugs, type system issues, invalid LLVM type names

---

## PR #1: Fix Invalid LLVM Type Generation for Unknown Types

### Impact: 🔥 HIGH
- **Estimated Tests Fixed**: ~15-20 tests (array operations, struct operations, embedded tests)
- **Root Cause**: `map-type-node` incorrectly treats non-type AST nodes (like "program") as struct types
- **Current Behavior**: Unknown type names get prefixed with `%` and emitted as LLVM struct types, causing invalid IR like `%program` or `program` (without %)

### Problem Analysis

The issue occurs in `stage1/src/ir/types.weave` in the `map-type-node` function:

```29:33:stage1/src/ir/types.weave
          ;; Unknown atom - treat as user-defined struct type, prefix with %
          (let b Buffer (buffer-new))
          (buffer-append-string b "%")
          (buffer-append-string b (sanitize-name name))
          (return (buffer-to-string b))
```

When type inference encounters an AST node that isn't a recognized type (e.g., a "program" node or other non-type identifier), it incorrectly assumes it's a struct type. This generates invalid LLVM types like:
- `program` (should be a proper type or error)
- `%program` (invalid struct type)
- Similar issues with other non-type identifiers

### Solution

1. **Add type validation**: Before treating an unknown atom as a struct type, verify it's actually a declared struct type by checking against `struct-names` array
2. **Improve error handling**: When an unknown type is encountered that isn't a struct, emit a proper error or fallback to a safe default (e.g., `i32` for integers, `i8*` for pointers)
3. **Fix type inference**: Ensure `infer-arg-type` and related functions don't pass non-type nodes to `map-type-node`

### Implementation Plan

1. **Modify `map-type-node`** in `stage1/src/ir/types.weave`:
   - Add `struct-names` parameter to validate struct types
   - Check if unknown atom is in `struct-names` before treating as struct
   - Add fallback logic for truly unknown types

2. **Update call sites** of `map-type-node`:
   - Pass `struct-names` array to all calls
   - Files: `stage1/src/ir/expr_ccall.weave`, `stage1/src/ir/pointers.weave`, `stage1/src/ir/types.weave`, etc.

3. **Fix `infer-arg-type`** in `stage1/src/ir/core/expr_core_types.weave`:
   - Ensure it only passes valid type nodes to `map-type-node`
   - Handle cases where expression type cannot be inferred

4. **Add tests**:
   - Test unknown type handling
   - Test that "program" and other non-type identifiers don't generate invalid types
   - Test struct type validation

### Expected Outcome
- Fixes array operation tests (`test_array_contains_42`, `test_array_copy_42`, etc.)
- Fixes struct operation tests (`test_struct_like_42`, etc.)
- Fixes embedded test failures with invalid LLVM identifiers
- Improves overall IR generation correctness

### Files to Modify
- `stage1/src/ir/types.weave` (primary fix)
- `stage1/src/ir/core/expr_core_types.weave` (type inference)
- `stage1/src/ir/expr_ccall.weave` (ccall type handling)
- `stage1/src/ir/pointers.weave` (pointer type handling)
- All other files that call `map-type-node`

---

## PR #2: Fix C Function Call (ccall) Type Generation

### Impact: 🔥 HIGH
- **Estimated Tests Fixed**: ~10 tests (all `test_ccall_*` tests in stage1 and stage2)
- **Root Cause**: `compile-ccall` generates invalid return types for C library functions, producing types like `program` instead of proper LLVM types
- **Current Behavior**: C function declarations use incorrect return types, causing LLVM validation errors

### Problem Analysis

The issue occurs in `stage1/src/ir/expr_ccall.weave` in the `compile-ccall` function:

```84:88:stage1/src/ir/expr_ccall.weave
              (if-stmt (== ret-ty-node (- 0 1))
                (buffer-append-string decls "i32")
                (emit-type-node decls a ret-ty-node)
              )
```

When `ret-ty-node` is not `-1` (meaning a return type was specified), `emit-type-node` is called. However, if the type node is malformed or represents a non-type (like "program"), it generates invalid LLVM types.

Additionally, the function doesn't properly handle common C library function types:
- `abs`, `atoi` should return `i32`
- `exit` should return `void`
- `malloc` should return `i8*` (already handled)
- `memcpy` should return `i8*`

### Solution

1. **Add C library function type registry**: Create a mapping of common C library functions to their correct LLVM types
2. **Validate return type nodes**: Before calling `emit-type-node`, verify the type node is valid
3. **Improve type node parsing**: Ensure return type nodes from `(returns TYPE)` are properly extracted and validated
4. **Add fallback handling**: If type cannot be determined, use safe defaults based on function name

### Implementation Plan

1. **Create C library function type mapping** in `stage1/src/ir/expr_ccall.weave`:
   ```weave
   (fn get-c-function-ret-type
     (params (name String)) (returns String)
     (body (do
       (if-stmt (string-eq name "abs") (return "i32") (do 0))
       (if-stmt (string-eq name "atoi") (return "i32") (do 0))
       (if-stmt (string-eq name "exit") (return "void") (do 0))
       (if-stmt (string-eq name "malloc") (return "i8*") (do 0))
       (if-stmt (string-eq name "memcpy") (return "i8*") (do 0))
       ;; ... more functions
       (return "")
     ))
   )
   ```

2. **Modify `compile-ccall`**:
   - Check C library function registry first
   - Validate `ret-ty-node` before calling `emit-type-node`
   - Use registry result if available, otherwise use parsed type
   - Add error handling for invalid types

3. **Fix type node extraction**:
   - Ensure `ret-ty-node` correctly extracts the type from `(returns TYPE)` form
   - Handle edge cases where type node might be malformed

4. **Add tests**:
   - Test all C library functions with correct types
   - Test invalid type node handling
   - Test fallback behavior

### Expected Outcome
- Fixes all `test_ccall_abs_42`, `test_ccall_atoi_42`, `test_ccall_exit_42`, `test_ccall_malloc_42`, `test_ccall_memcpy_42` tests
- Ensures C function declarations are valid LLVM IR
- Improves FFI reliability

### Files to Modify
- `stage1/src/ir/expr_ccall.weave` (primary fix)
- `stage1/src/ir/types.weave` (if type validation needed)
- Test files for ccall functionality

---

## PR #3: Fix get-field Implementation to Match Stage0 Behavior

### Impact: 🔥 HIGH
- **Estimated Tests Fixed**: ~5-7 tests (pointer operations, struct field access, self-hosting issues)
- **Root Cause**: Stage1's `get-field` implementation doesn't match stage0's correct behavior, causing type mismatches and self-compilation failures
- **Current Behavior**: get-field generates incorrect LLVM (missing loads, wrong pointer types)

### Problem Analysis

From `docs/sessions/2024-12-22-get-field-syntax-and-weavec2-bootstrap-issue.md`:

- **Stage0 (C compiler)**: Correctly implements new 2-arg syntax `(get-field ptr field-name)`
  - Compiles base expression first, gets Value with type
  - Uses result type to find struct definition
  - Generates correct GEP + load sequence

- **Stage1 (Weave compiler)**: Has buggy implementation
  - Attempts to infer struct name from AST node before compilation
  - Generates wrong LLVM: `%t0 = load %v0, %v0* %Arena` (loading from type name)
  - Should generate: `%t1 = load %Arena*, %Arena** %a` (loading from variable)
  - Missing load instruction after GEP, causing `i8**` vs `i8*` type mismatches

### Solution

1. **Align with stage0 implementation**: Study `stage0/src/expr.c` `cg_get_field()` function and replicate the logic in Weave
2. **Fix compilation order**: Compile base expression first, then use result type for struct lookup
3. **Fix GEP + load sequence**: Ensure proper load instruction is emitted after GEP
4. **Remove legacy syntax support**: Clean up dual-syntax support, use only new 2-arg form

### Implementation Plan

1. **Study stage0 implementation** (`stage0/src/expr.c:239`):
   - Understand how it compiles base expression
   - Understand how it infers struct type from result
   - Understand GEP + load sequence

2. **Fix `compile-get-field-node`** in `stage1/src/ir/pointers.weave`:
   - Compile base expression first (like stage0)
   - Get result type from compiled expression
   - Use result type to find struct definition
   - Generate correct GEP with proper indices
   - Emit load instruction after GEP (if field type requires it)

3. **Fix type inference** in `stage1/src/ir/core/expr_core_types.weave`:
   - Update `infer-arg-type` to correctly handle get-field
   - Return proper field type, not just `i8*`

4. **Remove legacy syntax**:
   - Delete 4-arg form handling from `expr_compile.weave`
   - Update `is-pointer-expr` to remove child count checks
   - Simplify code to only support new 2-arg syntax

5. **Add tests**:
   - Test get-field with various struct types
   - Test nested struct field access
   - Test pointer field access
   - Test self-compilation with get-field

### Expected Outcome
- Fixes `test_ptr_add_42` and related pointer/struct tests
- Enables successful self-compilation (weavec1 → weavec2)
- Aligns stage1 behavior with stage0's proven correct implementation
- Removes technical debt from dual-syntax support

### Files to Modify
- `stage1/src/ir/pointers.weave` (primary fix - `compile-get-field-node`)
- `stage1/src/ir/core/expr_core_types.weave` (type inference)
- `stage1/src/ir/core/expr_compile.weave` (remove legacy syntax)
- `stage1/src/ir/core/expr_core_ptrs.weave` (update `is-pointer-expr`)

### Reference Implementation
- `stage0/src/expr.c:239` - `cg_get_field()` function (correct implementation)

---

## Implementation Priority

1. **PR #1** (Type Generation) - Highest priority
   - Blocks many tests
   - Foundation for other fixes
   - Relatively straightforward

2. **PR #2** (ccall Types) - High priority
   - Clear, isolated issue
   - Well-defined solution
   - Quick win

3. **PR #3** (get-field) - High priority
   - Critical for self-hosting
   - More complex but well-documented
   - Has reference implementation

## Estimated Impact Summary

- **Total Tests Fixed**: ~30-37 tests (out of 42 failing)
- **Test Pass Rate Improvement**: 91% → ~98-99%
- **Self-Hosting**: PR #3 enables full self-compilation
- **Code Quality**: All PRs improve type system correctness and IR generation reliability

## Testing Strategy

For each PR:
1. Run full test suite: `cd build && ctest --output-on-failure`
2. Verify specific failing tests now pass
3. Ensure no regressions (all previously passing tests still pass)
4. For PR #3: Verify self-compilation (weavec1 → weavec2) succeeds

## Notes

- All PRs follow the "No Workarounds Policy" - proper fixes, not workarounds
- PRs can be implemented independently, but PR #1 should be done first as it may affect others
- Each PR includes comprehensive tests to prevent regressions
- All changes maintain backward compatibility with existing working code

