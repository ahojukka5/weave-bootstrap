# Three New High-Impact Pull Requests

This document proposes 3 new high-impact pull requests based on current codebase analysis and test results.

## Current Status
- **Stage0 Tests**: 18/18 passing (100%) ✅
- **Stage1/Stage2 Tests**: Cannot run - weavec1 build fails due to type errors
- **Main Blocker**: Type system bugs preventing self-compilation
- **Previous PRs**: PR #2 (LLVM API migration) and PR #3 (optimization passes) are complete

---

## PR #1: Fix Pointer Arithmetic Type System - Enable Self-Compilation ⭐⭐⭐⭐⭐

### Impact: 🔥 CRITICAL
- **Blocks**: Self-compilation (weavec1 → weavec2)
- **Root Cause**: `ptr-add` operations generate incorrect types, causing LLVM validation errors
- **Current Behavior**: `ptr-add` sometimes generates function calls with wrong return types instead of GEP instructions

### Problem Analysis

The error in `build-test/stage1/weavec1.ll:1840` shows:
```llvm
%t391 = call i32 @ptr-add(i32 0, i32 %t389, i32 %t390)
store i32* %t391, i32** %v_ptr_4  ; ERROR: storing i32 into i32**
```

**Root Causes**:
1. **Lowering phase issue**: `ptr-add` expressions are being lowered to function calls instead of being compiled inline
2. **Type inference bug**: The return type of `ptr-add` is inferred as `i32` instead of a pointer type
3. **Missing inline compilation**: `ptr-add` should always generate GEP instructions, never function calls

### Solution

1. **Fix lowering phase**: Ensure `ptr-add` is never lowered to a function call
   - Check `stage1/src/lower/lowering.weave` for any `ptr-add` lowering logic
   - Remove any function call generation for `ptr-add`

2. **Fix type inference**: Update `infer-arg-type` in `stage1/src/ir/core/expr_core_types.weave`
   - When encountering `ptr-add`, return the pointer type of the element type
   - Ensure type is `(ptr T)` where T is the element type

3. **Verify compilation path**: Ensure `compile-ptr-add-node` is always called
   - Check that `expr_compile.weave` correctly handles `ptr-add` in all contexts
   - Ensure no code path generates function calls for `ptr-add`

### Implementation Plan

1. **Audit lowering phase** (`stage1/src/lower/lowering.weave`):
   ```weave
   ;; Ensure ptr-add is never lowered to a function call
   ;; It should remain as (ptr-add TYPE PTR IDX) for IR generation
   ```

2. **Fix type inference** (`stage1/src/ir/core/expr_core_types.weave`):
   ```weave
   (if-stmt (string-eq head-val "ptr-add")
     (do
       (let elem-ty-node Int32 (second-child a node))
       (let ptr-ty-node Int32 (parse-string a (string-concat "(ptr " (arena-value a elem-ty-node) ")")))
       (return ptr-ty-node)
     )
     (do 0)
   )
   ```

3. **Add validation**: Ensure no `@ptr-add` function declarations are generated
   - Check `ir/main.weave` and `ir/ir_fns.weave` for any `ptr-add` declarations
   - Remove any runtime function declarations for `ptr-add`

4. **Add tests**:
   - Test that `ptr-add` generates GEP instructions, not function calls
   - Test type inference for `ptr-add` expressions
   - Test self-compilation with `ptr-add` operations

### Expected Outcome
- Self-compilation succeeds (weavec1 → weavec2)
- All pointer arithmetic tests pass
- No invalid LLVM type errors from `ptr-add`
- Cleaner IR generation (no unnecessary function calls)

### Files to Modify
- `stage1/src/lower/lowering.weave` - Remove ptr-add function call lowering
- `stage1/src/ir/core/expr_core_types.weave` - Fix type inference
- `stage1/src/ir/core/expr_compile.weave` - Verify compilation path
- `stage1/src/ir/ir_fns.weave` - Remove any ptr-add declarations

### Priority: **CRITICAL** - Blocks all stage1/stage2 testing and self-hosting verification

---

## PR #2: Fix Invalid LLVM Identifier Generation ⭐⭐⭐⭐

