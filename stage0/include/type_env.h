#ifndef WEAVE_BOOTSTRAP_STAGE0_TYPE_ENV_H
#define WEAVE_BOOTSTRAP_STAGE0_TYPE_ENV_H

#include "common.h"
#include "types.h"

typedef struct {
    char *name;
    TypeRef *target;
} AliasDef;

typedef struct {
    char *name;
    int field_count;
    char **field_names;
    TypeRef **field_types;
} StructDef;

typedef struct TypeEnv {
    int alias_count;
    int alias_cap;
    AliasDef *aliases;

    int struct_count;
    int struct_cap;
    StructDef *structs;
} TypeEnv;

void type_env_init(TypeEnv *e);

void type_env_add_alias(TypeEnv *e, const char *name, TypeRef *target);
TypeRef *type_env_resolve_alias(TypeEnv *e, const char *name);

void type_env_add_struct(TypeEnv *e, const char *name, int field_count, char **field_names, TypeRef **field_types);
StructDef *type_env_find_struct(TypeEnv *e, const char *name);
int struct_field_index(StructDef *s, const char *field);

#endif

