#include "ir.h"

void ir_init(IrCtx *ir, StrBuf *out) {
    sb_init(&ir->typedefs);
    sb_init(&ir->globals);
    sb_init(&ir->decls);
    ir->out = out;
    ir->temp = 0;
    ir->label = 0;
    ir->fn_table = NULL;
    ir->type_env = NULL;
    ir->current_fn = NULL;
    sl_init(&ir->declared_ccalls);
}

int ir_fresh_temp(IrCtx *ir) {
    int t = ir->temp;
    ir->temp += 1;
    return t;
}

int ir_fresh_label(IrCtx *ir) {
    int l = ir->label;
    ir->label += 1;
    return l;
}

void ir_emit_temp(StrBuf *out, int t) {
    sb_append(out, "%t");
    sb_printf_i32(out, t);
}

void ir_emit_label_ref(StrBuf *out, int lbl) {
    sb_append(out, "%L");
    sb_printf_i32(out, lbl);
}

void ir_emit_label_def(StrBuf *out, int lbl) {
    sb_append(out, "L");
    sb_printf_i32(out, lbl);
    sb_append(out, ":\n");
}
