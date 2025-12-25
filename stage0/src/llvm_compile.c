#include "llvm_compile.h"

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Support.h>

/* Optimization pass headers - may not be available in all LLVM versions */
#ifdef __has_include
  #if __has_include(<llvm-c/Transforms/Scalar.h>)
    #include <llvm-c/Transforms/Scalar.h>
    #define HAVE_LLVM_SCALAR_PASSES 1
  #endif
  #if __has_include(<llvm-c/Transforms/IPO.h>)
    #include <llvm-c/Transforms/IPO.h>
    #define HAVE_LLVM_IPO_PASSES 1
  #endif
  #if __has_include(<llvm-c/Transforms/Vectorize.h>)
    #include <llvm-c/Transforms/Vectorize.h>
    #define HAVE_LLVM_VECTORIZE_PASSES 1
  #endif
#else
  /* Fallback for compilers without __has_include */
  #include <llvm-c/Transforms/Scalar.h>
  #include <llvm-c/Transforms/IPO.h>
  #include <llvm-c/Transforms/Vectorize.h>
  #define HAVE_LLVM_SCALAR_PASSES 1
  #define HAVE_LLVM_IPO_PASSES 1
  #define HAVE_LLVM_VECTORIZE_PASSES 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/wait.h>

/* Convert optimization level to LLVM codegen level */
static LLVMCodeGenOptLevel get_opt_level(int opt_level) {
    switch (opt_level) {
        case 0: return LLVMCodeGenLevelNone;
        case 1: return LLVMCodeGenLevelLess;
        case 2: return LLVMCodeGenLevelDefault;
        case 3: return LLVMCodeGenLevelAggressive;
        default: return LLVMCodeGenLevelDefault;
    }
}

/* Initialize LLVM targets (idempotent, safe to call multiple times) */
static void init_llvm_targets(void) {
    static int initialized = 0;
    if (initialized) return;
    
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    
    initialized = 1;
}

/* Run optimization passes on a module based on optimization level.
 * Returns 0 on success, non-zero on error.
 * Note: Some LLVM versions may not have all pass functions available.
 * This function uses the legacy pass manager API which is widely supported.
 */
static int run_optimization_passes(LLVMModuleRef module, int opt_level) {
    if (opt_level == 0) {
        /* -O0: No optimizations */
        return 0;
    }
    
    LLVMPassManagerRef pm = LLVMCreatePassManager();
    if (!pm) {
        fprintf(stderr, "weavec: failed to create pass manager\n");
        return 1;
    }
    
    /* Add standard optimization passes based on level */
    /* Note: These functions may not be available in all LLVM versions.
     * If compilation fails, some passes may need to be conditionally compiled.
     */
    if (opt_level >= 1) {
        /* -O1: Basic optimizations */
        LLVMAddConstantPropagationPass(pm);
        LLVMAddInstructionCombiningPass(pm);
        LLVMAddReassociatePass(pm);
        LLVMAddGVNPass(pm);
        LLVMAddCFGSimplificationPass(pm);
        LLVMAddPromoteMemoryToRegisterPass(pm);
    }
    
    if (opt_level >= 2) {
        /* -O2: Standard optimizations (includes O1 passes) */
        LLVMAddDeadStoreEliminationPass(pm);
        LLVMAddAggressiveDCEPass(pm);
        LLVMAddFunctionInliningPass(pm);
        LLVMAddLoopUnrollPass(pm);
        LLVMAddLoopUnswitchPass(pm);
        LLVMAddLICMPass(pm);
        LLVMAddLoopDeletionPass(pm);
        LLVMAddLoopIdiomPass(pm);
        LLVMAddIndVarSimplifyPass(pm);
        LLVMAddLoopRotatePass(pm);
    }
    
    if (opt_level >= 3) {
        /* -O3: Aggressive optimizations (includes O2 passes) */
        LLVMAddArgumentPromotionPass(pm);
        LLVMAddFunctionAttrsPass(pm);
        LLVMAddTailCallEliminationPass(pm);
#ifdef HAVE_LLVM_VECTORIZE_PASSES
        LLVMAddLoopVectorizePass(pm);
        LLVMAddSLPVectorizePass(pm);
#endif
    }
    
    /* Run passes on the module */
    LLVMRunPassManager(pm, module);
    
    /* Clean up */
    LLVMDisposePassManager(pm);
    
    return 0;
}

