# Stage 0 — C Seed Compiler (`weavec0`)

This directory is intended to be **self-contained**: you can copy `bootstrap/stage0/` into a fresh repo and still build and run Stage 0.

Stage 0 is the first compiler in the bootstrap chain. It is deliberately small and grows only as needed to compile the next stage compiler.

## Directory layout

- `src/` – ANSI C (C89) implementation of the seed compiler.
- `include/` – headers for the seed compiler.
- `tests/` – minimal `.weave` programs used to test Stage 0.
- `Makefile` – build + test entrypoint for this stage.

## Build and test

From this directory:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

This builds the Stage 0 compiler (`weavec0`) and runs the `.weave` tests under `tests/` by:

1. Running `weavec0` to generate LLVM IR
2. Compiling the IR with `clang` + `runtime/runtime.c`
3. Running the resulting executable and checking the exit code

## Scope (today)

The current Stage 0 compiler is still a **seed**: it recognizes enough structure to find a `(return <int>)` and emits a tiny LLVM module that returns that value from `main`.

It already implements:

- S-expression parsing with `;` line comments
- `(include "...")` resolution (searching `stdlib/` then `bootstrap/` paths) for bootstrap work

## Roadmap: why `ccall` matters

As Stage 0 grows to compile `weavec1`, it will need a minimal FFI story. Supporting `(ccall ...)` early is valuable because it avoids creating a large wrapper prelude: the bootstrap compiler sources can call runtime/C functions directly.

The planned minimum `ccall` subset is:

- `(ccall "symbol" (returns Int32) (args (Int32 ...) (String ...) ...))`
- Implemented as an LLVM `declare` + `call` with simple type mapping.

This is *not implemented yet*; it comes with the “Stage 0 → Stage 1” work (functions + calls + control flow).
