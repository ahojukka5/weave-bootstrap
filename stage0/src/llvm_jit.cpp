// LLVM JIT Compilation using ORC JIT (C++ implementation, C interface)
// This file uses C++ to access LLVM's ORC JIT API

#include "llvm_compile.h"

#ifdef USE_LLVM_API

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>

using namespace llvm;
using namespace llvm::orc;

// JIT Session wrapper
struct JITSession {
    std::unique_ptr<LLJIT> jit;
    ThreadSafeContext tsc;
    
    JITSession() : tsc(std::make_unique<LLVMContext>()) {
        // Initialize LLVM targets
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
        
        // Create JIT
        auto jit_or_error = LLJITBuilder().create();
        if (jit_or_error) {
            jit = std::move(*jit_or_error);
        }
    }
    
    ~JITSession() {
        // Cleanup handled by unique_ptr
    }
};

extern "C" {

LLVMJITSessionRef llvm_jit_create_session(void) {
    try {
        auto session = new JITSession();
        if (!session->jit) {
            delete session;
            return NULL;
        }
        return reinterpret_cast<LLVMJITSessionRef>(session);
    } catch (...) {
        return NULL;
    }
}

int llvm_jit_add_module(LLVMJITSessionRef session_ref, const char *ir_string, size_t ir_len) {
    if (!session_ref || !ir_string) {
        return 1;
    }
    
    try {
        JITSession *session = reinterpret_cast<JITSession*>(session_ref);
        
        // Create memory buffer from IR string
        auto mem_buf = MemoryBuffer::getMemBufferCopy(
            StringRef(ir_string, ir_len), "jit_module");
        
        // Parse IR - create a new context for this module
        auto ctx = std::make_unique<LLVMContext>();
        SMDiagnostic err;
        auto module = parseIR(mem_buf->getMemBufferRef(), err, *ctx);
        if (!module) {
            return 1;
        }
        
        // Create thread-safe module with the context
        ThreadSafeModule tsm(std::move(module), ThreadSafeContext(std::move(ctx)));
        
        // Add module to JIT
        if (auto err = session->jit->addIRModule(std::move(tsm))) {
            return 1;
        }
        
        return 0;
    } catch (...) {
        return 1;
    }
}

void* llvm_jit_lookup_function(LLVMJITSessionRef session_ref, const char *function_name) {
    if (!session_ref || !function_name) {
        return NULL;
    }
    
    try {
        JITSession *session = reinterpret_cast<JITSession*>(session_ref);
        
        // Look up symbol
        auto sym = session->jit->lookup(function_name);
        if (!sym) {
            return NULL;
        }
        
        // Get address - in newer LLVM, ExecutorAddr can be converted directly
        return reinterpret_cast<void*>(sym->getValue());
    } catch (...) {
        return NULL;
    }
}

void llvm_jit_dispose_session(LLVMJITSessionRef session_ref) {
    if (session_ref) {
        delete reinterpret_cast<JITSession*>(session_ref);
    }
}

void* llvm_jit_compile_and_lookup(const char *ir_string, size_t ir_len, const char *function_name) {
    LLVMJITSessionRef session = llvm_jit_create_session();
    if (!session) {
        return NULL;
    }
    
    if (llvm_jit_add_module(session, ir_string, ir_len) != 0) {
        llvm_jit_dispose_session(session);
        return NULL;
    }
    
    void *func_ptr = llvm_jit_lookup_function(session, function_name);
    llvm_jit_dispose_session(session);
    
    return func_ptr;
}

} // extern "C"

#else

// Stub implementations when LLVM is not available
extern "C" {

LLVMJITSessionRef llvm_jit_create_session(void) { return NULL; }
int llvm_jit_add_module(LLVMJITSessionRef, const char *, size_t) { return 1; }
void* llvm_jit_lookup_function(LLVMJITSessionRef, const char *) { return NULL; }
void llvm_jit_dispose_session(LLVMJITSessionRef) {}
void* llvm_jit_compile_and_lookup(const char *, size_t, const char *) { return NULL; }

} // extern "C"

#endif // USE_LLVM_API

