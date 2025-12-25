#ifndef WEAVE_BOOTSTRAP_STAGE0C_LLVM_COMPILE_H
#define WEAVE_BOOTSTRAP_STAGE0C_LLVM_COMPILE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LLVM compilation interface - replaces clang system calls */

/* Compile LLVM IR string to object file (internal - takes explicit length).
 * Returns 0 on success, non-zero on error.
 * opt_level: 0=none, 1=less, 2=default, 3=aggressive
 */
int llvm_compile_ir_to_object_internal(const char *ir_string, size_t ir_len,
                                       const char *output_path, int opt_level);

/* Wrapper for ccall - takes string and computes length automatically */
int llvm_compile_ir_to_object(const char *ir_string, const char *output_path, int opt_level);

/* Compile LLVM IR string to assembly file (internal - takes explicit length).
 * Returns 0 on success, non-zero on error.
 * opt_level: 0=none, 1=less, 2=default, 3=aggressive
 */
int llvm_compile_ir_to_assembly_internal(const char *ir_string, size_t ir_len,
                                         const char *output_path, int opt_level);

/* Wrapper for ccall - takes string and computes length automatically */
int llvm_compile_ir_to_assembly(const char *ir_string, const char *output_path, int opt_level);

/* Compile LLVM IR string to object file with address sanitizer.
 * Returns 0 on success, non-zero on error.
 * opt_level: 0=none, 1=less, 2=default, 3=aggressive
 * use_asan: 1 to enable address sanitizer, 0 otherwise
 */
int llvm_compile_ir_to_object_asan(const char *ir_string, size_t ir_len,
                                    const char *output_path, int opt_level, int use_asan);

/* JIT Compilation - compile and execute LLVM IR at runtime */

/* Opaque handle for JIT session */
typedef void* LLVMJITSessionRef;

/* Create a new JIT session. Returns NULL on error. */
LLVMJITSessionRef llvm_jit_create_session(void);

/* Add a module to the JIT session and compile it.
 * Returns 0 on success, non-zero on error.
 * The module IR string should be a complete LLVM module.
 */
int llvm_jit_add_module(LLVMJITSessionRef session, const char *ir_string, size_t ir_len);

/* Look up a function by name and return its address.
 * Returns NULL if function not found.
 * The function pointer can be cast to the appropriate function type.
 */
void* llvm_jit_lookup_function(LLVMJITSessionRef session, const char *function_name);

/* Dispose of a JIT session and free all resources. */
void llvm_jit_dispose_session(LLVMJITSessionRef session);

/* Convenience function: compile IR and get function pointer in one call.
 * Returns function pointer on success, NULL on error.
 * This creates a temporary session, adds the module, looks up the function,
 * and disposes the session. For multiple lookups, use the session API above.
 */
void* llvm_jit_compile_and_lookup(const char *ir_string, size_t ir_len, const char *function_name);

#ifdef __cplusplus
}
#endif

#endif /* WEAVE_BOOTSTRAP_STAGE0C_LLVM_COMPILE_H */

