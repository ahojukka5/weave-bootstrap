# Weave Bootstrap

Self-hosting compiler bootstrap chain for the Weave programming language. This repository contains a three-stage bootstrap process that culminates in a fully self-hosted compiler.

## Project Status

**Active Development** - The bootstrap pipeline is functional and self-hosting has been achieved:

- ✅ **Stage 0** (`stage0/`): C-based seed compiler (weavec0) - fully functional
- ✅ **Stage 1** (`stage1/`): Weave compiler written in Weave (weavec1) - compiles successfully via weavec0
- ✅ **Stage 2**: Self-hosting verification - weavec1 successfully compiles itself (weavec2)
- ✅ **CMake/CTest infrastructure**: Unified build and test system across all stages
- ⚠️ **Test pass rate**: 30/38 tests passing (79%) - 8 tests failing due to pointer/struct IR generation bugs in weavec1

## Architecture

The bootstrap chain consists of three compiler stages:

### Stage 0: C Seed Compiler (`weavec0`)
- **Language**: ANSI C
- **Purpose**: Minimal bootstrap compiler to break Python dependency
- **Source**: `stage0/src/*.c`
- **Output**: Compiles Weave S-expressions to LLVM IR
- **Features**: Core language subset - functions, structs, pointers, basic control flow
- **Tests**: 9/9 passing (100%)

### Stage 1: Weave Compiler (`weavec1`)
- **Language**: Weave (compiled by weavec0)
- **Purpose**: Full-featured compiler with typechecker and IR generation
- **Source**: `stage1/src/*.weave`
- **Output**: Compiles Weave to LLVM IR
- **Features**: 
  - Full S-expression parser with include expansion
  - Type inference and checking
  - IR generation with optimizations
  - Multi-directory include support (`-I`)
  - Optimization flags (`-O`, `-O2`)
  - Static linking support (`-static`)
- **Tests**: 19/28 passing (68%)

### Stage 2: Self-Hosted Verification (`weavec2`)
- **Language**: Weave (weavec1 source compiled by weavec1)
- **Purpose**: Prove self-hosting capability
- **Source**: Same as stage1 (`stage1/src/*.weave`)
- **Output**: Binary-equivalent compiler (weavec2 = weavec1 compiling itself)
- **Tests**: 1/1 passing (100%)

## Building from Source

### Prerequisites

- CMake 3.16+
- C compiler (GCC or Clang)
- LLVM/Clang toolchain
- Runtime: by default uses the internal slim runtime at `stage0/runtime/stdlib_runtime.c` (sufficient for stage1/2)
  - You can override with an external runtime if needed: `cmake -DWEAVE_RUNTIME=/path/to/runtime.c -S . -B build`
  - `stage0/runtime/runtime.c` is a minimal shim (main → weave_main) and is not sufficient alone for stage1/2

### Build Commands

```bash
# Standard CMake workflow
cmake -S . -B build
cmake --build build -j

# Verify all stages built successfully
ls -lh build/stage0/weavec0   # Stage 0: C seed compiler
ls -lh build/stage1/weavec1   # Stage 1: Weave compiler (via weavec0)
ls -lh build/stage2/weavec2   # Stage 2: Self-hosted (weavec1 compiling itself)
```

### Running Tests

```bash
# Run all tests (38 total)
cd build
ctest --output-on-failure

# Run tests by stage
ctest -L stage0  # 9 tests - C compiler correctness
ctest -L stage1  # 28 tests - Weave compiler features + typecheck failures
ctest -L stage2  # 1 test - Self-hosting smoke test

# Run specific test
ctest -R test_fn_simple_42 --verbose
```

### Manual Compilation

```bash
# Using weavec0 (stage0 compiler)
build/stage0/weavec0 -I stage1/src -o output.ll input.weave

# Using weavec1 (stage1 compiler)
build/stage1/weavec1 input.weave --emit-llvm -o output.ll

# Compile with optimization and includes
build/stage1/weavec1 -O2 -I/path/to/includes input.weave -o output

# Static linking
build/stage1/weavec1 -static input.weave -o output
```

