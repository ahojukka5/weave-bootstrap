# ptr-add Compilation Bug Analysis

## Problem Summary

The `ptr-add` special form is being incorrectly compiled as a function call (`call i32 @ptr-add(...)`) instead of generating a `getelementptr` (GEP) instruction. This causes type errors in the generated LLVM IR because:

1. `ptr-add` should return a pointer type (e.g., `i8*`, `i32*`, `%Struct*`)
2. The function call is returning `i32` (as specified in the call instruction)
3. The result is then used where a pointer is expected, causing type mismatches

## Evidence

### Source of the Problem

The issue occurs when compiling `string-char-at` function in `stage1/src/string.weave` at line 145:
```weave
(let ptr (ptr Int8) (ptr-add Int8 s idx))
```

This function is part of the Stage1 compiler itself, so when the compiler compiles itself, it encounters `ptr-add` and should handle it as a special form, but instead compiles it as a function call.

### Generated IR Shows Function Calls

The generated LLVM IR contains multiple instances of:
```llvm
%t389 = ptrtoint i8* %t388 to i32
%t390 = load i32, i32* %v_idx_2
%t391 = call i32 @ptr-add(i32 0, i32 %t389, i32 %t390)
```

This is incorrect because:
- `ptr-add` should generate a GEP instruction, not a function call
- The pointer is being converted to an integer with `ptrtoint` before the call
- The function `@ptr-add` is not defined or declared anywhere in the IR

### Expected Behavior

`ptr-add` should generate code like:
```llvm
%t391 = getelementptr inbounds i8, i8* %t388, i32 %t390
```

### Error Messages

The compilation fails with:
```
error: '%t391' defined with type 'i32' but expected 'ptr'
```

This occurs because the result of `call i32 @ptr-add(...)` is `i32`, but it's being stored into an `i32*` variable.

## Root Cause Analysis

### Checks Are Not Working

Multiple defensive checks have been added to prevent `ptr-add` from being compiled as a function call:

1. **Primary check at line 100-115** in `expr_compile.weave`:
   ```weave
   (if-stmt (string-eq head-val "ptr-add")
     (do
       ;; Handle ptr-add as special form, generate GEP
       ...
     )
   )
   ```

2. **Secondary check at line 921-930** in the function call handler:
   ```weave
   (if-stmt (string-eq head-val "ptr-add")
     (do
       ;; Emit error and return
     )
   )
   ```

3. **Tertiary check at line 956-963** after `lookup-fn-ret-type`:
   ```weave
   (if-stmt (|| (== (string-length ret-type) 0) (string-eq head-val "ptr-add"))
     (do
       ;; Emit error and return
     )
   )
   ```

4. **Final check at line 968-975** before function call generation:
   ```weave
   (if-stmt (string-eq head-val "ptr-add")
     (do
       ;; Emit fatal error and return
     )
   )
   ```

### Why Checks Are Failing

The error message strings are present in the generated IR as constants:
```llvm
@.str854 = private constant [60 x i8] c"  ; CRITICAL ERROR: ptr-add reached function call handler!\0A\00"
@.str859 = private constant [71 x i8] c"  ; CRITICAL ERROR: ptr-add should never reach function call handler!\0A\00"
@.str862 = private constant [65 x i8] c"  ; FATAL ERROR: ptr-add reached function call generation code!\0A\00"
```

However, these strings are **never emitted as comments** in the IR, which means:
- The code that generates these error messages exists
- The `if-stmt` conditions are **not matching**
- `string-eq head-val "ptr-add"` is returning `0` (false) when it should return `1` (true)

### Possible Causes

1. **String comparison issue**: `string-eq` may not be working correctly, or `head-val` may not exactly match `"ptr-add"` (whitespace, encoding, etc.)

2. **Different code path**: `ptr-add` may be compiled through a different code path that bypasses the checks

3. **AST structure issue**: The AST node structure for `ptr-add` may be different than expected, causing `head-val` to be something other than `"ptr-add"`

4. **Timing issue**: The check may be evaluated before `head-val` is properly set, or there may be a scoping issue

## Current Implementation Status

### What Exists

1. **`compile-ptr-add-node` function** in `stage1/src/ir/pointers.weave`:
   - Correctly generates GEP instructions
   - Takes the element type node, pointer register, and index register
   - Returns the result register

2. **Type inference for `ptr-add`** in `stage1/src/ir/core/expr_core_types.weave`:
   - `infer-arg-type` correctly returns a pointer type for `ptr-add` expressions

3. **Function lookup rejection** in `stage1/src/ir/ir_util.weave`:
   - `lookup-fn-ret-type` explicitly returns empty string for `ptr-add`

4. **Multiple defensive checks** in `stage1/src/ir/core/expr_compile.weave`:
   - All checks are in place but not working

### What's Missing

The primary check at line 100-115 is not being executed or not matching. The `ptr-add` expression is falling through to the general function call handler.

## What It Takes to Fix

