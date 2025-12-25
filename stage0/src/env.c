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
    /* Allocate: 'v' (1) + '_' (1) + base (nb) + '_' (1) + numbuf (written) + '\0' (1) = nb + written + 4 */
    out = (char *)xmalloc(nb + (size_t)written + 4);
    out[0] = 'v';
    out[1] = '_';
    memcpy(out + 2, base, nb);
    out[2 + nb] = '_';
    memcpy(out + 3 + nb, numbuf, (size_t)written);
    out[3 + nb + (size_t)written] = '\0';
    free(base);
    return out;
}

static void env_add(VarEnv *e, const char *name, int kind, TypeRef *type) {
    env_reserve(e, e->names.len + 1);
    sl_push(&e->names, name);
    sl_push(&e->ssa_names, make_ssa_name(e, name));
    int idx = e->names.len - 1;
    e->kinds[idx] = kind;
    e->types[idx] = type;
    /* Debug: track TypeRef storage */
    if (getenv("WEAVEC0_DEBUG_MEM") && type) {
        fprintf(stderr, "[mem] env_add storing '%s': idx=%d, type=%p, type->kind=%d\n",
                name ? name : "<null>", idx, (void *)type, type->kind);
    }
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
    TypeRef *result = e->types[idx];
    /* Debug: verify type retrieval */
    if (getenv("WEAVEC0_DEBUG_SIGS") && strcmp(name, "a") == 0) {
        fprintf(stderr, "[dbg] env_type('a'): idx=%d, result=%p, result_kind=%d\n",
                idx, (void *)result, result ? result->kind : -1);
    }
    /* Debug: track TypeRef retrieval and verify integrity */
    if (getenv("WEAVEC0_DEBUG_MEM") && result) {
        int valid_kind = (result->kind >= 0 && result->kind <= 4); /* TY_I32=0, TY_PTR=4 */
        fprintf(stderr, "[mem] env_type retrieving '%s': idx=%d, type=%p, kind=%d, valid=%d\n",
                name ? name : "<null>", idx, (void *)result, result->kind, valid_kind);
        if (!valid_kind) {
            fprintf(stderr, "[mem] ERROR: Invalid TypeRef kind detected! Memory corruption?\n");
        }
    }
    return result;
}

const char *env_ssa_name(VarEnv *e, const char *name) {
    int idx = env_find(e, name);
    if (idx < 0 || idx >= e->ssa_names.len) return name;
    return e->ssa_names.items[idx];
}
