#include "fn_table.h"

#include <string.h>

static void fn_table_reserve(FnTable *t, int need) {
    int cap;
    if (need <= t->cap) return;
    cap = t->cap ? t->cap : 16;
    while (cap < need) cap *= 2;
    t->names = (char **)xrealloc(t->names, (size_t)cap * sizeof(char *));
    t->ret_types = (TypeRef **)xrealloc(t->ret_types, (size_t)cap * sizeof(TypeRef *));
    t->param_counts = (int *)xrealloc(t->param_counts, (size_t)cap * sizeof(int));
    t->param_types = (TypeRef ***)xrealloc(t->param_types, (size_t)cap * sizeof(TypeRef **));
    t->cap = cap;
}

void fn_table_init(FnTable *t) {
    t->count = 0;
    t->cap = 0;
    t->names = NULL;
    t->ret_types = NULL;
    t->param_counts = NULL;
    t->param_types = NULL;
}

int fn_table_find(FnTable *t, const char *name) {
    int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->names[i], name) == 0) return i;
    }
    return -1;
}

static TypeRef **copy_param_types(int param_count, TypeRef **param_types) {
    int i;
    TypeRef **pt;
    if (param_count <= 0) return NULL;
    pt = (TypeRef **)xmalloc((size_t)param_count * sizeof(TypeRef *));
    for (i = 0; i < param_count; i++) pt[i] = param_types[i];
    return pt;
}

void fn_table_add(FnTable *t, const char *name, TypeRef *ret_type, int param_count, TypeRef **param_types) {
    int idx = fn_table_find(t, name);
    TypeRef **pt = copy_param_types(param_count, param_types);
    if (idx >= 0) {
        t->ret_types[idx] = ret_type;
        t->param_counts[idx] = param_count;
        t->param_types[idx] = pt;
        return;
    }
    fn_table_reserve(t, t->count + 1);
    t->names[t->count] = xstrdup(name);
    t->ret_types[t->count] = ret_type;
    t->param_counts[t->count] = param_count;
    t->param_types[t->count] = pt;
    t->count += 1;
}

TypeRef *fn_table_ret_type(FnTable *t, const char *name, TypeRef *default_ret) {
    int idx = fn_table_find(t, name);
    if (idx < 0) return default_ret;
    return t->ret_types[idx];
}

int fn_table_param_count(FnTable *t, const char *name) {
    int idx = fn_table_find(t, name);
    if (idx < 0) return 0;
    return t->param_counts[idx];
}

TypeRef *fn_table_param_type(FnTable *t, const char *name, int index, TypeRef *default_ty) {
    int idx = fn_table_find(t, name);
    if (idx < 0) return default_ty;
    if (index < 0 || index >= t->param_counts[idx]) return default_ty;
    return t->param_types[idx][index];
}

