# Memo: Embedded Testing Design for Weave

## Goal
Design an embedded testing system for Weave that:

- Keeps tests **close to the implementation**.
- Uses a **two-phase structure**: setup (arrange/act) and expectation (assert/judge).
- Never fails silently: every failing test must produce a **clear, standardised message**.
- Allows both **simple, standard assertions** and more **cumbersome/custom checks** with rich failure diagnostics.
- Can be extended over time without changing the low-level runtime model.

## High-level Test Structure

Each function can carry an attached `tests` block:

```lisp
(fn add2
  (doc "Returns n + 2.")
  (params (n Int32))
  (returns Int32)
  (body (+ n 2))

  (tests
    (test add2-forty-is-forty-two
      (doc "add2(40) should be 42")
      (tags unit arithmetic)

      (setup
        (let result Int32 (add2 40)))

      (expect-eq result 42))
  ))
```

Key ideas:

- **`setup`**: arbitrary code that prepares the test, calls the function under test, and binds local variables.
- **`expect-*` forms**: declarative assertions that describe what *should* hold after setup.
- The compiler expands these high-level forms into a **low-level testing harness** that uses `if`/`printf`/`return` so you never have to write that boilerplate by hand.

## Standard Assertions (Surface Layer)

Provide a small standard library of assertion forms:

- `expect-eq actual expected`
- `expect-ne actual expected`
- `expect-approx actual expected tol`
- `expect-true cond`
- `expect-false cond`

Example:

```lisp
(test add2-zero-is-two
  (doc "add2(0) should be 2")
  (tags unit)

  (setup
    (let result Int32 (add2 0)))

  (expect-eq result 2))
```

### Behaviour on Failure

Each `expect-*` is guaranteed to:

1. Evaluate its condition.
2. If the condition holds, continue (or return success if it is the last expectation).
3. If the condition fails:
   - Print a **standardised failure message** that includes:
     - Test name.
     - Source location (file, line, column if available).
     - The assertion form (e.g. `expect-eq result 42`).
     - The actual value(s) and expected value(s).
   - Return a non-zero status code for the test.

This ensures *no test ever fails silently*.

## Custom and Extended Diagnostics

Sometimes the standard `"expected X, got Y"` is not enough. For these cases, support an extended assertion form that combines a condition with an optional debug block:

```lisp
(expect
  (cond (< n 4.0)
    (debug
      (printf "norm2(x) = %f (expected < 4.0)\n" n)
      (printf "x = %v\n" x))) )
```

Semantics:

- `cond <predicate> (debug ...)` means:
  - If `<predicate>` is `true`, the expectation passes.
  - If `false`, the `debug` block is executed and then the test returns non-zero.
- The compiler also emits a **default message** using the textual form of `<predicate>` and the test name, so even if the debug block is empty, the failure is still visible and traceable.

For convenience, this can be wrapped in a more ergonomic surface syntax, where `debug` is optional:

```lisp
(expect-true (< n 4.0)
  (debug
    (show n)
    (show x)))
```

Here `show` is sugar for a standard `printf` of name + value, compiled down to `ccall "printf" ...` calls.

## Desugaring to Low-Level Form

The front-end should treat `test`, `setup` and `expect-*` as **syntactic sugar** that is lowered into a low-level representation compatible with the rest of the language and runtime.

Example desugaring (conceptual):

```lisp
(test add2-forty-is-forty-two
  (setup
    (let result Int32 (add2 40)))
  (expect-eq result 42))
```

can be lowered to something akin to:

```lisp
(fn __test_add2-forty-is-forty-two
  (params ())
  (returns Int32)
  (body
    (let result Int32 (add2 40))
    (if-stmt (== result 42)
      (return 0)
      (do
        (ccall "printf" (returns Int32)
               (args
                 (String "add2-forty-is-forty-two: expected result == 42, got %d\0A")
                 (Int32 result)))
        (return 1)))))
```

Key properties:

- Tests become ordinary functions (e.g. `__test_add2-forty-is-forty-two`) returning an `ExitCode`-like integer.
- The **runtime test runner** can discover and execute these functions by convention (name pattern, metadata, tags).
- All formatting and printing is done via normal language primitives (`ccall "printf"`), so no special-casing is needed deeper in the pipeline.

## Grouping and Running Tests

Because tests live *inside* the function definition, you get natural grouping:

- All tests for `add2` sit inside the `add2` function, close to the implementation.
- The compiler can emit metadata (e.g. a table of tests with names, tags, and function pointers) so a test runner can:
  - Run tests for a single function.
  - Filter by tag (`unit`, `integration`, `arithmetic`, etc.).
  - Run the whole suite.

This layout also plays nicely with LLM tooling: the model sees implementation, docstring and tests together, which helps when proposing changes or new cases.

## Extensibility

This design is intentionally layered:

- The **surface syntax** (`test`, `setup`, `expect-*`, `debug`, `show`) is ergonomic and concise.
- The **lowering** to explicit control flow (`if-stmt`, `ccall`, `return`) is simple and transparent.

You can extend the assertion library without touching the backend:

- Add `expect-range x min max` (with a default or custom message).
- Add `expect-close-matrix A B tol` for linear algebra.
- Add domain-specific helpers (e.g. `expect-energy-decrease`, `expect-monotone`) that still expand to the same `if`/`printf`/`return` core.

At the same time, power users can always drop down to the lower-level `expect` + `debug` form when they need very specific failure reporting.

## Summary

- Embed tests per function using `(tests (test ...))` blocks.
- Keep a strict two-phase structure: `setup` for preparation and execution, `expect-*` for assertions.
- Ensure tests never fail silently: failing expectations always print a standard message and return non-zero.
- Provide a small set of high-level assertion forms that desugar into explicit control flow using normal language features.
- Allow extended diagnostics via `debug` blocks and helpers like `show`, so complex test failures include enough information to be actionable.

This gives Weave an embedded testing system that is ergonomic for everyday use, powerful enough for tricky numerical and HPC cases, and friendly to future automation and LLM-based tooling.

