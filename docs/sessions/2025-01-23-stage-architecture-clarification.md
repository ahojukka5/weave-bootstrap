# Stage Architecture Clarification

## Current Understanding

### Stage 0: C Seed Compiler (`weavec0`)
- **Purpose**: Minimal bootstrap compiler written in C
- **Compiles**: Weave S-expressions → LLVM IR
- **Tests**: `stage0/tests/` - Basic language features for C compiler

### Stage 1: Weave Compiler (`weavec1`)
- **Purpose**: Full-featured compiler written in Weave (compiled by `weavec0`)
- **Source**: `stage1/src/*.weave` - **This is where actual development happens**
- **Tests**: `stage1/tests/` - Currently only 30 basic tests
- **Status**: This is the main development target

### Stage 2: Self-Hosting Verification (`weavec2`)
- **Purpose**: Proof of concept - `weavec1` compiles itself
- **Source**: Same as stage1 (`stage1/src/*.weave` compiled by `weavec1`)
- **Tests**: `stage2/tests/` - Currently 89 comprehensive tests
- **Status**: Just verification that self-hosting works

## The Problem

**Stage 2 tests are mislocated!**

- Stage 2 has 89 comprehensive tests (including scientific computing examples)
- Stage 1 only has 30 basic tests
- But Stage 1 is where actual development happens
- Stage 2 is just proof of concept for self-hosting

**The comprehensive test suite should be in Stage 1, not Stage 2.**

## What Should Happen

### Stage 1 Tests (Development Tests)
- Should contain ALL comprehensive tests (currently 89 in stage2)
- These test the compiler being developed
- Should include:
  - Basic language features
  - Scientific computing examples
  - Edge cases
  - Typecheck failures
  - All current `stage2/tests/` content

### Stage 2 Tests (Self-Hosting Verification)
- Should only have minimal smoke tests
- Purpose: Verify that `weavec1` can compile itself
- Examples:
  - `test_smoke_exit_42.weave` - Basic compilation works
  - Maybe 2-3 more minimal tests
  - Not comprehensive - just proof that self-hosting works

## Current Test Distribution

- **Stage 1**: 30 tests (too few - basic features only)
- **Stage 2**: 89 tests (too many - should be in stage1)

## Recommendation

1. **Move comprehensive tests from `stage2/tests/` to `stage1/tests/`**
2. **Keep only minimal smoke tests in `stage2/tests/`** (3-5 tests max)
3. **Update CMakeLists.txt** to reflect this architecture
4. **Stage 2 becomes**: "Does weavec1 compile itself?" (yes/no check)
5. **Stage 1 becomes**: "Does the compiler work correctly?" (comprehensive testing)

## Why This Matters

- **Development workflow**: Developers work on stage1, so tests should be there
- **Clarity**: Stage2 is proof of concept, not a development target
- **Maintenance**: Easier to understand what tests what
- **CI/CD**: Stage1 tests validate compiler correctness, Stage2 just verifies self-hosting

## Next Steps

1. Move all comprehensive tests from `stage2/tests/` to `stage1/tests/`
2. Keep only smoke tests in `stage2/tests/`
3. Update documentation to reflect this architecture
4. Update CMakeLists.txt accordingly

