# LLVM Direct Integration Analysis

## Current State

Currently, the Weave compiler generates LLVM IR as text and then invokes `clang` via system calls to compile it:

1. **Stage0 (C)**: Uses `fork()` + `execvp()` to call clang directly
   - Location: `stage0/src/main.c` lines 237-308
   - Writes IR to temp file, then execs clang with appropriate flags

2. **Stage1 (Weave)**: Uses `ccall "system"` to invoke clang via shell
   - Location: `stage1/src/weavec1.weave` lines 162-191
   - Constructs command strings and calls system()

## What Direct LLVM Integration Would Provide

### Benefits
1. **JIT Compilation**: Execute code immediately without writing files
2. **Better Error Handling**: Direct access to LLVM diagnostics
3. **No External Dependencies**: No need for clang in PATH
4. **Faster Compilation**: Avoid process spawning overhead
5. **Advanced Features**: 
   - Runtime optimization passes
   - Custom optimization pipelines
   - Profile-guided optimization
   - Link-time optimization (LTO)
6. **Better Integration**: Direct control over compilation process

### Challenges
1. **Build Complexity**: LLVM is a large dependency
2. **API Complexity**: LLVM has both C and C++ APIs with different trade-offs
3. **Linking**: Still need a linker for executables (or use lld)
4. **Maintenance**: LLVM API changes between versions

## Implementation Approaches

### Option 1: LLVM C API (Recommended for Stage0)

**Pros:**
- Works with C codebase (stage0)
- Stable API
- Simpler than C++ API
- Can be called from Weave via ccall

**Cons:**
- Less feature-rich than C++ API
- Some advanced features not available
- More verbose for complex operations

**Required Libraries:**
- `libLLVM` (or individual components)
- LLVM C API headers

**Key Functions Needed:**
```c
// Parse LLVM IR from string
LLVMMemoryBufferRef LLVMCreateMemoryBufferWithMemoryRangeCopy(const char *data, size_t length, const char *name);
LLVMModuleRef LLVMParseIRInContext(LLVMContextRef context, LLVMMemoryBufferRef mem_buf, char **out_message, LLVMBool *out_error);

// Create target machine
LLVMTargetRef target;
LLVMTargetMachineRef target_machine;
LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &error);
target_machine = LLVMCreateTargetMachine(target, triple, cpu, features, opt_level, reloc, code_model);

// Emit object file
LLVMTargetMachineEmitToFile(target_machine, module, filename, LLVMObjectFile, &error);

// JIT compilation (requires ORC JIT)
// More complex, requires ORC JIT infrastructure
```

### Option 2: LLVM C++ API (Better for Future)

**Pros:**
- Full feature set
- Better JIT support (ORC JIT)
- More modern API
- Better documentation

**Cons:**
- Requires C++ code (would need C++ bridge for stage0)
- More complex
- Larger dependency footprint

### Option 3: Hybrid Approach

- Use C API for basic compilation (object files, assembly)
- Use C++ API for JIT (via separate C++ module)
- Bridge between C and C++ via C interface

## Required Dependencies

### Build System Changes

**CMake Configuration:**
```cmake
# Find LLVM
find_package(LLVM REQUIRED CONFIG)
llvm_map_components_to_libnames(LLVM_LIBS
  core
  executionengine
  orcjit
  native
  mc
  mcparser
  support
  target
  transformutils
  analysis
  bitwriter
  linker
)

# For C API
target_link_libraries(weavec0 ${LLVM_LIBS})
target_include_directories(weavec0 PRIVATE ${LLVM_INCLUDE_DIRS})
```

### Installation Requirements

**Option A: System LLVM**
- Requires LLVM development packages installed
- `apt-get install llvm-dev` (Debian/Ubuntu)
- `brew install llvm` (macOS)
- `yum install llvm-devel` (RHEL/CentOS)

**Option B: Bundled LLVM**
- Download and build LLVM as part of project
- More control over version
- Larger build time
- Better for reproducible builds

**Option C: LLVM via vcpkg/Conan**
- Package manager integration
- Easier dependency management

## Implementation Steps

### Phase 1: Basic Object File Generation (Replace clang -c)

1. **Add LLVM dependency to CMake**
   - Find LLVM package
   - Link required libraries
   - Add include directories

2. **Create LLVM wrapper module** (`stage0/src/llvm_compile.c`)
   ```c
   // Functions:
   // - llvm_compile_ir_to_object(ir_string, output_path, opt_level)
   // - llvm_compile_ir_to_assembly(ir_string, output_path, opt_level)
   ```

3. **Replace clang invocation in stage0**
   - Replace `fork()/execvp()` with direct LLVM calls
   - Parse IR from string instead of temp file
   - Emit object file directly

4. **Update stage1**
   - Add ccall wrappers for LLVM functions
   - Replace `system()` calls with LLVM ccall

