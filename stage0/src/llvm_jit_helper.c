// Helper functions for JIT that can be called from Weave via ccall
#include "llvm_compile.h"
#include "common.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* JIT compile LLVM IR and return function pointer as integer.
 * This is a wrapper that can be called from Weave via ccall.
 * 
 * Parameters:
 *   ir_string: LLVM IR code as string
 *   function_name: Name of function to look up
 * 
 * Returns: Function pointer as Int32 (cast to void* when needed)
 *          Returns 0 (NULL) on error
 */
int llvm_jit_compile_and_get_ptr(const char *ir_string, const char *function_name) {
    if (!ir_string || !function_name) {
        return 0;
    }
    
    void *func_ptr = llvm_jit_compile_and_lookup(ir_string, strlen(ir_string), function_name);
    return (int)(intptr_t)func_ptr;
}

/* JIT compile and call a function that takes two Int32s and returns Int32.
 * This is a convenience function for simple cases.
 * 
 * Parameters:
 *   ir_string: LLVM IR code as string
 *   function_name: Name of function to call
 *   arg1: First argument (Int32)
 *   arg2: Second argument (Int32)
 * 
 * Returns: Result of function call (Int32)
 *          Returns -1 on error
 */
int llvm_jit_call_i32_i32_i32(const char *ir_string, const char *function_name, int arg1, int arg2) {
    if (!ir_string || !function_name) {
        return -1;
    }
    
    typedef int (*FuncPtr)(int, int);
    FuncPtr func = (FuncPtr)llvm_jit_compile_and_lookup(ir_string, strlen(ir_string), function_name);
    
    if (!func) {
        return -1;
    }
    
    return func(arg1, arg2);
}

