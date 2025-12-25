#include "llvm_compile.h"

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Support.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

