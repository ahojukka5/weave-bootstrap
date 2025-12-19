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
} IrCtx;

void ir_init(IrCtx *ir, StrBuf *out);
int ir_fresh_temp(IrCtx *ir);
int ir_fresh_label(IrCtx *ir);
void ir_emit_temp(StrBuf *out, int t);
void ir_emit_label_ref(StrBuf *out, int lbl);
void ir_emit_label_def(StrBuf *out, int lbl);

#endif
