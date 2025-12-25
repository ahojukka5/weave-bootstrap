# Fix Memory Corruption in Typechecker

## Problem Summary

The Weave compiler's typechecker was experiencing memory corruption where `TypeRef` structures were being overwritten with string data. The corruption value `0x65685f76` (interpreted as ASCII: `'v_he'`) suggested that SSA name strings were overwriting `TypeRef` memory.

### Root Cause Identified ✅
**Heap-buffer-overflow in `make_ssa_name` function** (`stage0/src/env.c:75`)

The function was allocating insufficient memory for the SSA name string, causing a buffer overflow that corrupted adjacent memory (including `TypeRef` structures).

### Fix Applied ✅
Fixed buffer allocation in `make_ssa_name`:
- **Before**: Allocated `nb + written + 3` bytes (insufficient)
- **After**: Allocated `nb + written + 4` bytes (correct)
- **Issue**: Needed space for `'v' + '_' + base + '_' + numbuf + '\0'` = `nb + written + 4`, but was allocating only `nb + written + 3`

### Verification ✅
- AddressSanitizer no longer reports errors
- Stage1 compiler compiles successfully
- Typechecker now works correctly

## Root Cause Analysis Plan

### Hypothesis 1: Memory Reuse After Free
**Theory**: `TypeRef` structure is freed and memory is reused for SSA name strings.

**Investigation Steps**:
1. Check if `TypeRef` structures are ever freed during compilation
2. Verify that `xmalloc` allocations persist throughout compilation
3. Use memory debugging tools (Valgrind/AddressSanitizer) to detect use-after-free

**Files to Check**:
- `stage0/src/types.c` - TypeRef allocation functions
- `stage0/src/env.c` - Environment management
- `stage0/src/program.c` - Function compilation

### Hypothesis 2: Buffer Overflow in String Operations
**Theory**: SSA name creation (`make_ssa_name`) writes beyond allocated bounds, corrupting adjacent `TypeRef` structures.

**Investigation Steps**:
1. Verify all `memcpy` operations in `make_ssa_name` use correct bounds
2. Check for off-by-one errors in string allocation
3. Verify `snprintf` buffer sizes are correct

**Files to Check**:
- `stage0/src/env.c:61-78` - `make_ssa_name` function
- `stage0/src/env.c:38-59` - `sanitize_name` function

### Hypothesis 3: Array Reallocation Bug
**Theory**: When `env_reserve` reallocates arrays, `TypeRef` pointers are corrupted or point to freed memory.

**Investigation Steps**:
1. Verify `xrealloc` preserves pointer values correctly
2. Check if `TypeRef` pointers are stored correctly after reallocation
3. Verify array bounds are correct when accessing `e->types[idx]`

**Files to Check**:
- `stage0/src/env.c:28-36` - `env_reserve` function
- `stage0/src/env.c:80-86` - `env_add` function
- `stage0/src/common.c:27-31` - `xrealloc` implementation

### Hypothesis 4: TypeRef Pointer Corruption
**Theory**: The `TypeRef` pointer stored in environment is pointing to wrong memory location.

**Investigation Steps**:
1. Verify `TypeRef` pointer is stored correctly in `env_add`
2. Check if pointer is modified between storage and retrieval
3. Verify pointer is not pointing to stack-allocated memory

**Files to Check**:
- `stage0/src/program.c:217-234` - Parameter type parsing and storage
- `stage0/src/env.c:102-110` - `env_type` retrieval

## Fix Implementation Plan

### Phase 1: Identify Root Cause
1. **Add Memory Debugging**
   - Enable AddressSanitizer in CMake build
   - Add debug prints to track `TypeRef` pointer values and memory addresses
   - Verify `TypeRef` structures are not freed during compilation

2. **Add Defensive Assertions**
   - Assert that `TypeRef` pointers are non-NULL when stored
   - Assert that `TypeRef->kind` is valid (0-4) when stored
   - Assert that `TypeRef` pointer is still valid when retrieved

3. **Trace Memory Lifecycle**
   - Log when `TypeRef` is allocated (with address)
   - Log when `TypeRef` is stored in environment (with address)
   - Log when `TypeRef` is retrieved (with address and kind)
   - Compare addresses to detect if pointer is pointing to wrong memory