int llvm_compile_ir_to_object_internal(const char *ir_string, size_t ir_len,
                                        const char *output_path, int opt_level) {
    return llvm_compile_ir_to_object_asan(ir_string, ir_len, output_path, opt_level, 0);
}

int llvm_compile_ir_to_object_asan(const char *ir_string, size_t ir_len,
                                    const char *output_path, int opt_level, int use_asan) {
    char *error = NULL;
    LLVMContextRef context = NULL;
    LLVMMemoryBufferRef mem_buf = NULL;
    LLVMModuleRef module = NULL;
    LLVMTargetMachineRef target_machine = NULL;
    int result = 1;
    
    /* Initialize LLVM targets */
    init_llvm_targets();
    
    /* Create context */
    context = LLVMContextCreate();
    if (!context) {
        fprintf(stderr, "weavec: failed to create LLVM context\n");
        goto cleanup;
    }
    
    /* Create memory buffer from IR string */
    mem_buf = LLVMCreateMemoryBufferWithMemoryRangeCopy(
        ir_string, ir_len, "weave_module");
    if (!mem_buf) {
        fprintf(stderr, "weavec: failed to create memory buffer\n");
        goto cleanup;
    }
    
    /* Parse IR - LLVMParseIRInContext returns 0 on success */
    if (LLVMParseIRInContext(context, mem_buf, &module, &error) != 0) {
        if (error) {
            fprintf(stderr, "weavec: failed to parse LLVM IR: %s\n", error);
            LLVMDisposeMessage(error);
        } else {
            fprintf(stderr, "weavec: failed to parse LLVM IR\n");
        }
        goto cleanup;
    }
    
    if (!module) {
        fprintf(stderr, "weavec: failed to get module from parsed IR\n");
        goto cleanup;
    }
    
    /* Get target triple */
    char *triple = LLVMGetDefaultTargetTriple();
    if (!triple) {
        fprintf(stderr, "weavec: failed to get target triple\n");
        goto cleanup;
    }
    
    /* Get target */
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        if (error) {
            fprintf(stderr, "weavec: failed to get target: %s\n", error);
            LLVMDisposeMessage(error);
        } else {
            fprintf(stderr, "weavec: failed to get target for triple: %s\n", triple);
        }
        LLVMDisposeMessage(triple);
        goto cleanup;
    }
    
    /* Create target machine */
    target_machine = LLVMCreateTargetMachine(
        target,
        triple,
        "generic",  /* CPU */
        "",         /* Features */
        get_opt_level(opt_level),
        LLVMRelocDefault,
        LLVMCodeModelDefault);
    
    LLVMDisposeMessage(triple);
    
    if (!target_machine) {
        fprintf(stderr, "weavec: failed to create target machine\n");
        goto cleanup;
    }
    
    /* Run optimization passes before codegen if optimization level > 0 */
    if (opt_level > 0) {
        if (run_optimization_passes(module, opt_level) != 0) {
            fprintf(stderr, "weavec: warning: optimization passes failed, continuing without optimizations\n");
        }
    }
    
    /* Emit object file */
    if (LLVMTargetMachineEmitToFile(target_machine, module, output_path,
                                     LLVMObjectFile, &error) != 0) {
        if (error) {
            fprintf(stderr, "weavec: failed to emit object file: %s\n", error);
            LLVMDisposeMessage(error);
        } else {
            fprintf(stderr, "weavec: failed to emit object file\n");
        }
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (target_machine) {
        LLVMDisposeTargetMachine(target_machine);
    }
    /* Dispose module first - it may own the memory buffer */
    if (module) {
        LLVMDisposeModule(module);
        /* Module disposal may have disposed the memory buffer,
         * so we don't dispose it separately */
        mem_buf = NULL;
    }
    /* Only dispose memory buffer if module wasn't created */
    if (mem_buf) {
        LLVMDisposeMemoryBuffer(mem_buf);
    }
    if (context) {
        LLVMContextDispose(context);
    }
    
    return result;
}

