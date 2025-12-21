#ifndef WEAVE_BOOTSTRAP_STAGE0_IR_H
#define WEAVE_BOOTSTRAP_STAGE0_IR_H

#include "common.h"

typedef struct {
    StrBuf typedefs;
    StrBuf globals;
    StrBuf decls;
    StrBuf *out;
    int temp;
    int label;
    void *fn_table; /* FnTable*, kept opaque to avoid header cycles */
    void *type_env; /* TypeEnv*, kept opaque to avoid header cycles */
    const char *current_fn;
    StrList declared_ccalls; /* Track already-declared ccall symbols */
    int run_tests_mode; /* When nonzero, emit embedded test functions and test runner */
    StrList test_funcs;      /* Names of emitted test functions */
    StrList test_names;      /* Human-readable test names */
    StrList selected_test_names; /* Filters for selected test names */
    StrList selected_tags;        /* Filters for selected tags */
    int saw_expect;               /* Per-test flag: saw any expect-* assertion */
} IrCtx;

void ir_init(IrCtx *ir, StrBuf *out);
int ir_fresh_temp(IrCtx *ir);
int ir_fresh_label(IrCtx *ir);
void ir_emit_temp(StrBuf *out, int t);
void ir_emit_label_ref(StrBuf *out, int lbl);
void ir_emit_label_def(StrBuf *out, int lbl);

#endif
