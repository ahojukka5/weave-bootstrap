# Test Integration for Scientific Computing Examples

## Summary

All scientific computing test programs are automatically integrated into the CMake build system and will be compiled and tested when running `cmake --build build`.

## Automatic Test Discovery

The build system uses CMake's `file(GLOB)` to automatically discover all test files:

```cmake
file(GLOB STAGE2_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.weave)
```

This means **all** files matching `test_*.weave` in `stage2/tests/` are automatically:
1. **Compiled** to LLVM IR using `weavec1`
2. **Linked** with clang to create executables
3. **Executed** and verified to return exit code 42

## New Scientific Computing Tests

The following tests have been added and will be automatically included:

1. **test_newton_method_42.weave** - Newton's method for root finding
2. **test_trapezoidal_integration_42.weave** - Numerical integration
3. **test_correlation_42.weave** - Pearson correlation coefficient
4. **test_linear_regression_42.weave** - Linear regression analysis
5. **test_monte_carlo_pi_42.weave** - Monte Carlo π estimation

## Test Execution Flow

For each test file:

1. **Compile**: `weavec1 test_*.weave --emit-llvm -o test_*.ll`
2. **Link**: `clang test_*.ll -lm -o test_*`
3. **Run**: `./test_*`
4. **Verify**: Exit code must be 42

## Running Tests

### Build and Test All
```bash
cmake --build build
ctest --test-dir build
```

### Build and Test Specific Test
```bash
cmake --build build
ctest --test-dir build -R stage2_test_newton_method_42
```

### Build Only (No Test Execution)
```bash
cmake --build build
```

## Test Verification

All tests follow the pattern:
- **Naming**: `test_*_42.weave`
- **Return Value**: Must return 42 (exit code)
- **Structure**: `(program ... (entry main ... (return 42)))`

## Current Test Count

- **Total test files**: 83 (including 5 new scientific computing tests)
- **Auto-discovered**: All `test_*.weave` files
- **Typecheck tests**: Excluded from runtime tests (expected to fail compilation)

## Integration Status

✅ **Automatic Discovery**: Tests are auto-discovered via GLOB
✅ **Compilation**: Tests are compiled during build
✅ **Execution**: Tests are executed and verified via CTest
✅ **Exit Code Check**: All tests must return 42

## Next Steps

The scientific computing tests are now fully integrated. When you run:
```bash
cmake --build build
```

All tests (including the new scientific computing examples) will be:
1. Compiled
2. Linked
3. Executed
4. Verified to return 42

No additional configuration needed - the build system handles everything automatically!