/* Link object files into an executable using clang as linker.
 * This replaces system() calls with a more controlled fork/exec approach.
 * Note: For full LLVM integration, this could use lld instead of clang.
 */
int llvm_link_objects(const char *object_files, const char *extra_flags, const char *output_path) {
    pid_t pid;
    int status;
    int result = 1;
    
    /* Build command string */
    size_t cmd_size = 64; /* "clang " */
    if (extra_flags) cmd_size += strlen(extra_flags);
    if (output_path) cmd_size += strlen(output_path) + 4; /* " -o " */
    if (object_files) cmd_size += strlen(object_files);
    
    char *cmd = (char *)malloc(cmd_size);
    if (!cmd) {
        fprintf(stderr, "weavec: failed to allocate memory for link command\n");
        return 1;
    }
    
    /* Construct command string */
    strcpy(cmd, "clang");
    if (extra_flags && strlen(extra_flags) > 0) {
        strcat(cmd, " ");
        strcat(cmd, extra_flags);
    }
    if (output_path && strlen(output_path) > 0) {
        strcat(cmd, " -o ");
        strcat(cmd, output_path);
    }
    if (object_files && strlen(object_files) > 0) {
        strcat(cmd, " ");
        strcat(cmd, object_files);
    }
    
    /* Fork and execute via sh -c for proper argument handling */
    pid = fork();
    if (pid == 0) {
        /* Child process */
        execlp("sh", "sh", "-c", cmd, NULL);
        fprintf(stderr, "weavec: failed to execute linker command\n");
        exit(1);
    } else if (pid > 0) {
        /* Parent process: wait for child */
        if (waitpid(pid, &status, 0) == pid) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                result = 0;
            } else {
                fprintf(stderr, "weavec: linker failed with exit code %d\n", 
                        WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            }
        } else {
            fprintf(stderr, "weavec: failed to wait for linker process\n");
        }
    } else {
        /* Fork failed */
        fprintf(stderr, "weavec: failed to fork linker process\n");
    }
    
    free(cmd);
    return result;
}

int llvm_compile_ir_to_assembly_internal(const char *ir_string, size_t ir_len,
                                          const char *output_path, int opt_level) {
    char *error = NULL;
    LLVMContextRef context = NULL;
    LLVMMemoryBufferRef mem_buf = NULL;
    LLVMModuleRef module = NULL;
    LLVMTargetMachineRef target_machine = NULL;
    int result = 1;
    
    /* Initialize LLVM targets */
    init_llvm_targets();
    
    /* Create context */
    context = LLVMContextCreate();
    if (!context) {
        fprintf(stderr, "weavec: failed to create LLVM context\n");
        goto cleanup;
    }
    
    /* Create memory buffer from IR string */
    mem_buf = LLVMCreateMemoryBufferWithMemoryRangeCopy(
        ir_string, ir_len, "weave_module");
    if (!mem_buf) {
        fprintf(stderr, "weavec: failed to create memory buffer\n");
        goto cleanup;
    }
    
    /* Parse IR - LLVMParseIRInContext returns 0 on success */
    if (LLVMParseIRInContext(context, mem_buf, &module, &error) != 0) {
        if (error) {
            fprintf(stderr, "weavec: failed to parse LLVM IR: %s\n", error);
            LLVMDisposeMessage(error);
        } else {
            fprintf(stderr, "weavec: failed to parse LLVM IR\n");
        }
        goto cleanup;
    }
    
    if (!module) {
        fprintf(stderr, "weavec: failed to get module from parsed IR\n");
        goto cleanup;
    }
    
    /* Get target triple */
    char *triple = LLVMGetDefaultTargetTriple();
    if (!triple) {
        fprintf(stderr, "weavec: failed to get target triple\n");
        goto cleanup;
    }
    
    /* Get target */
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        if (error) {
            fprintf(stderr, "weavec: failed to get target: %s\n", error);
            LLVMDisposeMessage(error);
        } else {
            fprintf(stderr, "weavec: failed to get target for triple: %s\n", triple);
        }
        LLVMDisposeMessage(triple);
        goto cleanup;
    }
    
    /* Create target machine */
    target_machine = LLVMCreateTargetMachine(
        target,
        triple,
        "generic",
        "",
        get_opt_level(opt_level),
        LLVMRelocDefault,
        LLVMCodeModelDefault);
    
    LLVMDisposeMessage(triple);
    
    if (!target_machine) {
        fprintf(stderr, "weavec: failed to create target machine\n");
        goto cleanup;
    }
    
    /* Run optimization passes before codegen if optimization level > 0 */
    if (opt_level > 0) {
        if (run_optimization_passes(module, opt_level) != 0) {
            fprintf(stderr, "weavec: warning: optimization passes failed, continuing without optimizations\n");
        }
    }
    
    /* Emit assembly file */
    if (LLVMTargetMachineEmitToFile(target_machine, module, output_path,
                                     LLVMAssemblyFile, &error) != 0) {
        if (error) {
            fprintf(stderr, "weavec: failed to emit assembly file: %s\n", error);
            LLVMDisposeMessage(error);
        } else {
            fprintf(stderr, "weavec: failed to emit assembly file\n");
        }
        goto cleanup;
    }
    
    result = 0;
    