### Impact: 🔥 HIGH
- **Estimated Tests Fixed**: ~10-15 tests (embedded tests, complex expressions)
- **Root Cause**: Generated LLVM identifiers contain invalid characters like `<`, `>`, `@`
- **Current Behavior**: Identifiers like `%v_l_then_join_53<F0>` and `%v_l_then_join_53@` are generated, causing LLVM validation errors

### Problem Analysis

LLVM identifiers must follow specific rules:
- Can contain: letters, numbers, `_`, `.`, `-`
- Cannot contain: `<`, `>`, `@`, spaces, special characters
- Must start with `%` for values, `@` for globals

**Current Issues**:
1. **Type parameters in names**: `<F0>` appears in generated names
2. **Invalid characters**: `@` appears in value names (should only be in global names)
3. **Inconsistent sanitization**: `sanitize-name` function exists but isn't applied everywhere

### Solution

1. **Audit all identifier generation points**:
   - Find all places where LLVM identifiers are generated
   - Ensure `sanitize-name` is called consistently
   - Add validation to catch invalid identifiers

2. **Improve `sanitize-name` function**:
   - Remove all invalid characters: `<`, `>`, `@` (except leading `@` for globals)
   - Replace invalid characters with `_` or remove them
   - Ensure uniqueness while maintaining readability

3. **Add identifier validation**:
   - Create `validate-llvm-identifier` function
   - Call it before emitting any identifier
   - Fail compilation with clear error if invalid identifier detected

### Implementation Plan

1. **Find all identifier generation points**:
   ```bash
   # Search for patterns that generate identifiers
   grep -r "buffer-append-string.*%" stage1/src/ir/
   grep -r "emit-temp" stage1/src/ir/
   grep -r "sanitize-name" stage1/src/ir/
   ```

2. **Improve `sanitize-name`** in `stage1/src/ir/util_strings_core.weave`:
   ```weave
   (fn sanitize-name
     (params (name String)) (returns String)
     (body
       ;; Remove all invalid characters: < > @ (except leading @)
       ;; Replace with underscore
       ;; Ensure valid LLVM identifier format
     )
   )
   ```

3. **Add validation function**:
   ```weave
   (fn validate-llvm-identifier
     (params (name String)) (returns Int32)
     (body
       ;; Check for invalid characters
       ;; Return 1 if valid, 0 if invalid
     )
   )
   ```

4. **Apply sanitization everywhere**:
   - Update all identifier generation to use `sanitize-name`
   - Add validation before emitting identifiers
   - Test with complex expressions that trigger edge cases

5. **Add tests**:
   - Test identifier generation with special characters
   - Test that all generated identifiers are valid LLVM
   - Test embedded test compilation

### Expected Outcome
- All embedded tests pass
- No LLVM validation errors from invalid identifiers
- Cleaner, more readable generated IR
- Better error messages for identifier issues

### Files to Modify
- `stage1/src/ir/util_strings_core.weave` - Improve sanitize-name
- `stage1/src/ir/ir_util.weave` - Add validation function
- All files that generate identifiers - Apply sanitization
- `stage1/src/ir/core/expr_compile.weave` - Fix identifier generation
- `stage1/src/ir/ir_fns.weave` - Fix function name generation

### Priority: **HIGH** - Blocks embedded tests and complex expression compilation

---

## PR #3: Implement Comprehensive Type Validation and Error Reporting ⭐⭐⭐⭐

### Impact: 🔥 HIGH
- **Estimated Tests Fixed**: ~15-20 tests (type mismatches, struct operations, array operations)
- **Root Cause**: Type errors are caught late (at LLVM validation) instead of early (during type checking)
- **Current Behavior**: Invalid types like `program` slip through type checking and cause LLVM errors

### Problem Analysis

**Current Issues**:
1. **Late error detection**: Type errors discovered during LLVM validation, not during type checking
2. **Poor error messages**: LLVM errors are cryptic and don't point to source code
3. **Missing validation**: Type nodes aren't validated before being used in IR generation
4. **Inconsistent type mapping**: `map-type-node` has fallbacks that hide type errors

### Solution