### Phase 2: Implement Fix
1. **Fix Root Cause**
   - Based on Phase 1 findings, implement proper fix
   - Ensure fix addresses root cause, not just symptoms
   - Remove any workarounds or defensive checks that are no longer needed

2. **Verify Fix**
   - Run all Stage0 tests
   - Compile Stage1 compiler
   - Verify no memory corruption occurs
   - Verify typechecker errors are correct

### Phase 3: Cleanup
1. **Remove Debug Code**
   - Remove temporary debug prints
   - Remove unnecessary defensive checks (if root cause is fixed)
   - Keep essential defensive checks for robustness

2. **Documentation**
   - Document the root cause and fix
   - Add comments explaining why fix is necessary
   - Update any relevant documentation

## Implementation Steps (Completed)

### Step 1: Enable AddressSanitizer ✅
Enabled AddressSanitizer in Debug builds in `stage0/CMakeLists.txt`:
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()
```

### Step 2: Add TypeRef Lifecycle Tracking ✅
Added debug prints (gated by `WEAVEC0_DEBUG_MEM` environment variable):
- `type_ptr` and `type_struct`: log allocation with address and kind
- `env_add`: log storage with index and type pointer
- `env_type`: log retrieval with validation check

### Step 3: Identify Root Cause ✅
AddressSanitizer detected **heap-buffer-overflow in `make_ssa_name` at line 75**:
- Buffer allocation was insufficient: `nb + written + 3` bytes
- Required: `nb + written + 4` bytes (for `'v' + '_' + base + '_' + numbuf + '\0'`)
- Overflow was corrupting adjacent memory, including `TypeRef` structures

### Step 4: Implement Fix ✅
Fixed buffer allocation in `make_ssa_name`:
```c
// Before: out = (char *)xmalloc(nb + (size_t)written + 3);
// After:
out = (char *)xmalloc(nb + (size_t)written + 4);
out[0] = 'v';
out[1] = '_';
memcpy(out + 2, base, nb);
out[2 + nb] = '_';
memcpy(out + 3 + nb, numbuf, (size_t)written);
out[3 + nb + (size_t)written] = '\0';
```

### Step 5: Verify Fix ✅
- AddressSanitizer no longer reports errors
- Stage1 compiler compiles successfully
- Typechecker works correctly

## Testing Plan

1. **Unit Tests**
   - Test `env_add` with multiple parameters
   - Test `env_reserve` reallocation
   - Test `make_ssa_name` with various inputs

2. **Integration Tests**
   - Compile Stage1 compiler
   - Run all Stage0 tests
   - Verify no memory corruption

3. **Regression Tests**
   - Verify existing functionality still works
   - Check that typechecker errors are still correct
   - Ensure no performance regression

## Success Criteria

1. ✅ No memory corruption detected by AddressSanitizer
2. ✅ Stage1 compiler compiles successfully
3. ✅ Typechecker reports correct types (no corruption)
4. ✅ No defensive workarounds needed - root cause fixed
5. ✅ Code is clean and maintainable
6. ✅ Debug code is gated behind environment variables (optional cleanup)

## Files to Modify

### Primary Files
- `stage0/src/env.c` - Environment management (likely fix location)
- `stage0/src/program.c` - Parameter type parsing
- `stage0/src/types.c` - TypeRef allocation
- `stage0/CMakeLists.txt` - Enable AddressSanitizer

### Secondary Files (if needed)
- `stage0/src/common.c` - Memory allocation functions
- `stage0/src/expr.c` - Expression compilation (defensive checks)

## Actual Effort

- **Phase 1 (Root Cause)**: ~1 hour (AddressSanitizer immediately identified the issue)
- **Phase 2 (Fix)**: ~15 minutes (simple buffer size correction)
- **Phase 3 (Verification)**: ~15 minutes
- **Total**: ~1.5 hours

## Notes

- AddressSanitizer was instrumental in quickly identifying the root cause
- The corruption value `0x65685f76` = `'v_he'` was indeed from SSA name strings
- The fix was straightforward once the root cause was identified
- No workarounds needed - proper fix implemented
- Debug code is kept (gated behind environment variables) for future debugging

