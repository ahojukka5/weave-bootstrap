# Embedded Tests in Functions (Stage 0)

This seed compiler supports embedding unit tests directly inside `fn` definitions.

## Syntax

Inside a function form, add a `(tests ...)` block containing one or more `(test ...)` items:

```lisp
(fn add2
  (doc "Returns n + 2.")
  (params (n Int32))
  (returns Int32)
  (body (+ n 2))
  (tests
    (test "add2(40) == 42"
      (body
        (do
          (let result Int32 (add2 40))
          (if-stmt (== result 42)
            (return 0)
            (return 1)
          )
        )
      )
    )
  )
)
```

- Each `(test ...)` wraps a `(body ...)` block that must return `Int32`: `0` for pass, nonzero for failure.
- Tests are ignored for normal builds; they compile only when running in test mode.

## Running Embedded Tests

Use the `-generate-tests` flag (alias: `-run-tests`):

```bash
# Emit LLVM IR containing test functions and a synthetic main
./weavec0 -generate-tests -emit-llvm -o out.ll path/to/program.weave

# Produce an executable and run
./weavec0 -generate-tests -o a.out path/to/program.weave -runtime stage0/runtime/runtime.c
./a.out; echo "failures=$?"
```

Behavior:
- The compiler emits a synthetic `weave_main` that calls all embedded tests and returns the number of failing tests.
- User-defined `(entry ...)` is ignored in `-generate-tests` mode.
- In normal mode (no `-generate-tests`), embedded tests are ignored and your `entry` is compiled as usual.

## Selecting tests

You can filter by name or tag:

```bash
./weavec0 -generate-tests -tag unit -tag arithmetic -o a.out program.weave -runtime stage0/runtime/runtime.c
./weavec0 -generate-tests -test add2-zero-is-two -o a.out program.weave -runtime stage0/runtime/runtime.c
```

The executable prints each test name before running and returns the number of failures.

## Two-phase test structure

Tests may use a two-phase structure:

```lisp
(test add2-zero-is-two
  (doc "add2(0) should be 2")
  (tags unit)
  (setup
    (let result Int32 (add2 0))
  )
  (inspect
    (do
      (if-stmt (== result 2)
        (return 0)
        (do
          (ccall "printf" (returns Int32) (args (String "expected 2, got %d\0A") (Int32 result)))
          (return 1)
        )
      )
    )
  )
)
```

## Notes

- Test bodies can use any existing statements/expressions, including calls to the function under test.
- Test names (strings) are currently not used by the runner; they may be surfaced later via a listing flag.
