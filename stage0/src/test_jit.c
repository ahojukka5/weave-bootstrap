// Simple test program for JIT functionality
#include "llvm_compile.h"
#include <stdio.h>
#include <string.h>

int main() {
    const char *ir = "define i32 @add(i32 %a, i32 %b) {\n"
                     "  %sum = add i32 %a, %b\n"
                     "  ret i32 %sum\n"
                     "}\n";
    
    printf("Testing LLVM JIT compilation...\n");
    
    // Compile and get function pointer
    typedef int (*AddFunc)(int, int);
    AddFunc add_func = (AddFunc)llvm_jit_compile_and_lookup(ir, strlen(ir), "add");
    
    if (!add_func) {
        printf("❌ JIT compilation failed\n");
        return 1;
    }
    
    printf("✅ JIT compilation successful!\n");
    
    // Test the function
    int result = add_func(10, 20);
    printf("add(10, 20) = %d\n", result);
    
    if (result == 30) {
        printf("✅ JIT execution successful!\n");
        return 0;
    } else {
        printf("❌ Wrong result: expected 30, got %d\n", result);
        return 1;
    }
}

