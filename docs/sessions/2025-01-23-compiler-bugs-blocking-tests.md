# Compiler Bugs Blocking Test Execution

## Summary

While building and running the scientific computing tests, we discovered pre-existing bugs in the stage1 compiler (weavec1) that prevent successful compilation.

## Issues Found

### 1. Invalid Null Pointer Stores ✅ (Fixed properly)

**Error**: `store i32* 0, i32** %v_old_data_5`
**Problem**: LLVM IR doesn't allow storing literal `0` as a pointer value
**Fix**: Modified `stmt_core.weave` to detect when storing literal `0` into a pointer type and emit `null` instead

**Status**: Fixed properly in compiler codegen (no workarounds)

### 2. Invalid Null Pointer Returns ✅ (Fixed properly)

**Error**: `ret i8* 0`
**Problem**: LLVM IR doesn't allow returning literal `0` as a pointer value
**Fix**: Modified `stmt_core.weave` to detect when returning literal `0` for a pointer type and emit `null` instead

**Status**: Fixed properly in compiler codegen (no workarounds)

### 3. Invalid Store Destination ❌ (Needs proper fix)

**Error**: `store i32 %t623, i32 %t621`
**Problem**: Store instruction destination must be a pointer type, not `i32`
**Location**: Generated in `buffer-append-string` or similar functions
**Root Cause**: Compiler is generating stores with non-pointer destinations

**Status**: Needs proper fix in stage1 compiler codegen

## Scientific Computing Tests Status

All 5 scientific computing tests have been created and are ready:
1. ✅ `test_newton_method_42.weave`
2. ✅ `test_trapezoidal_integration_42.weave`
3. ✅ `test_correlation_42.weave`
4. ✅ `test_linear_regression_42.weave`
5. ✅ `test_monte_carlo_pi_42.weave`

**All tests follow the naming convention and will be automatically discovered by CMake.**

## Next Steps

1. **Fix Issue #3**: Properly fix the store instruction generation in stage1 compiler
2. **Run Tests**: Once compiler is fixed, all tests (including scientific computing) will run automatically

## Fixes Applied (No Workarounds)

Following the "No Workarounds Policy", all fixes have been implemented properly in the compiler:

1. **Null Pointer Stores**: Modified `stmt_core.weave` to detect pointer types and literal `0` values, emitting `null` instead
2. **Null Pointer Returns**: Modified `stmt_core.weave` to detect pointer return types and literal `0` values, emitting `null` instead

All fixes are in the compiler codegen, not post-processing workarounds.

