#include "types.h"

#include "type_env.h"

#include <stdio.h>
#include <stdlib.h>
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
    /* Debug: track TypeRef allocation */
    if (getenv("WEAVEC0_DEBUG_MEM")) {
        fprintf(stderr, "[mem] type_struct allocated: %p, kind=%d, name='%s'\n",
                (void *)t, t->kind, name ? name : "<null>");
    }
    return t;
}

TypeRef *type_ptr(TypeRef *pointee) {
    TypeRef *t = (TypeRef *)xmalloc(sizeof(TypeRef));
    t->kind = TY_PTR;
    t->name = NULL;
    t->pointee = pointee;
    /* Debug: track TypeRef allocation */
    if (getenv("WEAVEC0_DEBUG_MEM")) {
        fprintf(stderr, "[mem] type_ptr allocated: %p, kind=%d, pointee=%p\n",
                (void *)t, t->kind, (void *)pointee);
    }
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

    /* Handle list types: (ptr T) or (struct Name) */
    if (n->kind == N_LIST) {
        Node *head = list_nth(n, 0);
        if (head && is_atom(head, "ptr")) {
            Node *inner_node = list_nth(n, 1);
            if (!inner_node) {
                /* Missing inner type in (ptr ...) - default to i32 */
                return type_i32();
            }
            /* Recursively parse the inner type */
            TypeRef *inner = parse_type_node(tenv, inner_node);
            if (!inner) {
                /* Defensive: if parsing failed, default to i32 */
                inner = type_i32();
            }
            /* Create pointer type - inner can be any type (struct, ptr, i32, etc.) */
            /* If inner is already a pointer, that means (ptr (ptr T)) which is valid */
            return type_ptr(inner);
        }
        if (is_atom(head, "struct")) {
            Node *name_node = list_nth(n, 1);
            if (name_node && name_node->kind == N_ATOM) {
                return type_struct(atom_text(name_node));
            }
            return type_struct("unknown");
        }
        /* Unknown list form - default to i32 */
        return type_i32();
    }

    /* Handle atom types: Int32, String, Arena, etc. */
    if (n->kind != N_ATOM) return type_i32();
    s = atom_text(n);
    if (!s) return type_i32();
    if (strcmp(s, "Int32") == 0) return type_i32();
    if (strcmp(s, "Void") == 0) return type_void();
    if (is_handle_name(s)) return type_i8ptr();

    alias = tenv ? type_env_resolve_alias(tenv, s) : NULL;
    if (alias) return alias;

    return type_struct(s);
}

