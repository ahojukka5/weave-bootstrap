# Stage 1 (weavec1) — Minimal Real Compiler

Stage 1 is the first bootstrap stage that looks like a real compiler pipeline:

1. **Tokenize** `.weave` source into tokens
2. **Parse** tokens into a structured representation (an arena-based S-expression tree)
3. **Compile** a tiny language subset to **LLVM IR**

Stage 1 is still intentionally small, but it is designed to be extended feature-by-feature.

## Goals

- Replace Stage 0’s ad-hoc “scan text for `(return ...)`” with a real tokenizer + parser.
- Use **ADT/structured data**, not HashMap ASTs.
- Keep the compiler implementation in `.weave` and organized via `(include ...)`.
- Keep compiler sources using kebab-case helper wrappers (no `weave_*` calls directly).

## Current Supported Subset (Stage 1)

The compiler is currently meant to handle a small subset of Weave programs:

- Top-level: `(program ...)` with one `(entry main ...)`.
- Statements: `(do ...)`, `(let name Int32 init)`, `(set name expr)`, `(while cond body)`, `(return expr)`, `(if-stmt cond then else)`, expression statements (anything else falls back to “evaluate for side effects”).
- Expressions: integer literals, string literals (lower to unnamed LLVM globals), variable refs, `(+ a b)`, comparisons like `(== a b)` (returns `0`/`1` as `i32`), `ccall`, pointer ops `(addr name)`, `(load Ty ptr-expr)`, `(store Ty value-expr ptr-expr)`, `(bitcast Ptr<T> expr)`, `(get-field Ty idx ptr-expr)` (GEP assuming the pointer already has the right element type).
- Struct declarations: `(struct Name (fields Ty1 Ty2 ...))` emit `%Name = type { <mapped tys> }` and are used by `(get-field Struct<Name> idx ptr)` to GEP with an extra leading `0` index.
- Let/set now respect the annotated type, so you can bind `Ptr<Int8>`/`Ptr<Int32>` locals (alloca uses the mapped type).
- `ccall`: maps `Int32 -> i32`, `Int8 -> i8`, `String -> i8*`, `Void -> void`, `Ptr<T> -> <T>*`, `Struct<Name> -> %Name` (no struct layout emission yet). Duplicate `declare` lines are deduplicated inside a single module.
- Documentation nodes: `(doc "text")` may appear at module level or as the first element of a function body; they are ignored by codegen but parsed so humans can annotate modules/functions.

### Important restriction (for now)

To keep LLVM IR emission simple, Stage 1 currently assumes variable names are
LLVM-friendly identifiers like `x`, `tmp1`, `value_2`.

Avoid names with `-` (kebab-case) for local variables in Stage 1 test programs,
because LLVM local identifiers require quoting/escaping for such names and Stage 1
does not implement that yet.

## Directory Map

- `bootstrap/stage1/src/`: all Stage 1 compiler sources.
  - `weavec1.weave`: compiler entrypoint.
  - `prelude.weave`: Stage 1 wrappers (imports Stage 0 prelude + array helpers).
  - `tokenizer.weave`, `sexpr_parser.weave`, `arena.weave`: frontend.
  - `ir/`: IR generation pieces (`main.weave`, `stmt.weave`, `expr.weave`, `emit.weave`, `util.weave`).
- `bootstrap/stage1/tests/`: Stage 1 sample programs (each returns 42).

## Build and Verify

Build:

```bash
scripts/build-bootstrap1.sh      # uses stage0 (weavec0) to build weavec1
```

Verify end-to-end (weavec1 → IR → clang+runtime → run):

```bash
scripts/verify-bootstrap1.sh
```

## When Stage 1 Becomes Stage 2

Stage 2 should start once Stage 1 is no longer “just enough to prove parsing + tiny codegen”, and we’re ready to add
compiler features that change the architecture (not just add one more expression):

- A proper typed AST (beyond raw S-expr trees)
- Type checking (even if minimal)
- More real language constructs (if/while, function calls, multiple functions)
- Compiler-in-compiler compilation targets (eventually: compile `weavec1.weave` with `weavec1`)

In practice: once Stage 1 can compile a small suite of `.weave` programs that resemble compiler code (not just toy returns),
it’s time to introduce Stage 2’s “real compiler architecture” modules.
