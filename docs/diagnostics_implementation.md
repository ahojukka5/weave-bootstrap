# Diagnostics Module Implementation

## Overview
Implemented centralized diagnostics infrastructure to provide consistent, structured error/warning reporting across all compiler phases (parsing, type checking, code generation).

## Changes

### New Files Created
- **stage0/include/diagnostics.h**: Diagnostics API header
  - `DiagSeverity` enum (ERROR, WARNING, NOTE)
  - `diag_report()`: Core reporting function with location/code/message/detail
  - `diag_error()`, `diag_warn()`, `diag_note()`: Convenience wrappers
  - `diag_fatal()`: Report error and exit immediately

- **stage0/src/diagnostics.c**: Implementation
  - Structured output format: `filename:line:col: severity: [code]: message`
  - Optional detail notes printed on separate line
  - Consistent stderr output

### Files Modified

#### stage0/CMakeLists.txt
- Added `src/diagnostics.c` to weavec0 sources

#### stage0/src/expr.c
- Added `#include "diagnostics.h"`
- Refactored `ensure_type_ctx_at()` to use `diag_fatal()` instead of `fprintf + die`
- Type mismatch errors now formatted as:
  ```
  file:line:col: error: [type-mismatch]: type mismatch in expression
    note: in function 'fn_name', context 'ctx': wanted Type1, got Type2
  ```

#### stage0/src/program.c
- Added `#include "diagnostics.h"`
- Replaced `fprintf + exit(1)` calls with `diag_fatal()` for:
  - Empty (tests ...) sections
  - Missing (tests ...) sections
- Improved expect-eq/expect-ne printf formatting:
  - Chooses `%d` vs `%p` based on operand type
  - Emits correctly typed LLVM IR arguments

#### stage0/src/sexpr.c
- Added `#include "diagnostics.h"` (prepared for future refactoring)

## Benefits

### Consistency
All error messages now follow a uniform format with:
- Precise source location (file:line:col)
- Severity level (error/warning/note)
- Error code for categorization
- Primary message and optional details

### Tooling-Friendly
- Structured format enables editor integration
- Error codes allow systematic handling
- Clear separation of location, severity, and message

### Developer Experience
- More informative error messages with context
- Better type mismatch reporting showing expected vs actual types
- Easier to locate and fix issues

## Example Output

**Before:**
```
type mismatch in __test_array-str-new_0 (ctx=cmp): wanted i32, got i8*
type mismatch in expression
```

**After:**
```
/home/user/arrays.weave:26:24: error: [type-mismatch]: type mismatch in expression
  note: in function '__test_array-str-new_0', context 'cmp': wanted i32, got i8*
```

## Testing

- Stage0 tests: 18/18 passing ✓
- Stage1 tests: Same failures as before, but with improved error messages
- Build: Clean compilation with no warnings

## Future Work

1. Extend to parser (sexpr.c) for syntax errors
2. Add warning levels and -Werror support
3. Optional JSONL output for machine-readable diagnostics
4. Integrate with CI for automated error pattern detection
5. Add diagnostic suppression/filtering capabilities
