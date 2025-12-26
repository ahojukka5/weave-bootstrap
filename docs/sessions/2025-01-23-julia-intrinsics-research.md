# Research: How Julia Handles Builtins/Intrinsics

## Julia's Intrinsic System

Based on Julia's architecture, here's how they handle low-level operations:

### 1. Intrinsics Registry

Julia has a registry of intrinsics defined in `src/intrinsics.cpp` (or similar). Each intrinsic has:
- A unique ID (`jl_intrinsic_t` enum)
- A name
- Type signature information
- Code generation behavior

### 2. Base.Intrinsics Module

Julia exposes intrinsics through `Base.Intrinsics` module, which provides:
- Type-safe wrappers
- Documentation
- Type checking

### 3. Code Generation

In Julia's compiler:
- Intrinsics are recognized during parsing/type inference
- They map directly to LLVM intrinsics or specific IR patterns
- Type information drives code generation

### 4. Example Structure (Hypothetical)

```cpp
// Julia's approach (simplified)
typedef enum {
    JL_INT_ADD_PTR,      // Pointer addition
    JL_INT_GETFIELD,     // Field access
    // ... many more
} jl_intrinsic_t;

typedef struct {
    jl_intrinsic_t id;
    const char *name;
    jl_value_t *sig;  // Type signature
    // Code generation info
} jl_intrinsic_def;
```

### 5. Key Differences from Our Approach

**Julia:**
- Intrinsics are part of the type system
- Multiple dispatch can select different implementations
- Type inference propagates through intrinsics
- Intrinsics are first-class in the language

**Our Current Approach:**
- Hardcoded checks in expression compiler
- No type-driven dispatch
- Limited extensibility

## What We Should Learn

1. **Registry-Based**: All intrinsics in one place
2. **Type-Driven**: Type signatures drive code generation
3. **Extensible**: Easy to add new intrinsics
4. **First-Class**: Intrinsics are part of the language, not compiler internals

## Next Steps

To properly study Julia's implementation:
1. Clone Julia repository: `git clone https://github.com/JuliaLang/julia.git`
2. Look at `src/intrinsics.cpp` or `src/intrinsics.h`
3. Study `base/intrinsics.jl` for the user-facing API
4. Examine how type inference handles intrinsics

