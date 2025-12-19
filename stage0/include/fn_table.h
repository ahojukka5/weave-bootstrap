#ifndef WEAVE_BOOTSTRAP_STAGE0_FN_TABLE_H
#define WEAVE_BOOTSTRAP_STAGE0_FN_TABLE_H

#include "common.h"
#include "types.h"

typedef struct {
    int count;
    int cap;
    char **names;
    TypeRef **ret_types;
    int *param_counts;
    TypeRef ***param_types;
} FnTable;

void fn_table_init(FnTable *t);
void fn_table_add(FnTable *t, const char *name, TypeRef *ret_type, int param_count, TypeRef **param_types);
int fn_table_find(FnTable *t, const char *name);
TypeRef *fn_table_ret_type(FnTable *t, const char *name, TypeRef *default_ret);
TypeRef *fn_table_param_type(FnTable *t, const char *name, int index, TypeRef *default_ty);
int fn_table_param_count(FnTable *t, const char *name);

#endif

