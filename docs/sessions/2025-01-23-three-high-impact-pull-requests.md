# Three High-Impact Pull Requests

This document identifies three high-impact pull requests that would significantly improve the Weave bootstrap compiler project.

## 1. Fix Type System Issues - Fix 42 Failing Tests ⭐⭐⭐⭐⭐

**Impact**: Critical - Blocks correctness and full self-hosting verification

**Problem**: 
- 42 tests are currently failing (402/444 passing, 91% pass rate)
- IR generation produces invalid LLVM type names like `program` instead of proper types
- Type system issues with nested pointers and struct types
- Missing or incorrect function declarations for C library functions

**Root Causes**:
1. **Invalid LLVM type generation**: The `map-type-node` function in `stage1/src/ir/types.weave` has a fallback that returns `"i32"` for unknown types, but in some cases it's generating `"program"` as a type name, which is invalid in LLVM IR
2. **Type mismatch in function arguments**: Many tests show `wanted i32, got i8*` or vice versa, indicating type inference/checking issues
3. **C function declarations**: Missing proper type declarations for C library functions like `malloc`, `abs`, `atoi`, `exit`, `memcpy`

**Affected Tests**:
- Stage 1: 9 failing tests (C function calls, pointer arithmetic, struct operations)
- Stage 2: 22 failing tests (array operations, C function calls, struct/array operations)
- Embedded tests: Multiple type mismatch errors in IR generation

**Implementation Plan**:

1. **Fix `map-type-node` to never generate invalid types**:
   - Ensure the function always returns valid LLVM type strings
   - Add explicit checks to prevent `"program"`, `"fn"`, `"entry"`, `"module"` from being used as types
   - Improve struct type detection and validation

2. **Fix type inference for pointer operations**:
   - Review `stage1/src/ir/pointers.weave` to ensure proper type propagation
   - Fix `compile-addr`, `compile-load-node`, `compile-store-node`, `compile-get-field-node` functions
   - Ensure pointer arithmetic maintains correct types

3. **Add proper C function declarations**:
   - Update prelude or add declarations for all C library functions used
   - Ensure return types and parameter types match C signatures exactly
   - Fix declarations in `stage1/src/libc.weave` or prelude

4. **Fix struct field type handling**:
   - Review `lower-get-field` and `lower-set-field` in `stage1/src/lower/struct_ops.weave`
   - Ensure struct field access maintains correct types through compilation

**Files to Modify**:
- `stage1/src/ir/types.weave` - Fix type mapping logic
- `stage1/src/ir/pointers.weave` - Fix pointer type handling
- `stage1/src/ir/ir_fns.weave` - Fix function type declarations
- `stage1/src/libc.weave` - Add proper C function declarations
- `stage1/src/lower/struct_ops.weave` - Fix struct field type handling

**Expected Outcome**:
- All 444 tests passing (100% pass rate)
- No invalid LLVM type names in generated IR
- Correct type checking throughout compilation pipeline
- Full self-hosting verification

**Priority**: **CRITICAL** - This blocks correctness and is the foundation for all other improvements.

---

## 2. Migrate Stage1 to LLVM API - Remove system() Calls ⭐⭐⭐⭐

**Impact**: Complete LLVM integration, remove shell dependencies, faster compilation

**Problem**:
- Stage1 (`weavec1.weave`) still uses `system("clang ...")` calls for linking (line 200)
- This is inconsistent with stage0 which uses LLVM API directly
- Creates platform dependencies (requires shell, clang in PATH)
- Slower due to fork/exec overhead
- Less robust error handling

**Current State**:
```weave
;; Line 200 in stage1/src/weavec1.weave
(let cmd String (string-concat "clang" (string-concat extra ...)))
(let link-rc Int32 (ccall "system" (returns Int32) (args (String cmd))))
```

**Implementation Plan**:

1. **Add LLVM linking API to stage0**:
   - Extend `stage0/src/llvm_compile.c` to add linking functionality
   - Add `llvm_link_objects` function that uses LLVM's linker (or lld)
   - Expose via ccall in `stage0/src/program.c`

2. **Replace system() call in stage1**:
   - Replace `system("clang ...")` with LLVM API call
   - Use new `llvm-link-objects` ccall function
   - Handle runtime linking properly

