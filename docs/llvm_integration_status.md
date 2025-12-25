# LLVM Integration Status

## Implementation Progress

### Automatic LLVM Download and Build ✅

**Completed:**
- ✅ Added `WEAVE_AUTO_BUILD_LLVM` CMake option
- ✅ Implemented ExternalProject to download and build LLVM automatically
- ✅ Configured minimal LLVM build (C API only, no tests/docs)
- ✅ Added proper dependency handling
- ✅ Created documentation

**Usage:**
```bash
cmake -DWEAVE_AUTO_BUILD_LLVM=ON ..
cmake --build . --target llvm_external  # Takes 30-60+ minutes
cmake ..  # Reconfigure to find built LLVM
cmake --build .  # Build project
```

See `docs/llvm_auto_build.md` for detailed instructions.

## Implementation Progress

### Phase 1: Basic Object File Generation ✅ (Partially Complete)

**Completed:**
- ✅ Added LLVM dependency detection in CMake (optional, falls back to clang)
- ✅ Created LLVM wrapper module (`llvm_compile.c/h`)
- ✅ Updated `main.c` to conditionally use LLVM API or clang fallback
- ✅ Build system works with and without LLVM installed

**Remaining:**
- ⚠️ LLVM API function verification needed (when LLVM is installed)
- ⚠️ Testing with actual LLVM installation
- ⚠️ Address sanitizer support in LLVM API (currently stubbed)

### Current State

The codebase now supports **both** LLVM direct API and clang fallback:

1. **With LLVM installed**: Uses LLVM C API directly to compile IR to object files
2. **Without LLVM**: Falls back to existing clang system calls

### How to Enable LLVM Integration

1. **Install LLVM development packages:**
   ```bash
   # macOS
   brew install llvm
   
   # Ubuntu/Debian
   sudo apt-get install llvm-dev
   
   # Or build from source
   ```

2. **Configure CMake with LLVM:**
   ```bash
   cmake -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm ..
   ```

3. **Build:**
   ```bash
   make
   ```

The build system will automatically detect LLVM and use it if available.

### Known Issues

1. **LLVM API Function**: The code uses `LLVMParseIRInContext` which may need verification against the actual LLVM version installed. The exact API signature may vary.

2. **Address Sanitizer**: The `llvm_compile_ir_to_object_asan` function is implemented but the ASAN flags may need to be passed differently in LLVM API vs clang.

3. **Linking**: For executables, we still use clang as a linker. Full LLVM integration would use `lld`.

### Next Steps

1. **Test with LLVM installed** to verify API correctness
2. **Fix any API compatibility issues** based on LLVM version
3. **Add assembly output support** (`-S` flag) using LLVM API
4. **Phase 2**: Integrate lld for full LLVM linking
5. **Phase 3**: Add JIT compilation support

### Files Modified

- `stage0/CMakeLists.txt` - Added LLVM detection and conditional linking
- `stage0/include/llvm_compile.h` - LLVM compilation API header
- `stage0/src/llvm_compile.c` - LLVM compilation implementation
- `stage0/src/main.c` - Updated to use LLVM API conditionally

### Testing

To test the LLVM integration:

```bash
# Build with LLVM (if installed)
cd build-llvm
cmake ..
make

# Test object file generation
./weavec0 test.weave -c -o test.o

# Test executable (still uses clang for linking)
./weavec0 test.weave -o test
```

