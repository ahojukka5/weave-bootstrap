#ifndef WEAVE_BOOTSTRAP_STAGE0_ENV_H
#define WEAVE_BOOTSTRAP_STAGE0_ENV_H

#include "common.h"
#include "types.h"

typedef struct {
    StrList names;
    StrList ssa_names;
    int *kinds; /* 0=local alloca, 1=param ssa */
    TypeRef **types;
    int cap;
} VarEnv;

void env_init(VarEnv *e);
int env_find(VarEnv *e, const char *name);
int env_has(VarEnv *e, const char *name);
void env_add_local(VarEnv *e, const char *name, TypeRef *type);
void env_add_param(VarEnv *e, const char *name, TypeRef *type);
int env_kind(VarEnv *e, const char *name);
TypeRef *env_type(VarEnv *e, const char *name);
const char *env_ssa_name(VarEnv *e, const char *name);

#endif
