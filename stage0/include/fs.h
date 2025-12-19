#ifndef WEAVE_BOOTSTRAP_STAGE0C_FS_H
#define WEAVE_BOOTSTRAP_STAGE0C_FS_H

#include "common.h"
#include "sexpr.h"

char *read_file_all(const char *path);

/* Resolves and merges (include "...") into the provided parsed top list. */
void merge_includes(Node *top, StrList *included_files, const char *base_dir, StrList *include_dirs, const char *current_filename);

#endif
