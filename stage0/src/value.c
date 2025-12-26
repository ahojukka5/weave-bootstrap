#include "codegen.h"

Value value_const_i32(int v) {
    Value out;
    out.type = type_i32();
    out.kind = 0;
    out.const_i32 = v;
    out.temp = -1;
    out.ssa_name = NULL;
    out.is_const = 1;
    out.is_pointer = 0;
    out.is_boxed = 0;
    return out;
}

Value value_temp(TypeRef *t, int temp) {
    Value out;
    out.type = t;
    out.kind = 1;
    out.const_i32 = 0;
    out.temp = temp;
    out.ssa_name = NULL;
    out.is_const = 0;
    out.is_pointer = (t && (t->kind == TY_PTR || t->kind == TY_I8PTR));
    out.is_boxed = 0;
    return out;
}

Value value_ssa(TypeRef *t, const char *name) {
    Value out;
    out.type = t;
    out.kind = 2;
    out.const_i32 = 0;
    out.temp = -1;
    out.ssa_name = name;
    out.is_const = 0;
    out.is_pointer = (t && (t->kind == TY_PTR || t->kind == TY_I8PTR));
    out.is_boxed = 0;
    return out;
}
