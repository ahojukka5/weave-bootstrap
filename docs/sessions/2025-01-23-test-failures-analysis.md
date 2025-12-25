# Test Failures Analysis and Root Causes

## Overview

After migrating Stage1 to LLVM API, we need to:
1. Remove C runtime dependency (stdlib_runtime.c)
2. Fix all test failures
3. Understand why issues are hard to implement

## Issue Categories

### 1. **Missing Array Runtime Functions** (Stage0 Tests)

**Problem**: Stage0's `arena-create` function calls `weave_array_i32_new()` and `weave_array_str_new()` which are declared but not defined.

**Why it's hard**:
- These functions are implemented in Weave (`stage1/src/arrays.weave`)
- Stage0 is a C compiler - it can't call Weave functions
- Stage0 tests are simple and don't actually need arenas
- But `arena-create` is always generated in the IR

**Root Cause**: Stage0 unconditionally generates `arena-create` function even when not needed.

**Solution Options**:
1. **Don't generate arena-create if unused** (best)
2. Implement minimal C stubs (not desired - we want pure Weave)
3. Make arena-create use malloc directly without arrays (simpler)

---

### 2. **Invalid LLVM Type "program"** (Stage1/Stage2 Tests)

**Problem**: IR generation sometimes emits `program` as a type name, which is invalid LLVM IR.

**Example Error**:
```
Invalid LLVM type: program
Expected: i32, i8*, %StructName, etc.
```

**Why it's hard**:
- The type system in `stage1/src/ir/types.weave` has a fallback that should catch "program"
- But the issue occurs in multiple places:
  - Function return types
  - Bitcast operations  
  - Struct field types
  - Pointer operations

**Root Cause**: Type mapping happens in multiple places and some code paths bypass the `map-type-node` function that has the "program" check.

**Where it happens**:
1. `expr_ccall.weave` - C function return types
2. `pointers.weave` - Bitcast operations
3. `expr_structs.weave` - Struct field access
4. `ir_util.weave` - Function signature collection

**Why it's hard to fix**:
- Type information flows through many layers (typecheck → lower → IR gen)
- Some code directly uses type node values without mapping
- Need to audit every place that emits types to LLVM IR
- Type nodes can be in different states (parsed, lowered, checked)

---

### 3. **C Function Call Type Issues** (Stage1/Stage2)

**Problem**: C function declarations have wrong return types.

**Example**:
```llvm
declare program @abs(i32)  ; WRONG - should be i32
```

**Why it's hard**:
- C function signatures are hardcoded in `expr_ccall.weave`
- The type mapping for return types doesn't always work
- Some C functions have complex signatures (varargs, pointers)

**Root Cause**: `get-ccall-ret-type` function in `expr_ccall.weave` doesn't properly map all types.

---

### 4. **Nested Pointer Type Issues**

**Problem**: Types like `(ptr (ptr Int32))` don't map correctly to LLVM.

**Why it's hard**:
- Pointer nesting creates complex type trees
- LLVM requires explicit pointer levels: `i32**` not `(ptr (ptr i32))`
- Type mapping needs to recursively handle nesting
- Some code assumes single-level pointers

**Root Cause**: `map-type-node` handles `(ptr T)` but may not handle deep nesting correctly in all contexts.

---

### 5. **Struct Field Type Mismatches**

**Problem**: Struct field access generates wrong types.

**Example**:
```llvm
%field = getelementptr inbounds program, program* %s, i32 0, i32 0  ; WRONG
```

**Why it's hard**:
- Struct types need to be resolved through multiple layers
- Field indices need type information
- Struct names must match between declaration and use
- Type checking happens before IR generation, but IR gen needs type info

**Root Cause**: Struct type resolution doesn't always propagate correctly to IR generation.

---

### 6. **Invalid LLVM Identifiers**

**Problem**: Generated identifiers like `%v_l_then_join_53<F0>` contain invalid characters.

**Why it's hard**:
- Identifier generation happens in many places
- Need to sanitize all generated names
- Some names come from user code, some are synthetic
- Must ensure uniqueness while being valid LLVM identifiers

**Root Cause**: Name sanitization (`sanitize-name`) isn't applied everywhere identifiers are generated.

---

## Why These Issues Are Hard

### 1. **Multi-Stage Compilation Pipeline**

The compiler has multiple stages:
- Parsing → Lowering → Type Checking → IR Generation

Type information flows through all stages, and bugs can be introduced at any stage. Fixing one stage may break another.

### 2. **Type System Complexity**

- Types can be: atoms, lists, structs, pointers, nested combinations
- Type information exists in multiple forms:
  - Parsed AST nodes
  - Type-checked nodes  
  - Lowered nodes
  - String representations
- Mapping between forms is error-prone

### 3. **Code Duplication**

Type mapping logic exists in multiple places:
- `map-type-node` - main mapping
- `emit-type-node` - IR emission
- `map-simple-type-name` - simple types
- Direct string comparisons in various files

Changes need to be synchronized across all locations.

### 4. **Testing Challenges**

- Stage1 tests require stage1 to compile (chicken-egg)
- Stage2 tests require stage1 AND stage2 to work
- Some failures are in edge cases that are hard to trigger
- Type errors may not manifest until linking

### 5. **Self-Hosting Complexity**

- The compiler compiles itself
- Bugs can cause cascading failures
- Hard to debug when the debugging tool (compiler) is broken
- Need to maintain compatibility across stages

---

## Fix Strategy

### Phase 1: Fix Stage0 (Easiest)
1. Make `arena-create` generation conditional
2. Or simplify arena to not use arrays
3. Tests should pass immediately

### Phase 2: Fix Type Mapping (Medium)
1. Audit all type emission points
2. Ensure `map-type-node` is always used
3. Add "program" check everywhere
4. Fix nested pointer handling

### Phase 3: Fix C Function Types (Medium)
1. Fix `get-ccall-ret-type` to handle all cases
2. Add proper type mapping for all C functions
3. Test with all ccall usages

### Phase 4: Fix Struct/Array Types (Hard)
1. Fix struct type resolution
2. Fix field access type generation
3. Fix array operation types
4. Requires understanding full type flow

### Phase 5: Fix Identifier Generation (Medium)
1. Find all identifier generation points
2. Apply sanitization everywhere
3. Ensure uniqueness

---

## Estimated Difficulty

- **Stage0 arena fix**: Easy (1-2 hours)
- **Type "program" fix**: Medium (4-8 hours) - need to find all emission points
- **C function types**: Medium (2-4 hours) - localized to expr_ccall.weave
- **Nested pointers**: Hard (8-16 hours) - affects many files
- **Struct fields**: Hard (8-16 hours) - complex type resolution
- **Identifiers**: Medium (4-8 hours) - many locations but straightforward

**Total**: ~30-50 hours of focused work

The hardest part is the type system because it's foundational and touches everything. One small bug can cause failures in many tests.