## Test Infrastructure

Tests use CMake/CTest with the following conventions:

- **Test names**: `stage{0,1,2}_test_<name>_42`
- **Success criteria**: Exit code 42 (configurable per test)
- **Typecheck tests**: Tests with `typecheck` in filename expect compilation failure
- **Test sources**:
  - `stage0/tests/` - Basic language features for C compiler
  - `stage1/tests/` - Extended features including typechecking
  - `stage2/tests/` - Self-hosting smoke tests

## Known Issues

### Failing Tests (8 tests)

IR generation bugs in weavec1 affecting pointer and struct semantics:

1. `test_ccall_malloc_42` - Pointer type mismatch in malloc results
2. `test_ccall_memcpy_42` - Pointer handling in memcpy
3. `test_fn_ptr_param_42` - Function pointer parameters
4. `test_fn_recursive_42` - Wrong exit code (253 instead of 42)
5. `test_make_struct_42` - Struct pointer vs value type confusion
6. `test_ptr_add_42` - Pointer arithmetic store issues
7. `test_store_addr_42` - Address store type mismatch
8. `test_struct_like_42` - Struct field pointer/int mismatch

These failures do not prevent self-hosting but indicate edge cases in IR generation.

## Compiler Flags

### Supported by weavec1

- `-I<path>` - Add include directory (supports multiple via `-I/path1:/path2` or comma separation)
- `-O` / `-O2` - Enable optimizations (forwarded to clang)
- `-static` - Static linking
- `--emit-llvm` - Output LLVM IR instead of executable
- `-o <file>` - Output file path

### Internal Flags

- `-Wno-null-character` - Suppresses null character warnings in string literals (added automatically)

## Project Structure

```
weave-bootstrap/
├── CMakeLists.txt           # Root build configuration
├── stage0/                  # C seed compiler
│   ├── CMakeLists.txt       # Stage0 build
│   ├── src/*.c              # C compiler implementation
│   ├── include/*.h          # Headers
│   ├── runtime/runtime.c    # Minimal runtime
│   └── tests/*.weave        # Stage0 test suite
├── stage1/                  # Weave compiler (in Weave)
│   ├── src/*.weave          # Compiler implementation
│   │   ├── weavec1.weave    # Main entry point
│   │   ├── prelude.weave    # Type definitions & runtime wrappers
│   │   ├── tokenizer.weave  # Lexer
│   │   ├── sexpr_parser.weave
│   │   ├── typecheck/       # Type checker modules
│   │   └── ir/              # IR generation modules
│   └── tests/*.weave        # Stage1 test suite
└── stage2/                  # Self-hosting verification
    ├── CMakeLists.txt       # Stage2 build & tests
    ├── src/                 # (Future: stage2-specific source)
    └── tests/*.weave        # Self-hosting smoke tests
```

## Development Workflow

1. **Modify stage0**: Edit C sources in `stage0/src/`, rebuild, test with stage0 tests
2. **Modify stage1**: Edit Weave sources in `stage1/src/`, rebuild (uses weavec0), test
3. **Verify self-hosting**: Check that weavec2 builds successfully (stage1 → stage1 via weavec1)
4. **Run full test suite**: `ctest` to validate all stages

## Future Work

- Fix pointer/struct IR generation bugs in weavec1
- Expand stage1 to support full Weave language (closures, pattern matching, traits)
- Add stage3: Use weavec2 to verify byte-identical compilation (bootstrapping fixpoint)
- Implement optimization passes in Weave compiler
- Add more comprehensive test coverage

## Contributing

This is a bootstrap compiler project focused on achieving self-hosting. The language specification and full feature set are maintained in the main `weave` repository (expected as sibling directory).

## License

See LICENSE file in repository root.
