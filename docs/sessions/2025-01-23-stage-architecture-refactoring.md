# Stage Architecture Refactoring

## Summary

The stage architecture has been clarified and documented. The test organization now correctly reflects the purpose of each stage.

## Current State

### Stage 1: Development Target
- **Tests**: 88 comprehensive tests in `stage1/tests/`
- **Purpose**: Main development target for compiler features
- **Includes**: All language features, scientific computing examples, edge cases, typecheck tests

### Stage 2: Self-Hosting Verification
- **Tests**: 3 minimal smoke tests in `stage2/tests/`
  - `test_smoke_exit_42.weave` - Basic compilation works
  - `test_fn_simple_42.weave` - Simple function compilation
  - `test_let_add_42.weave` - Basic variable operations
- **Purpose**: Proof of concept that `weavec1` can compile itself

## Architectural Rule

A new rule has been created in `.cursor/rules/stage-architecture.mdc` that documents:

1. **Stage 0**: C bootstrap compiler (foundation)
2. **Stage 1**: Weave compiler (development target) + ALL comprehensive tests
3. **Stage 2**: Self-hosting proof of concept + minimal smoke tests only

## Key Principles

1. **Development happens in Stage 1**: All compiler features are implemented in `stage1/src/*.weave`
2. **Comprehensive tests belong in Stage 1**: All tests that validate compiler correctness go in `stage1/tests/`
3. **Stage 2 is proof of concept**: Only minimal smoke tests to verify self-hosting works
4. **No separate Stage 2 source**: Stage 2 uses the same source as Stage 1, compiled by Stage 1

## CMakeLists.txt Configuration

Both `stage1/CMakeLists.txt` and `stage2/CMakeLists.txt` are correctly configured:

- **Stage 1**: Discovers all `test_*.weave` files automatically
- **Stage 2**: Explicitly lists only 3 minimal smoke tests

## Benefits

1. **Clear development workflow**: Developers work on Stage 1, tests are there
2. **Proper test organization**: Comprehensive tests validate the compiler being developed
3. **Minimal Stage 2**: Self-hosting verification doesn't need comprehensive tests
4. **Better CI/CD**: Stage 1 tests validate correctness, Stage 2 just verifies self-hosting

## Files Created/Modified

- ✅ Created `.cursor/rules/stage-architecture.mdc` - Architectural rule for Cursor
- ✅ Verified `stage1/CMakeLists.txt` - Correctly discovers all tests
- ✅ Verified `stage2/CMakeLists.txt` - Correctly lists only smoke tests
- ✅ Verified test distribution - 88 tests in Stage 1, 3 in Stage 2

## Next Steps

The architecture is now properly documented and organized. Future development should:

1. Add all new comprehensive tests to `stage1/tests/`
2. Only add minimal smoke tests to `stage2/tests/` if needed for self-hosting verification
3. Reference `.cursor/rules/stage-architecture.mdc` when in doubt about test placement