3. **Add proper error handling**:
   - Return meaningful error codes
   - Provide better error messages than shell exit codes

**Files to Modify**:
- `stage0/include/llvm_compile.h` - Add linking function declaration
- `stage0/src/llvm_compile.c` - Implement `llvm_link_objects` function
- `stage0/src/program.c` - Add ccall declaration for `llvm-link-objects`
- `stage1/src/weavec1.weave` - Replace `system()` call with LLVM API call

**Alternative Approach** (if full LLVM linking is complex):
- Use LLVM's `LLVMLinkModules` API to link object files
- Or use `lld` directly via LLVM API instead of system call

**Expected Outcome**:
- No `system()` calls in stage1
- Faster compilation (no process spawning)
- Better error handling
- Cross-platform (no shell dependency)
- Consistent with stage0 approach

**Priority**: **HIGH** - Completes LLVM integration and improves build system quality.

---

## 3. Implement LLVM Optimization Passes ⭐⭐⭐⭐

**Impact**: Significantly better performance for compiled code

**Problem**:
- Optimization level (`-O`, `-O2`) is passed to codegen but no actual optimization passes are run
- `llvm_compile.c` has `get_opt_level()` function that converts optimization level to `LLVMCodeGenOptLevel`
- However, this only affects code generation, not optimization passes
- Generated code is unoptimized even with `-O2` flag

**Current State**:
- `stage0/src/llvm_compile.c` line 112: `get_opt_level(opt_level)` is passed to `LLVMCreateTargetMachine`
- This sets the codegen optimization level but doesn't run optimization passes
- No `LLVMRunPassManager` or `LLVMRunFunctionPassManager` calls

**Implementation Plan**:

1. **Add optimization pass manager**:
   - Create `LLVMPassManagerRef` in `llvm_compile_ir_to_object_asan`
   - Add standard optimization passes based on optimization level:
     - `-O0`: No passes (current behavior)
     - `-O1`: Basic optimizations (inlining, constant propagation)
     - `-O2`: Standard optimizations (all of O1 + dead code elimination, loop optimizations)
     - `-O3`: Aggressive optimizations (all of O2 + vectorization, etc.)

2. **Run passes before codegen**:
   - Run optimization passes on the module before emitting object code
   - Use `LLVMRunPassManager` for module-level passes
   - Or use `LLVMRunFunctionPassManager` for function-level passes

3. **Add pass manager creation helper**:
   - Create function `create_pass_manager(opt_level)` that sets up appropriate passes
   - Use LLVM's pass builder API or legacy pass manager API

**Files to Modify**:
- `stage0/include/llvm_compile.h` - Add pass manager function declarations
- `stage0/src/llvm_compile.c` - Implement optimization pass setup and execution
- May need to add `#include <llvm-c/Transforms/PassManagerBuilder.h>` or use new pass manager API

**LLVM API Usage**:
```c
// Legacy API (simpler, widely available)
LLVMPassManagerRef pm = LLVMCreatePassManager();
LLVMAddConstantPropagationPass(pm);
LLVMAddInstructionCombiningPass(pm);
LLVMAddPromoteMemoryToRegisterPass(pm);
// ... more passes based on opt_level
LLVMRunPassManager(pm, module);
LLVMDisposePassManager(pm);
```

**Expected Outcome**:
- `-O2` flag actually runs optimizations
- Generated code performance comparable to `clang -O2`
- Smaller binaries with dead code elimination
- Better code quality overall

**Priority**: **HIGH** - Significant performance improvement for compiled Weave programs.

---

## Summary

These three pull requests address:
1. **Correctness** (PR #1) - Fix 42 failing tests, ensure type system works correctly
2. **Architecture** (PR #2) - Complete LLVM integration, remove shell dependencies
3. **Performance** (PR #3) - Actually optimize generated code

**Recommended Order**:
1. Start with PR #1 (type system fixes) - Foundation for correctness
2. Then PR #2 (LLVM API migration) - Clean up architecture
3. Finally PR #3 (optimization passes) - Performance polish

All three are high-impact and would significantly improve the project's quality, correctness, and performance.