### Immediate Fix Options

1. **Debug the string comparison**:
   - Add debug output to verify what `head-val` actually contains
   - Verify that `string-eq` is working correctly
   - Check if there are any encoding or whitespace issues

2. **Add explicit debugging**:
   - Emit the actual value of `head-val` in the generated IR to see what it contains
   - This would help identify if the string is different than expected

3. **Try alternative comparison**:
   - Use character-by-character comparison instead of `string-eq`
   - Check string length first, then compare characters

4. **Check AST structure**:
   - Verify that `ptr-add` nodes have the expected structure
   - Ensure `arena-value a head` returns `"ptr-add"` for `ptr-add` nodes

### Long-term Solution

1. **Fix the root cause**: Determine why `string-eq head-val "ptr-add"` is not matching
   - This may require debugging the Weave compiler itself
   - May need to add debug output or tracing

2. **Add comprehensive tests**: Create test cases that verify `ptr-add` generates GEP, not function calls

3. **Improve error handling**: If checks fail, emit more informative error messages that help diagnose the issue

### Investigation Steps

1. **Add debug output** to see what `head-val` actually contains:
   ```weave
   (buffer-append-string out "  ; DEBUG: head-val = '")
   (buffer-append-string out head-val)
   (buffer-append-string out "'\n")
   ```

2. **Check if `ptr-add` is being parsed correctly**:
   - Verify the AST structure for `(ptr-add Type ptr idx)` expressions
   - Ensure the head node has value `"ptr-add"`

3. **Verify `string-eq` function**:
   - Test `string-eq` with known values
   - Check if there are any edge cases or bugs

4. **Check for alternative code paths**:
   - Search for all places where function calls are generated
   - Verify that `ptr-add` can't bypass the checks through a different path

## Impact

- **Current**: Stage1 compiler cannot compile itself due to type errors
- **Tests**: All tests that use `ptr-add` will fail
- **Functionality**: Pointer arithmetic is completely broken

## Priority

**CRITICAL** - This blocks compilation of the Stage1 compiler itself, preventing further development.

## Related Files

- `stage1/src/ir/core/expr_compile.weave` - Main compilation logic
- `stage1/src/ir/pointers.weave` - `compile-ptr-add-node` implementation
- `stage1/src/ir/core/expr_core_types.weave` - Type inference for `ptr-add`
- `stage1/src/ir/ir_util.weave` - Function lookup that rejects `ptr-add`

## Next Steps

### Immediate Actions

1. **Add runtime debug output** using `ccall "puts"` to trace execution:
   ```weave
   (ccall "puts" (returns Int32) (args (String (string-concat "DEBUG: head-val='" (string-concat head-val "'")))))
   (ccall "puts" (returns Int32) (args (String (string-concat "DEBUG: string-eq result=" (int-to-string (string-eq head-val "ptr-add"))))))
   ```
   This will help identify what `head-val` actually contains and whether `string-eq` is working.

2. **Verify `string-eq` function**: Test it with known values to ensure it's working correctly. Check `stage1/src/string.weave` for the implementation.

3. **Check for encoding issues**: Verify that `arena-value` returns exactly `"ptr-add"` with no whitespace, special characters, or encoding issues.

4. **Inspect generated IR more carefully**: Look for any patterns in where `ptr-add` function calls are generated - are they all in the same function? Are they all in similar contexts?

### Root Cause Investigation

The fact that error message strings exist as constants but are never emitted suggests:
- The code paths exist but conditions aren't matching
- `string-eq head-val "ptr-add"` is consistently returning `0` (false)
- This could indicate:
  - A bug in `string-eq` function
  - `head-val` contains something other than `"ptr-add"` (whitespace, encoding, etc.)
  - The check is not being reached due to a different code path

### Latest Attempts

1. **Character-by-character fallback check**: Added a fallback that checks each character of `head-val` against "ptr-add" (ASCII codes: 112, 116, 114, 45, 97, 100, 100) if `string-eq` fails. This also did not work.

2. **Multiple defensive checks**: Added checks at multiple points in the code (lines 102, 921, 956, 968) - none are matching.

3. **Verified source**: Confirmed that `ptr-add` is used in `string-char-at` function, which is part of the Stage1 compiler itself.

### Alternative Approaches

If string comparison continues to fail:

1. **Structural check**: Instead of string comparison, check if:
   - Node has exactly 4 children
   - First child value is "ptr-add" (using character-by-character comparison)
   - Second child is a type node
   - Third and fourth children are expressions

2. **Node kind check**: If `ptr-add` has a unique node kind, use that instead of string comparison

3. **Early return pattern**: Make `ptr-add` handling even more explicit by checking it before any other operations, possibly even before getting `head-val` if possible

### Testing Strategy

Once fixed:
1. Add unit tests that verify `ptr-add` generates GEP, not function calls
2. Add integration tests with various `ptr-add` usage patterns
3. Verify that all existing tests pass
4. Add regression tests to prevent this from happening again