cleanup:
    if (target_machine) {
        LLVMDisposeTargetMachine(target_machine);
    }
    /* Dispose module first - it may own the memory buffer */
    if (module) {
        LLVMDisposeModule(module);
        /* Module disposal may have disposed the memory buffer,
         * so we don't dispose it separately */
        mem_buf = NULL;
    }
    /* Only dispose memory buffer if module wasn't created */
    if (mem_buf) {
        LLVMDisposeMemoryBuffer(mem_buf);
    }
    if (context) {
        LLVMContextDispose(context);
    }
    
    return result;
}

/* Link object files into an executable using clang as linker.
 * This replaces system() calls with a more controlled fork/exec approach.
 * Note: For full LLVM integration, this could use lld instead of clang.
 */
int llvm_link_objects(const char *object_files, const char *extra_flags, const char *output_path) {
    pid_t pid;
    int status;
    int result = 1;
    
    /* Build command string */
    size_t cmd_size = 64; /* "clang " */
    if (extra_flags) cmd_size += strlen(extra_flags);
    if (output_path) cmd_size += strlen(output_path) + 4; /* " -o " */
    if (object_files) cmd_size += strlen(object_files);
    
    char *cmd = (char *)malloc(cmd_size);
    if (!cmd) {
        fprintf(stderr, "weavec: failed to allocate memory for link command\n");
        return 1;
    }
    
    /* Construct command string */
    strcpy(cmd, "clang");
    if (extra_flags && strlen(extra_flags) > 0) {
        strcat(cmd, " ");
        strcat(cmd, extra_flags);
    }
    if (output_path && strlen(output_path) > 0) {
        strcat(cmd, " -o ");
        strcat(cmd, output_path);
    }
    if (object_files && strlen(object_files) > 0) {
        strcat(cmd, " ");
        strcat(cmd, object_files);
    }
    
    /* Fork and execute via sh -c for proper argument handling */
    pid = fork();
    if (pid == 0) {
        /* Child process */
        execlp("sh", "sh", "-c", cmd, NULL);
        fprintf(stderr, "weavec: failed to execute linker command\n");
        exit(1);
    } else if (pid > 0) {
        /* Parent process: wait for child */
        if (waitpid(pid, &status, 0) == pid) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                result = 0;
            } else {
                fprintf(stderr, "weavec: linker failed with exit code %d\n", 
                        WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            }
        } else {
            fprintf(stderr, "weavec: failed to wait for linker process\n");
        }
    } else {
        /* Fork failed */
        fprintf(stderr, "weavec: failed to fork linker process\n");
    }
    
    free(cmd);
    return result;
}

