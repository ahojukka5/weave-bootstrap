#include "env.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void env_init(VarEnv *e) {
    sl_init(&e->names);
    sl_init(&e->ssa_names);
    e->kinds = NULL;
    e->types = NULL;
    e->cap = 0;
}

int env_find(VarEnv *e, const char *name) {
    int i;
    for (i = e->names.len - 1; i >= 0; i--) {
        if (strcmp(e->names.items[i], name) == 0) return i;
    }
    return -1;
}

int env_has(VarEnv *e, const char *name) {
    return env_find(e, name) >= 0;
}

static void env_reserve(VarEnv *e, int need) {
    int cap;
    if (need <= e->cap) return;
    cap = e->cap ? e->cap : 16;
    while (cap < need) cap *= 2;
    e->kinds = (int *)xrealloc(e->kinds, (size_t)cap * sizeof(int));
    e->types = (TypeRef **)xrealloc(e->types, (size_t)cap * sizeof(TypeRef *));
    e->cap = cap;
}

static char *sanitize_name(const char *name) {
    size_t n = strlen(name);
    char *out;
    size_t i;
    if (n == 0) return xstrdup("v");
    out = (char *)xmalloc(n + 2);
    for (i = 0; i < n; i++) {
        char c = name[i];
        if (isalnum((unsigned char)c) || c == '_') out[i] = c;
        else out[i] = '_';
    }
    out[n] = '\0';
    if (isdigit((unsigned char)out[0])) {
        char *prefixed = (char *)xmalloc(n + 3);
        prefixed[0] = 'v';
        prefixed[1] = '_';
        memcpy(prefixed + 2, out, n + 1);
        free(out);
        return prefixed;
    }
    return out;
}

static char *make_ssa_name(VarEnv *e, const char *name) {
    char *base = sanitize_name(name);
    size_t nb = strlen(base);
    char numbuf[32];
    int idx = e->names.len;
    int written;
    char *out;
    written = snprintf(numbuf, sizeof(numbuf), "%d", idx);
    if (written < 0) written = 0;
    out = (char *)xmalloc(nb + (size_t)written + 3);
    out[0] = 'v';
    out[1] = '_';
    memcpy(out + 2, base, nb);
    memcpy(out + 2 + nb, "_", 1);
    memcpy(out + 3 + nb, numbuf, (size_t)written + 1);
    free(base);
    return out;
}

static void env_add(VarEnv *e, const char *name, int kind, TypeRef *type) {
    env_reserve(e, e->names.len + 1);
    sl_push(&e->names, name);
    sl_push(&e->ssa_names, make_ssa_name(e, name));
    e->kinds[e->names.len - 1] = kind;
    e->types[e->names.len - 1] = type;
}

void env_add_local(VarEnv *e, const char *name, TypeRef *type) {
    env_add(e, name, 0, type);
}

void env_add_param(VarEnv *e, const char *name, TypeRef *type) {
    env_add(e, name, 1, type);
}

int env_kind(VarEnv *e, const char *name) {
    int idx = env_find(e, name);
    if (idx < 0) return -1;
    return e->kinds[idx];
}

TypeRef *env_type(VarEnv *e, const char *name) {
    int idx = env_find(e, name);
    if (idx < 0) return NULL;
    return e->types[idx];
}

const char *env_ssa_name(VarEnv *e, const char *name) {
    int idx = env_find(e, name);
    if (idx < 0 || idx >= e->ssa_names.len) return name;
    return e->ssa_names.items[idx];
}
