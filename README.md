# Weave Bootstrap

Self-hosting compiler bootstrap chain for the Weave programming language. This repository contains a three-stage bootstrap process that culminates in a fully self-hosted compiler.

## Project Status

**Active Development** - The bootstrap pipeline is functional and self-hosting has been achieved:

- ✅ **Stage 0** (`stage0/`): C-based seed compiler (weavec0) - fully functional
- ✅ **Stage 1** (`stage1/`): Weave compiler written in Weave (weavec1) - compiles successfully via weavec0
- ✅ **Stage 2**: Self-hosting verification - weavec1 successfully compiles itself (weavec2)
- ✅ **CMake/CTest infrastructure**: Unified build and test system across all stages
- ⚠️ **Test pass rate**: 402/444 tests passing (91%) - 42 tests failing due to IR generation bugs and type system issues

## Architecture

The bootstrap chain consists of three compiler stages:

### Stage 0: C Seed Compiler (`weavec0`)
- **Language**: ANSI C
- **Purpose**: Minimal bootstrap compiler to break Python dependency
- **Source**: `stage0/src/*.c`
- **Output**: Compiles Weave S-expressions to LLVM IR
- **Features**: Core language subset - functions, structs, pointers, basic control flow
- **Tests**: 18/18 passing (100%)

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
- **Tests**: 333/342 passing (97%) - 9 failures

### Stage 2: Self-Hosted Verification (`weavec2`)
- **Language**: Weave (weavec1 source compiled by weavec1)
- **Purpose**: Prove self-hosting capability
- **Source**: Same as stage1 (`stage1/src/*.weave`)
- **Output**: Binary-equivalent compiler (weavec2 = weavec1 compiling itself)
- **Tests**: 62/84 passing (74%) - 22 failures

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
# Run all tests (444 total)
cd build
ctest --output-on-failure

# Run tests by stage
ctest -L stage0  # 18 tests - C compiler correctness
ctest -L stage1  # 342 tests - Weave compiler features + embedded tests + typecheck failures
ctest -L stage2  # 84 tests - Self-hosting verification tests

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

### Failing Tests (42 tests total)

**Stage 1 failures (9 tests):**
- `test_ccall_abs_42`, `test_ccall_atoi_42`, `test_ccall_exit_42`, `test_ccall_malloc_42`, `test_ccall_memcpy_42` - C function call type issues (generating invalid LLVM types like `program` instead of proper types)
- `test_ptr_add_42` - Pointer arithmetic issues
- `test_struct_like_42` - Struct field type mismatches
- Several embedded tests: IR generation bugs with invalid LLVM identifiers (e.g., `%v_l_then_join_53<F0>`, `%v_l_then_join_53@`), type system issues with nested pointers, and missing function declarations

**Stage 2 failures (22 tests):**
- Array operations: `test_array_contains_42`, `test_array_copy_42`, `test_array_count_42`, `test_array_fill_42`, `test_array_max_42`, `test_array_min_42`, `test_array_reverse_42` - Invalid LLVM type `program` in bitcast operations
- C function calls: `test_ccall_abs_42`, `test_ccall_atoi_42`, `test_ccall_exit_42`, `test_ccall_malloc_42`, `test_ccall_memcpy_42` - Invalid return types in function declarations
- Other failures: `test_average_42`, `test_binary_search_42`, `test_count_until_zero_42`, `test_find_index_42`, `test_matrix_add_42`, `test_median_42`, `test_ptr_add_42`, `test_struct_like_42`, `test_sum_array_42`, `test_variance_42` - Similar type system issues with struct/array operations

These failures are primarily due to:
1. IR generation producing invalid LLVM type names (e.g., `program` instead of proper types)
2. Type system issues with nested pointers and struct types
3. Missing or incorrect function declarations for C library functions

Self-hosting is functional, but these edge cases need to be addressed for full correctness.

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
