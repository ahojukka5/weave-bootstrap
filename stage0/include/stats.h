#ifndef WEAVE_BOOTSTRAP_STAGE0_STATS_H
#define WEAVE_BOOTSTRAP_STAGE0_STATS_H

/* Compiler statistics/telemetry system - Julia-style tracking
 * 
 * Tracks what the compiler generates to help with:
 * - Debugging codegen issues
 * - Finding optimization opportunities
 * - Measuring performance impact
 * - Understanding compiler behavior
 */

typedef struct {
    /* Intrinsic/Builtin statistics */
    int emitted_intrinsics;
    int emitted_gep;
    int emitted_bitcast;
    int emitted_ptr_add;
    int emitted_get_field;
    
    /* Arithmetic operations */
    int emitted_add;
    int emitted_sub;
    int emitted_mul;
    int emitted_div;
    
    /* Comparisons */
    int emitted_cmp;
    int emitted_eq;
    int emitted_ne;
    int emitted_lt;
    int emitted_le;
    int emitted_gt;
    int emitted_ge;
    
    /* Logical operations */
    int emitted_and;
    int emitted_or;
    
    /* Memory operations */
    int emitted_load;
    int emitted_store;
    int emitted_alloca;
    
    /* Function calls */
    int emitted_calls;
    int emitted_ccalls;
    
    /* Type conversions */
    int emitted_type_conversions;
    int emitted_ptrtoint;
    int emitted_inttoptr;
    
    /* Control flow */
    int emitted_branches;
    int emitted_returns;
    
    /* Other */
    int emitted_string_lits;
    int emitted_constants;
} CompilerStats;

/* Global statistics instance */
extern CompilerStats compiler_stats;

/* Initialize statistics */
void stats_init(void);

/* Reset all statistics to zero */
void stats_reset(void);

/* Print statistics summary */
void stats_print(void);

/* Increment a statistic - Julia-style macro */
#define STAT_INC(field) (compiler_stats.field++)

/* Convenience macros for common operations */
#define STAT_INC_INTRINSIC() STAT_INC(emitted_intrinsics)
#define STAT_INC_GEP() STAT_INC(emitted_gep)
#define STAT_INC_BITCAST() STAT_INC(emitted_bitcast)
#define STAT_INC_PTR_ADD() STAT_INC(emitted_ptr_add)
#define STAT_INC_GET_FIELD() STAT_INC(emitted_get_field)
#define STAT_INC_ADD() STAT_INC(emitted_add)
#define STAT_INC_SUB() STAT_INC(emitted_sub)
#define STAT_INC_MUL() STAT_INC(emitted_mul)
#define STAT_INC_DIV() STAT_INC(emitted_div)
#define STAT_INC_CMP() STAT_INC(emitted_cmp)
#define STAT_INC_LOAD() STAT_INC(emitted_load)
#define STAT_INC_STORE() STAT_INC(emitted_store)
#define STAT_INC_CALL() STAT_INC(emitted_calls)
#define STAT_INC_CCALL() STAT_INC(emitted_ccalls)

#endif

