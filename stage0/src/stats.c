#include "stats.h"
#include "common.h"
#include <stdio.h>
#include <string.h>

/* Global statistics instance */
CompilerStats compiler_stats = {0};

void stats_init(void) {
    memset(&compiler_stats, 0, sizeof(compiler_stats));
}

void stats_reset(void) {
    memset(&compiler_stats, 0, sizeof(compiler_stats));
}

void stats_print(void) {
    printf("\n=== Compiler Statistics ===\n\n");
    
    printf("Intrinsics/Builtins:\n");
    printf("  Intrinsics:        %d\n", compiler_stats.emitted_intrinsics);
    printf("  GEP:               %d\n", compiler_stats.emitted_gep);
    printf("  Bitcast:           %d\n", compiler_stats.emitted_bitcast);
    printf("  Ptr-Add:           %d\n", compiler_stats.emitted_ptr_add);
    printf("  Get-Field:         %d\n", compiler_stats.emitted_get_field);
    printf("\n");
    
    printf("Arithmetic:\n");
    printf("  Add:               %d\n", compiler_stats.emitted_add);
    printf("  Sub:               %d\n", compiler_stats.emitted_sub);
    printf("  Mul:               %d\n", compiler_stats.emitted_mul);
    printf("  Div:               %d\n", compiler_stats.emitted_div);
    printf("\n");
    
    printf("Comparisons:\n");
    printf("  Total:             %d\n", compiler_stats.emitted_cmp);
    printf("  ==:                %d\n", compiler_stats.emitted_eq);
    printf("  !=:                %d\n", compiler_stats.emitted_ne);
    printf("  <:                 %d\n", compiler_stats.emitted_lt);
    printf("  <=:                %d\n", compiler_stats.emitted_le);
    printf("  >:                 %d\n", compiler_stats.emitted_gt);
    printf("  >=:                %d\n", compiler_stats.emitted_ge);
    printf("\n");
    
    printf("Logical:\n");
    printf("  &&:                %d\n", compiler_stats.emitted_and);
    printf("  ||:                %d\n", compiler_stats.emitted_or);
    printf("\n");
    
    printf("Memory:\n");
    printf("  Load:              %d\n", compiler_stats.emitted_load);
    printf("  Store:             %d\n", compiler_stats.emitted_store);
    printf("  Alloca:            %d\n", compiler_stats.emitted_alloca);
    printf("\n");
    
    printf("Functions:\n");
    printf("  Calls:             %d\n", compiler_stats.emitted_calls);
    printf("  C Calls:           %d\n", compiler_stats.emitted_ccalls);
    printf("\n");
    
    printf("Type Conversions:\n");
    printf("  Total:             %d\n", compiler_stats.emitted_type_conversions);
    printf("  PtrToInt:          %d\n", compiler_stats.emitted_ptrtoint);
    printf("  IntToPtr:          %d\n", compiler_stats.emitted_inttoptr);
    printf("\n");
    
    printf("Control Flow:\n");
    printf("  Branches:          %d\n", compiler_stats.emitted_branches);
    printf("  Returns:           %d\n", compiler_stats.emitted_returns);
    printf("\n");
    
    printf("Other:\n");
    printf("  String Literals:   %d\n", compiler_stats.emitted_string_lits);
    printf("  Constants:        %d\n", compiler_stats.emitted_constants);
    printf("\n");
}

