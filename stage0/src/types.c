#include "types.h"

#include "type_env.h"

#include <string.h>

static TypeRef g_i32 = { TY_I32, NULL, NULL };
static TypeRef g_i8ptr = { TY_I8PTR, NULL, NULL };
static TypeRef g_void = { TY_VOID, NULL, NULL };

TypeRef *type_i32(void) { return &g_i32; }
TypeRef *type_i8ptr(void) { return &g_i8ptr; }
TypeRef *type_void(void) { return &g_void; }

TypeRef *type_struct(const char *name) {
    TypeRef *t = (TypeRef *)xmalloc(sizeof(TypeRef));
    t->kind = TY_STRUCT;
    t->name = xstrdup(name ? name : "");
    t->pointee = NULL;
    return t;
}

TypeRef *type_ptr(TypeRef *pointee) {
    TypeRef *t = (TypeRef *)xmalloc(sizeof(TypeRef));
    t->kind = TY_PTR;
    t->name = NULL;
    t->pointee = pointee;
    return t;
}

int type_eq(TypeRef *a, TypeRef *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    if (a->kind == TY_STRUCT) return strcmp(a->name ? a->name : "", b->name ? b->name : "") == 0;
    if (a->kind == TY_PTR) return type_eq(a->pointee, b->pointee);
    return 1;
}

void emit_llvm_type(StrBuf *out, TypeRef *t) {
    if (!t) {
        sb_append(out, "i32");
        return;
    }
    if (t->kind == TY_I32) sb_append(out, "i32");
    else if (t->kind == TY_I8PTR) sb_append(out, "i8*");
    else if (t->kind == TY_VOID) sb_append(out, "void");
    else if (t->kind == TY_STRUCT) {
        sb_append(out, "%");
        sb_append(out, t->name ? t->name : "");
    } else if (t->kind == TY_PTR) {
        emit_llvm_type(out, t->pointee);
        sb_append(out, "*");
    } else {
        sb_append(out, "i32");
    }
}

static int is_handle_name(const char *s) {
    return strcmp(s, "String") == 0 ||
           strcmp(s, "Buffer") == 0 ||
           strcmp(s, "ArrayString") == 0 ||
           strcmp(s, "ArrayInt32") == 0;
}

TypeRef *parse_type_node(TypeEnv *tenv, Node *n) {
    const char *s;
    TypeRef *alias;
    if (!n) return type_i32();

    if (n->kind == N_LIST && is_atom(list_nth(n, 0), "ptr")) {
        TypeRef *inner = parse_type_node(tenv, list_nth(n, 1));
        return type_ptr(inner);
    }

    s = atom_text(n);
    if (strcmp(s, "Int32") == 0) return type_i32();
    if (strcmp(s, "Void") == 0) return type_void();
    if (is_handle_name(s)) return type_i8ptr();

    alias = tenv ? type_env_resolve_alias(tenv, s) : NULL;
    if (alias) return alias;

    return type_struct(s);
}

