# High-Impact Improvement Proposals

## 1. **Interactive REPL with JIT** ⭐⭐⭐⭐⭐
**Impact**: Transform development experience - instant feedback loop

**What**: Build an interactive Read-Eval-Print Loop that uses JIT compilation to execute Weave expressions immediately.

**Implementation**:
- Add `weavec0 --repl` mode
- Read Weave expressions from stdin
- Compile each expression to LLVM IR
- JIT compile and execute immediately
- Print results
- Maintain session state (variables, functions)

**Benefits**:
- Instant feedback for testing ideas
- Interactive debugging
- Learning tool for Weave language
- Rapid prototyping

**Example Usage**:
```bash
$ weavec0 --repl
weave> (let x Int32 42)
42
weave> (let add (fn (a Int32) (b Int32) (returns Int32) (+ a b)))
#<function>
weave> (add 10 20)
30
weave> (llvm-jit "define i32 @square(i32 %x) { ret i32 mul i32 %x, %x }" "square" (args 5))
25
```

---

## 2. **Compile Weave Expressions to IR and JIT** ⭐⭐⭐⭐⭐
**Impact**: Make JIT actually useful for Weave code, not just raw LLVM IR

**What**: Extend `llvm-jit` to accept Weave code, compile it to IR, then JIT it.

**Implementation**:
- Add `(llvm-jit-weave ...)` special form
- Accept Weave function definition as S-expression
- Compile to LLVM IR internally
- JIT compile and execute
- Support closures and Weave types

**Benefits**:
- Write JIT code in Weave, not raw LLVM IR
- Type-safe JIT compilation
- Better integration with Weave ecosystem

**Example Usage**:
```weave
(let result Int32 (llvm-jit-weave
  (fn add (params (a Int32) (b Int32)) (returns Int32) (+ a b))
  (args 10 20)))
```

---

## 3. **Migrate Stage1 to LLVM API** ⭐⭐⭐⭐
**Impact**: Complete the LLVM integration, remove system() dependencies

**What**: Replace `ccall "system"` calls in `weavec1.weave` with LLVM API calls via ccall wrappers.

**Current State**: Stage1 still uses `system("clang ...")` calls (3 locations in `weavec1.weave`)

**Implementation**:
- Add LLVM ccall wrappers to stage0 (expose `llvm_compile_ir_to_object` etc.)
- Update stage1 to use LLVM ccalls instead of system()
- Remove clang dependency from stage1
- Faster compilation (no fork/exec overhead)

**Benefits**:
- Faster compilation (no process spawning)
- Better error handling
- Cross-platform (no shell dependency)
- Consistent with stage0 approach

**Files to Modify**:
- `stage0/src/program.c` - Add LLVM ccall declarations
- `stage1/src/weavec1.weave` - Replace system() calls

---

## 4. **LLVM Optimization Passes** ⭐⭐⭐⭐
**Impact**: Significantly better performance for compiled code

**What**: Add LLVM optimization passes (inlining, constant propagation, dead code elimination, etc.)

**Implementation**:
- Use `LLVMRunPassManager` or `LLVMRunFunctionPassManager`
- Add `-O`, `-O2`, `-O3` flags that actually run optimizations
- Currently optimization level is passed to codegen, but no passes are run

**Benefits**:
- Faster generated code
- Smaller binaries
- Better code quality
- Competitive with clang -O2

**Current State**: `llvm_compile.c` has `get_opt_level()` but no pass manager setup

---

## 5. **Fix Type System Issues (42 Failing Tests)** ⭐⭐⭐⭐⭐
**Impact**: Critical for correctness and self-hosting verification

**What**: Fix the 42 failing tests, primarily:
- Invalid LLVM type `program` in IR generation
- Type system issues with nested pointers
- Missing/incorrect function declarations for C functions

**Implementation**:
- Fix IR generation to emit proper types (not `program`)
- Fix pointer type handling in typechecker
- Add proper C function declarations to prelude

**Benefits**:
- 100% test pass rate
- Correct self-hosting
- Production-ready compiler

**Priority**: Very High - blocks full bootstrap verification

---

## 6. **Better Error Messages with Source Locations** ⭐⭐⭐
**Impact**: Much better developer experience

**What**: Improve error messages to show:
- Exact source location (file, line, column)
- Context around error (show code snippet)
- Suggestions for fixes
- Type mismatch details

**Implementation**:
- Enhance `diagnostics.c` with source context
- Add source location tracking through compilation
- Pretty-print type mismatches

**Benefits**:
- Faster debugging
- Better onboarding
- Professional tooling feel

---

## 7. **Hot Reloading for Development** ⭐⭐⭐
**Impact**: Faster iteration during compiler development

**What**: Watch file changes and automatically recompile + JIT execute tests.

**Implementation**:
- File watcher (inotify/FSEvents)
- Auto-recompile on change
- Run affected tests automatically
- Show results in terminal

**Benefits**:
- Instant feedback during development
- Test-driven development workflow
- Reduced context switching

---

## 8. **Expand JIT to Support More Types** ⭐⭐⭐
**Impact**: Make JIT more useful for real code

**What**: Currently JIT only supports `i32 -> i32 -> i32`. Expand to:
- All primitive types (i8, i16, i32, i64, f32, f64)
- Pointers
- Structs
- Variable argument counts
- Return types beyond i32

**Implementation**:
- Generic JIT helper functions
- Type-based dispatch
- Dynamic function signatures

**Benefits**:
- JIT becomes actually useful
- Can JIT compile real Weave functions
- Foundation for REPL

---

## Recommendation Priority

**Immediate (This Week)**:
1. Fix Type System Issues (#5) - Blocks correctness
2. Migrate Stage1 to LLVM API (#3) - Complete integration

**High Value (Next Sprint)**:
3. REPL with JIT (#1) - Transform development experience
4. Compile Weave to IR and JIT (#2) - Make JIT useful

**Performance (When Needed)**:
5. LLVM Optimization Passes (#4) - Better code quality

**Polish (Ongoing)**:
6. Better Error Messages (#6)
7. Hot Reloading (#7)
8. Expand JIT Types (#8)