### Phase 2: Executable Linking

**Option A: Use system linker (ld)**
- Still need to call linker, but can use LLVM for compilation
- Simpler, but still external dependency

**Option B: Use lld (LLVM's linker)**
- Full LLVM stack
- Can be embedded or called programmatically
- Better integration

### Phase 3: JIT Compilation

1. **Set up ORC JIT infrastructure**
   - Create JIT execution session
   - Add LLVM module to JIT
   - Get function pointer
   - Execute directly

2. **Add JIT mode to compiler**
   - New flag: `--jit` or `--run`
   - Compile and execute immediately
   - Useful for REPL/interactive mode

3. **Runtime optimization**
   - Apply optimization passes at runtime
   - Profile-guided optimization

## Code Structure

### New Files Needed

```
stage0/
  src/
    llvm_compile.c      # LLVM compilation wrapper
    llvm_compile.h       # Header for LLVM functions
  include/
    llvm_compile.h       # Public API
```

### Modified Files

```
stage0/
  src/
    main.c              # Replace clang exec with LLVM calls
  CMakeLists.txt        # Add LLVM dependency
stage1/
  src/
    weavec1.weave        # Replace system() with LLVM ccall
```

## Example Implementation (C API)

### Basic Object File Generation

```c
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>

int llvm_compile_ir_to_object(const char *ir_string, size_t ir_len, 
                               const char *output_path, int opt_level) {
    char *error = NULL;
    
    // Initialize LLVM
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    
    // Create context and parse IR
    LLVMContextRef context = LLVMContextCreate();
    LLVMMemoryBufferRef mem_buf = LLVMCreateMemoryBufferWithMemoryRangeCopy(
        ir_string, ir_len, "weave_module");
    
    LLVMModuleRef module;
    if (LLVMParseIRInContext(context, mem_buf, &error, NULL) != 0) {
        fprintf(stderr, "Failed to parse IR: %s\n", error);
        LLVMDisposeMessage(error);
        return 1;
    }
    
    // Get target machine
    char *triple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        fprintf(stderr, "Failed to get target: %s\n", error);
        return 1;
    }
    
    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
        target, triple, "generic", "", 
        opt_level == 0 ? LLVMCodeGenLevelNone :
        opt_level == 1 ? LLVMCodeGenLevelLess :
        opt_level == 2 ? LLVMCodeGenLevelDefault :
        LLVMCodeGenLevelAggressive,
        LLVMRelocDefault, LLVMCodeModelDefault);
    
    // Emit object file
    if (LLVMTargetMachineEmitToFile(target_machine, module, output_path, 
                                     LLVMObjectFile, &error) != 0) {
        fprintf(stderr, "Failed to emit object file: %s\n", error);
        LLVMDisposeMessage(error);
        return 1;
    }
    
    // Cleanup
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeModule(module);
    LLVMDisposeMemoryBuffer(mem_buf);
    LLVMContextDispose(context);
    
    return 0;
}
```

### JIT Compilation (More Complex)

Requires ORC JIT which is primarily C++ API. Would need C++ bridge or use C API's simpler (but deprecated) MCJIT.

## Migration Path

### Step 1: Proof of Concept
- Add LLVM dependency to build
- Create minimal `llvm_compile_ir_to_object()` function
- Test with simple IR string
- Replace one clang call in stage0

### Step 2: Full Object File Support
- Support all optimization levels
- Support assembly output (-S)
- Handle errors properly
- Update stage1 to use it

### Step 3: Linking
- Integrate lld or keep using system linker
- Support static linking
- Support runtime library linking

### Step 4: JIT (Optional)
- Add JIT infrastructure
- Implement --jit flag
- Add REPL mode

## Testing Strategy

1. **Unit Tests**: Test LLVM wrapper functions with known IR
2. **Integration Tests**: Ensure existing tests still pass
3. **Performance Tests**: Compare with clang invocation
4. **Cross-platform**: Test on different targets

## Estimated Effort

- **Phase 1 (Object files)**: 2-3 days
- **Phase 2 (Linking)**: 1-2 days  
- **Phase 3 (JIT)**: 3-5 days
- **Testing & Polish**: 2-3 days

**Total**: ~1-2 weeks for full implementation

## Recommendations

1. **Start with C API** for stage0 compatibility
2. **Implement object file generation first** (most immediate benefit)
3. **Keep linker external initially** (simpler, can optimize later)
4. **Add JIT as optional feature** (nice-to-have, not critical)
5. **Version pin LLVM** for reproducible builds
6. **Consider LLVM version compatibility** (support multiple versions or pin to one)

## Alternative: Incremental Approach

Instead of full replacement, could:
1. Add `--use-llvm-api` flag to opt-in
2. Keep clang as fallback
3. Gradually migrate features
4. Remove clang dependency once stable

This reduces risk and allows gradual migration.