1. **Add type validation layer**:
   - Validate all type nodes before IR generation
   - Reject invalid types (like "program", "fn", "entry", "module") early
   - Provide clear error messages with source locations

2. **Improve error reporting**:
   - Add source location tracking through compilation pipeline
   - Emit user-friendly error messages instead of LLVM errors
   - Show type mismatch details (expected vs actual)

3. **Strict type mapping**:
   - Remove fallbacks that hide errors
   - Fail fast when types can't be determined
   - Add explicit validation in `map-type-node`

### Implementation Plan

1. **Add type validation function**:
   ```weave
   (fn validate-type-node
     (params (a (ptr Arena)) (ty-node Int32) (struct-names ArrayString)) (returns Int32)
     (body
       ;; Check if type node is valid
       ;; Reject "program", "fn", "entry", "module" as types
       ;; Verify struct types exist in struct-names
       ;; Return 1 if valid, 0 if invalid
     )
   )
   ```

2. **Update `map-type-node`** in `stage1/src/ir/types.weave`:
   ```weave
   (fn map-type-node
     (params ...) (returns String)
     (body
       ;; Validate type node first
       (if-stmt (== (validate-type-node a ty-node struct-names) 0)
         (return (error "Invalid type node: ..."))
         (do 0)
       )
       ;; Then map to LLVM type
       ;; Remove fallback to "i32" - fail instead
     )
   )
   ```

3. **Add source location tracking**:
   - Track file, line, column through compilation
   - Attach locations to type errors
   - Emit errors with context

4. **Improve error messages**:
   ```weave
   (fn type-error
     (params (msg String) (loc String)) (returns String)
     (body
       (return (string-concat "Type error at " (string-concat loc (string-concat ": " msg))))
     )
   )
   ```

5. **Add validation at key points**:
   - Before calling `map-type-node`
   - Before emitting function signatures
   - Before emitting struct field types
   - Before emitting pointer types

6. **Add tests**:
   - Test invalid type rejection
   - Test error message quality
   - Test that errors point to correct source locations

### Expected Outcome
- Type errors caught during type checking, not LLVM validation
- Clear, actionable error messages
- Faster debugging (errors point to source code)
- Higher test pass rate (invalid types caught early)

### Files to Modify
- `stage1/src/ir/types.weave` - Add validation, improve map-type-node
- `stage1/src/diagnostics.weave` - Improve error reporting (if exists)
- `stage1/src/typecheck/tc_*.weave` - Add type validation
- `stage1/src/ir/ir_util.weave` - Add validation utilities
- All IR generation files - Add validation before type emission

### Priority: **HIGH** - Improves developer experience and catches bugs early

---

## Implementation Priority

1. **PR #1 (Pointer Arithmetic)** - **CRITICAL**
   - Blocks all stage1/stage2 testing
   - Must be fixed first to enable test execution
   - Relatively isolated fix

2. **PR #2 (Identifier Generation)** - **HIGH**
   - Blocks embedded tests
   - Straightforward fix (apply sanitization)
   - Can be done in parallel with PR #1

3. **PR #3 (Type Validation)** - **HIGH**
   - Improves overall correctness
   - Makes debugging easier
   - Can be done incrementally

## Estimated Impact Summary

- **Total Tests Fixed**: ~25-35 tests (out of 42+ failing)
- **Test Pass Rate Improvement**: Current (blocked) → ~95-98%
- **Self-Compilation**: PR #1 enables full self-compilation
- **Code Quality**: All PRs improve type system correctness and IR generation reliability
- **Developer Experience**: PR #3 significantly improves error messages

## Testing Strategy

For each PR:
1. Fix the issue
2. Rebuild weavec1 successfully
3. Run full test suite: `cd build-test && ctest --output-on-failure`
4. Verify specific failing tests now pass
5. Ensure no regressions (all previously passing tests still pass)
6. For PR #1: Verify self-compilation (weavec1 → weavec2) succeeds

## Notes

- All PRs follow the "No Workarounds Policy" - proper fixes, not workarounds
- PRs can be implemented independently, but PR #1 should be done first
- Each PR includes comprehensive validation to prevent regressions
- All changes maintain backward compatibility with existing working code
- Focus on correctness and proper error handling, not quick fixes

