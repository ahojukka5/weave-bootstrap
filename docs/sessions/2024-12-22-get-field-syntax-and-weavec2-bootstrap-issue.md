# Session 2024-12-22: get-field Syntax and weavec2 Bootstrap Issue

## What Has Been Done

### Investigation
1. **Verified stage2 test status**: All 21 stage2 tests pass when using weavec1 compiler
2. **Identified weavec2 build failure**: Self-hosted compiler (weavec2) fails during compilation with type errors
3. **Root cause analysis**: Discovered dual-syntax support for `get-field` and `set-field`:
   - **Legacy form**: `(get-field (struct T) idx ptr)` - 3 arguments, returns pointer (requires explicit load)
   - **New form**: `(get-field ptr field-name)` - 2 arguments, returns loaded value

### Key Findings
- **stage0 (C compiler)**: Correctly implements new 2-arg syntax in `cg_get_field()` (stage0/src/expr.c:239)
- **stage1 (Weave compiler)**: Has buggy implementation that causes type mismatches during self-compilation
- **Bootstrapping problem**: 
  - weavec0 (stage0 C) can compile stage1 source → produces working weavec1
  - weavec1 cannot self-compile (produce weavec2) due to get-field bugs
- **Error manifestation**: `error: '%t1' defined with type 'i8**' but expected 'i8*'` at array-i32-len call

### Attempted Solutions
1. **Tried to fix child count check**: Changed argc check from 3 to 4, but this caused different error ("loading unsized types")
2. **Attempted new-form-only implementation**: Removed legacy code, but hit same errors
3. **Reverted changes**: Back to original state with i8**/i8* mismatch error

### Technical Details
- **Child counting**: `count-children` in stage1 includes the head node, so:
  - New form `(get-field a kinds)` has 3 total children
  - Legacy form `(get-field T 0 ptr)` has 4 total children
- **Source code uses new syntax**: Files like arena/core_ops.weave use `(get-field a kinds)`
- **Stage0 handles it correctly**: C implementation compiles base expression and infers struct from result type
- **Stage1 has inference bug**: Attempts to infer struct name from AST node before compilation, generates wrong LLVM

## What Has To Be Done

### Immediate Priority: Fix stage1 get-field Implementation
1. **Debug the core issue**: Understand why stage1's `compile-expr` generates wrong LLVM for base expression
   - Generated LLVM shows: `%t0 = load %v0, %v0* %Arena` (wrong - loading from type name)
   - Should generate: `%t1 = load %Arena*, %Arena** %a` (correct - loading from variable)
2. **Compare with stage0**: Study how stage0's C implementation correctly handles this
   - Stage0 compiles base expression first, gets Value with type
   - Stage0 uses result type to find struct definition
   - Stage1 needs similar approach but in Weave

### Implementation Strategy
**Option A: Fix struct name inference** (recommended)
- Ensure `base-node` passed to `compile-expr` is correct (use `second-child`)
- Verify `compile-expr` is actually called and emits LLVM
- Check if struct name inference happens in the right order
- Ensure `ptr-reg` calculation uses correct temp value

**Option B: Mimic stage0 more closely**
- Add type tracking to stage1's IR generation
- Store result types from `compile-expr` like stage0's Value struct
- Use result type for struct lookup instead of AST analysis

### Cleanup After Fix
1. **Remove legacy syntax entirely**:
   - Delete legacy get-field branch (4-arg form) from expr_compile.weave
   - Delete legacy set-field branch from expr_compile.weave
   - Update is-pointer-expr to remove child count check
   - Simplify infer-arg-type get-field handling
2. **Verify all code uses new syntax**: Ensure no legacy calls remain
3. **Test self-hosting**: Confirm weavec1 can build weavec2 successfully

### Long-term Goals
- **Runtime migration**: Once weavec2 builds, implement array/buffer helpers in Weave
- **Eliminate C wrappers**: Use direct ccalls and get-field/set-field for struct access
- **Clean architecture**: One syntax, one implementation, fully bootstrapped

## Error Log

### Current Error (Reverted State)
```
/tmp/weavec1_tmp.ll:988:37: error: '%t1' defined with type 'i8**' but expected 'i8*'
  %t2 = call i32 @array-i32-len(i8* %t1)
                                    ^
```

### Context
```llvm
%a = alloca %Arena*
store %Arena* %p_v_a, %Arena** %a
...
%t1 = getelementptr inbounds %Arena, %Arena* %t0, i32 0, i32 0
%t2 = call i32 @array-i32-len(i8* %t1)
```
Issue: %t1 is result of GEP (pointer to field = i8**), but no load instruction emitted before call.

### Error When argc Check Changed
```
/tmp/weavec1_tmp.ll:986:14: error: loading unsized types is not allowed
  %t0 = load %v0, %v0* %Arena
             ^
```
Issue: Somehow compiling type node instead of variable node.

## Files Modified
- stage1/src/ir/core/expr_compile.weave (reverted)
- stage1/src/ir/core/expr_core_ptrs.weave (reverted)  
- stage1/src/ir/core/expr_core_types.weave (reverted)

## Related Files
- stage0/src/expr.c - Working C implementation of get-field
- stage1/src/arena/core_ops.weave - Uses new get-field syntax
- stage1/src/typecheck/typecheck.weave - Uses new get-field syntax

## Next Session
Start by creating minimal test case that isolates the get-field compilation issue, then systematically debug the stage1 Weave implementation to match stage0's correct behavior.
