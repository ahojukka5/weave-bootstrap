# Refactoring compile-expr: Alternative Architectures

## Current Problem

`compile-expr` in `stage1/src/ir/core/expr_compile.weave` is 1041 lines, exceeding the 1024 line limit. It's a monolithic recursive dispatcher with many `if-stmt` checks for different expression forms.

## Current Architecture

**Monolithic Dispatcher Pattern:**
- Single large `compile-expr` function
- Many sequential `if-stmt` checks for expression kinds
- All logic inline within the dispatcher
- Recursive calls to `compile-expr` for sub-expressions

**Existing Separate Files (but not fully utilized):**
- `expr_core_atoms.weave` - atom compilation helpers
- `expr_binops.weave` - binary operation helpers  
- `expr_structs.weave` - struct field access (has `compile-get-field-expr`)
- `expr_ccall.weave` - C call helpers
- `pointers.weave` - pointer operations (has `compile-ptr-add-node`)
- `expr_calls.weave` - function call helpers

## Alternative Architectures

### Option 1: Split by Expression Kind (Recommended)

**Structure:**
```
compile-expr (main dispatcher, ~100 lines)
  ├─> compile-atom-expr (atoms, literals)
  ├─> compile-binary-op-expr (+, -, *, ==, etc.)
  ├─> compile-function-call-expr (function calls)
  ├─> compile-get-field-expr (struct field access)
  ├─> compile-ptr-add-expr (pointer arithmetic)
  ├─> compile-let-expr (let bindings)
  ├─> compile-if-expr (conditional expressions)
  ├─> compile-do-expr (do blocks)
  └─> ... (other expression types)
```

**Benefits:**
- Each function handles one expression kind
- Functions can be in separate files
- Easier to test and maintain
- Natural file size limits
- Clear separation of concerns

**Implementation:**
- Main `compile-expr` becomes a thin dispatcher
- Extract each `if-stmt` branch into a separate function
- Functions take same parameters as `compile-expr`
- Each function in its own file (or grouped logically)

### Option 2: Table-Driven Dispatch

**Structure:**
```
expression-kind -> compilation-function mapping
Use a lookup table or pattern matching
```

**Benefits:**
- Very fast dispatch
- Easy to add new expression types
- Centralized registration

**Drawbacks:**
- More complex setup
- Less flexible for special cases
- Harder to debug

### Option 3: Visitor Pattern

**Structure:**
```
Expression visitor that dispatches based on node kind
Each expression type has its own visit method
```

**Benefits:**
- Classic OOP pattern
- Very extensible
- Clear separation

**Drawbacks:**
- More boilerplate
- May not fit Weave's functional style
- Requires more infrastructure

### Option 4: Split by Concern (Current Partial Approach)

**Structure:**
```
Keep current structure but extract more helpers
- expr_core_atoms.weave - all atom handling
- expr_binops.weave - all binary ops
- expr_structs.weave - all struct ops
- expr_calls.weave - all function calls
- expr_compile.weave - just dispatcher
```

**Benefits:**
- Minimal changes to current code
- Natural grouping
- Files already exist

**Drawbacks:**
- Still need to refactor main dispatcher
- Some expression types don't fit cleanly

## Recommended Approach: Option 1

**Phase 1: Extract Expression Handlers**
1. Create `compile-atom-expr` in `expr_core_atoms.weave`
2. Create `compile-binary-op-expr` in `expr_binops.weave`
3. Create `compile-function-call-expr` in `expr_calls.weave`
4. Create `compile-let-expr` in new `expr_let.weave`
5. Create `compile-if-expr` in new `expr_control.weave`
6. Create `compile-do-expr` in new `expr_control.weave`
7. Extract other expression types similarly

**Phase 2: Simplify Main Dispatcher**
1. `compile-expr` becomes a thin dispatcher (~100-200 lines)
2. Each `if-stmt` becomes a function call
3. All logic moved to specialized functions

**Example Refactored Structure:**

```weave
;; expr_compile.weave (main dispatcher, ~150 lines)
(fn compile-expr ...)
  (if-stmt (== (arena-kind a node) (node-kind-atom))
    (return (compile-atom-expr a node ...))
    (do 0)
  )
  (if-stmt (== (arena-kind a node) (node-kind-list))
    (do
      (let head Int32 (arena-first-child a node))
      (let head-val String (arena-value a head))
      (if-stmt (string-eq head-val "let")
        (return (compile-let-expr a node ...))
        (do 0)
      )
      (if-stmt (string-eq head-val "+")
        (return (compile-binary-op-expr a node ...))
        (do 0)
      )
      ;; ... other dispatches
    )
  )
)

;; expr_let.weave (let expression handler, ~50 lines)
(fn compile-let-expr ...)
  ;; All let-specific logic here

;; expr_binops.weave (binary operations, ~200 lines)
(fn compile-binary-op-expr ...)
  ;; All binary op logic here
```

## Benefits of Refactoring

1. **File Size**: Each file stays under limits naturally
2. **Maintainability**: Each expression type is isolated
3. **Testability**: Can test each handler independently
4. **Readability**: Clear what each function does
5. **Extensibility**: Easy to add new expression types
6. **No Workarounds**: Proper architectural solution

## Migration Strategy

1. Start with one expression type (e.g., `let`)
2. Extract it to a separate function
3. Update dispatcher to call it
4. Verify tests still pass
5. Repeat for other expression types
6. Eventually `compile-expr` becomes just dispatch

## Comparison with Other Compilers

**LLVM**: Uses separate functions for each instruction type
**Julia**: Has separate codegen functions for different expression types
**Rust**: Uses visitor pattern with separate handlers
**GCC**: Uses separate functions for different tree node types

All major compilers avoid monolithic dispatchers for exactly this reason.

