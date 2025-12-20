#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file_all(const char *path) {
    FILE *f = fopen(path, "rb");
    long size;
    char *buf;
    size_t nread;
    if (!f) {
        fprintf(stderr, "weavec0c: cannot read file: %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) size = 0;
    buf = (char *)xmalloc((size_t)size + 1);
    nread = fread(buf, 1, (size_t)size, f);
    buf[nread] = '\0';
    /* File size limits with override mechanism:
       - Soft limit (256 lines): warning
       - Hard limit (512 lines): error
       - Override: Use ";;; @weave-allow-long-file: <reason>" at file start
         to extend hard limit to 1024 lines. Reason must be non-empty.
    */
    {
        size_t lines = 0;
        size_t hard_limit = 512;
        int has_override = 0;
        const char *override_marker = ";;; @weave-allow-long-file:";
        const size_t marker_len = strlen(override_marker);
        
        /* Check for override directive in first line */
        if (nread > marker_len && strncmp(buf, override_marker, marker_len) == 0) {
            const char *reason_start = buf + marker_len;
            const char *newline = strchr(reason_start, '\n');
            size_t reason_len = newline ? (size_t)(newline - reason_start) : strlen(reason_start);
            
            /* Skip whitespace */
            while (reason_len > 0 && (*reason_start == ' ' || *reason_start == '\t')) {
                reason_start++;
                reason_len--;
            }
            
            /* Verify reason is not empty */
            if (reason_len > 0) {
                has_override = 1;
                hard_limit = 1024;
                fprintf(stderr,
                        "weavec0c: note: file has long-file override (limit extended to 1024 lines): %s\n",
                        path);
            } else {
                fprintf(stderr,
                        "weavec0c: error: @weave-allow-long-file directive requires a reason: %s\n",
                        path);
                exit(1);
            }
        }
        
        /* Count lines */
        if (nread > 0) {
            size_t i;
            for (i = 0; i < nread; i++) {
                if (buf[i] == '\n') lines++;
            }
            if (buf[nread - 1] != '\n') lines++;
        }
        
        /* Check limits */
        if (lines > hard_limit) {
            fprintf(stderr,
                    "weavec0c: cannot fit in it memory more than %zu things (file has %zu lines): %s\n",
                    hard_limit, lines, path);
            if (!has_override) {
                fprintf(stderr,
                        "weavec0c: hint: If this file truly cannot be split logically, add:\n");
                fprintf(stderr,
                        "weavec0c:       ;;; @weave-allow-long-file: <explain why this file must be long>\n");
                fprintf(stderr,
                        "weavec0c:       at the very first line of the file.\n");
            }
            exit(1);
        }
        if (lines > 256) {
            fprintf(stderr,
                    "weavec0c: warning: file exceeds soft limit of 256 lines (has %zu lines): %s\n",
                    lines, path);
        }
    }
    fclose(f);
    return buf;
}

static char *path_join2(const char *a, const char *b) {
    size_t na = strlen(a);
    size_t nb = strlen(b);
    int need_slash = 0;
    char *out;
    if (na > 0 && a[na - 1] != '/' && a[na - 1] != '\\') need_slash = 1;
    out = (char *)xmalloc(na + (size_t)need_slash + nb + 1);
    memcpy(out, a, na);
    if (need_slash) out[na++] = '/';
    memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static char *resolve_include_path(const char *include_path, const char *base_dir, StrList *include_dirs) {
    char *cand;

    /* Relative includes: resolve against base_dir if provided. */
    if (include_path[0] == '.' && (include_path[1] == '/' || (include_path[1] == '.' && include_path[2] == '/'))) {
        if (!base_dir) die("relative include used but base_dir is NULL");
        cand = path_join2(base_dir, include_path);
        if (file_exists(cand)) return cand;
        free(cand);
        return NULL;
    }

    /* Search include dirs. */
    if (include_dirs) {
        int i;
        for (i = 0; i < include_dirs->len; i++) {
            cand = path_join2(include_dirs->items[i], include_path);
            if (file_exists(cand)) return cand;
            free(cand);
        }
    }

    /* Last resort: try as-is (relative to CWD). */
    cand = xstrdup(include_path);
    if (file_exists(cand)) return cand;
    free(cand);

    return NULL;
}

static char *dir_name(const char *path) {
    const char *slash = NULL;
    size_t len;
    char *out;
    int i;

    if (!path) return xstrdup(".");
    len = strlen(path);
    for (i = (int)len - 1; i >= 0; i--) {
        if (path[i] == '/' || path[i] == '\\') {
            slash = path + i;
            break;
        }
    }
    if (!slash) return xstrdup(".");
    len = (size_t)(slash - path);
    out = (char *)xmalloc(len + 1);
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

static void merge_file_into(Node *dst_list, const char *file_path, StrList *included_files, StrList *include_dirs, const char *current_filename);

static void merge_includes_in_list(Node *list, StrList *included_files, const char *base_dir, StrList *include_dirs, const char *current_filename) {
    int i;
    for (i = 0; i < list->count; i++) {
        Node *form = list->items[i];
        Node *head = list_nth(form, 0);
        if (!head || head->kind != N_ATOM) continue;

        if (strcmp(head->text, "include") == 0) {
            Node *path_node = list_nth(form, 1);
            const char *inc = atom_text(path_node);
            char *resolved = NULL;

            if (!inc || inc[0] == '\0') continue;
            resolved = resolve_include_path(inc, base_dir, include_dirs);
            if (!resolved) {
                fprintf(stderr, "weavec0c: include not found: %s (base_dir=%s)\n", inc, base_dir ? base_dir : "<null>");
                exit(1);
            }
            {
                char *canon = realpath(resolved, NULL);
                const char *use = canon ? canon : resolved;
                if (!sl_contains(included_files, use)) {
                    sl_push(included_files, use);
                    merge_file_into(list, use, included_files, include_dirs, current_filename);
                }
                if (canon) free(canon);
            }
            /* Prevent re-processing this include when the merged nodes are walked again. */
            head->text = "";
            free(resolved);
        } else if (is_atom(head, "module") || is_atom(head, "program")) {
            merge_includes_in_list(form, included_files, base_dir, include_dirs, current_filename);
        }
    }
}

void merge_includes(Node *top, StrList *included_files, const char *base_dir, StrList *include_dirs, const char *current_filename) {
    merge_includes_in_list(top, included_files, base_dir, include_dirs, current_filename);
}

static void merge_file_into(Node *dst_list, const char *file_path, StrList *included_files, StrList *include_dirs, const char *current_filename) {
    char *src = read_file_all(file_path);
    Node *file_top = parse_top(src, file_path);
    char *dir = dir_name(file_path);
    int i;
    free(src);

    merge_includes(file_top, included_files, dir, include_dirs, file_path);
    free(dir);

    for (i = 0; i < file_top->count; i++) {
        node_list_push(dst_list, file_top->items[i]);
    }
}
