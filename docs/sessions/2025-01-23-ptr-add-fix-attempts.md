# ptr-add Compilation Bug - Fix Attempts

## Problem
`ptr-add` expressions are being compiled as function calls (`call i32 @ptr-add(...)`) instead of generating `getelementptr` (GEP) instructions.

## Root Cause
The `ptr-add` special form is not being recognized in `compile-expr`, causing it to fall through to the general function call handler.

## Attempted Fixes

### 1. String Equality Check
Added `string-eq head-val "ptr-add"` check at the top of list expression handling (line 98).
- **Status**: Not working - still generates function calls

### 2. Character-by-Character Check
Added character-by-character comparison checking for "ptr-add" pattern.
- **Status**: Not working - still generates function calls

### 3. Structural Check
Added check using `count-children` to verify 4 children + head length 7.
- **Status**: Not working - still generates function calls

### 4. Multiple Fallback Checks
Added checks in the function call handler to catch `ptr-add` if it reaches that point.
- **Status**: Not working - still generates function calls

### 5. Simplified Check
Simplified to only use `string-eq` as the primary check.
- **Status**: Not working - still generates function calls

## Current State
- Check is at line 98 in `expr_compile.weave`, right after getting `head-val`
- Check should work but doesn't - suggests deeper issue
- 11 instances of `call i32 @ptr-add` still present in generated IR

## Hypothesis
The checks aren't matching, which suggests either:
1. `string-eq` isn't working correctly for "ptr-add"
2. `head-val` contains something other than "ptr-add"
3. The check isn't being reached due to a different code path
4. There's a bug in the Weave compiler itself preventing the checks from working

## Additional Attempts

### 6. Check at Very Beginning of compile-expr
Added check at the very beginning of `compile-expr`, before the atom/list check (line 23).
- **Status**: Not working - still generates function calls

## Current Code State
Multiple checks are in place:
1. Line 23-41: Check at the very beginning (before atom/list check)
2. Line 128-140: Check in the list expression handler
3. Line 955-961: Error check in function call handler
4. Line 966-980: Fallback check in function call handler
5. Line 1010-1015: Final check before function call emission

None of these checks are matching `ptr-add` expressions.

## Next Steps
Requires runtime debugging to:
1. Verify what `head-val` actually contains when processing `ptr-add`
2. Verify if `string-eq` is working correctly
3. Verify if the check is being reached
4. Identify if there's a different code path handling `ptr-add`
5. Check if `ptr-add` is being transformed or normalized before reaching `compile-expr`

