/* Helper functions for LLVM compilation that can be called from Weave via ccall */
#include "llvm_compile.h"
#include "common.h"
#include <string.h>
#include <stdlib.h>

/* Wrapper for llvm_compile_ir_to_assembly
 * Parameters: (i8* ir_string, i8* output_path, i32 opt_level)
 * Returns: i32 (0 = success, non-zero = error)
 */
int llvm_compile_ir_to_assembly(const char *ir_string, const char *output_path, int opt_level) {
    if (!ir_string || !output_path) {
        return 1;
    }
    size_t ir_len = strlen(ir_string);
    return llvm_compile_ir_to_assembly_internal(ir_string, ir_len, output_path, opt_level);
}

/* Wrapper for llvm_compile_ir_to_object
 * Parameters: (i8* ir_string, i8* output_path, i32 opt_level)
 * Returns: i32 (0 = success, non-zero = error)
 */
int llvm_compile_ir_to_object(const char *ir_string, const char *output_path, int opt_level) {
    if (!ir_string || !output_path) {
        return 1;
    }
    size_t ir_len = strlen(ir_string);
    return llvm_compile_ir_to_object_internal(ir_string, ir_len, output_path, opt_level);
}

/* Note: llvm_link_objects is implemented directly in llvm_compile.c
 * No wrapper needed here - it's called directly via ccall
 */

