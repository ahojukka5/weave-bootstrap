#include "type_env.h"

#include <string.h>

static void reserve_aliases(TypeEnv *e, int need) {
    int cap;
    if (need <= e->alias_cap) return;
    cap = e->alias_cap ? e->alias_cap : 16;
    while (cap < need) cap *= 2;
    e->aliases = (AliasDef *)xrealloc(e->aliases, (size_t)cap * sizeof(AliasDef));
    e->alias_cap = cap;
}

static void reserve_structs(TypeEnv *e, int need) {
    int cap;
    if (need <= e->struct_cap) return;
    cap = e->struct_cap ? e->struct_cap : 8;
    while (cap < need) cap *= 2;
    e->structs = (StructDef *)xrealloc(e->structs, (size_t)cap * sizeof(StructDef));
    e->struct_cap = cap;
}

void type_env_init(TypeEnv *e) {
    e->alias_count = 0;
    e->alias_cap = 0;
    e->aliases = NULL;
    e->struct_count = 0;
    e->struct_cap = 0;
    e->structs = NULL;
}

void type_env_add_alias(TypeEnv *e, const char *name, TypeRef *target) {
    int i;
    for (i = 0; i < e->alias_count; i++) {
        if (strcmp(e->aliases[i].name, name) == 0) {
            e->aliases[i].target = target;
            return;
        }
    }
    reserve_aliases(e, e->alias_count + 1);
    e->aliases[e->alias_count].name = xstrdup(name);
    e->aliases[e->alias_count].target = target;
    e->alias_count += 1;
}

TypeRef *type_env_resolve_alias(TypeEnv *e, const char *name) {
    int i;
    if (!e) return NULL;
    for (i = 0; i < e->alias_count; i++) {
        if (strcmp(e->aliases[i].name, name) == 0) return e->aliases[i].target;
    }
    return NULL;
}

void type_env_add_struct(TypeEnv *e, const char *name, int field_count, char **field_names, TypeRef **field_types) {
    int i;
    StructDef *s;
    for (i = 0; i < e->struct_count; i++) {
        if (strcmp(e->structs[i].name, name) == 0) {
            /* Replace (minimal). */
            e->structs[i].field_count = field_count;
            e->structs[i].field_names = field_names;
            e->structs[i].field_types = field_types;
            return;
        }
    }
    reserve_structs(e, e->struct_count + 1);
    s = &e->structs[e->struct_count];
    s->name = xstrdup(name);
    s->field_count = field_count;
    s->field_names = field_names;
    s->field_types = field_types;
    e->struct_count += 1;
}

StructDef *type_env_find_struct(TypeEnv *e, const char *name) {
    int i;
    for (i = 0; i < e->struct_count; i++) {
        if (strcmp(e->structs[i].name, name) == 0) return &e->structs[i];
    }
    return NULL;
}

int struct_field_index(StructDef *s, const char *field) {
    int i;
    if (!s) return -1;
    for (i = 0; i < s->field_count; i++) {
        if (strcmp(s->field_names[i], field) == 0) return i;
    }
    return -1;
}

