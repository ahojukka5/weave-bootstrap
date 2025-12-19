/*
 * Slim runtime for bootstrap builds (stage1/stage2).
 *
 * Provides:
 *  - main(argc, argv) that forwards to weave_main()
 *  - Minimal CLI helpers: weave_get_input, weave_get_option, weave_has_flag
 *  - String helpers: weave_string_length, weave_string_concat, weave_int_to_string,
 *    weave_string_to_int, weave_string_char_at, weave_string_slice, weave_string_eq
 *  - File I/O: weave_read_file, weave_write_file
 *  - Buffer helpers: weave_buffer_new, weave_buffer_clear, weave_buffer_append_byte,
 *    weave_buffer_append_string, weave_buffer_to_string
 *  - Array helpers (strings, i32): weave_array_str_*, weave_array_i32_*
 *
 * This intentionally excludes advanced features present in the external weave runtime.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------- Entry ---------------------- */
extern int weave_main(void);
// Forward declarations for CLI helpers used in optional debug logging
char *weave_get_input(void);
char *weave_get_option(char *name);
int weave_has_flag(char *name);

static int g_argc = 0;
static char **g_argv = NULL;

int main(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
    const char *dbg = getenv("WEAVE_DEBUG_CLI");
    if (dbg && dbg[0] == '1') {
        fprintf(stderr, "[weave-runtime] argv (%d):\n", g_argc);
        for (int i = 0; i < g_argc; ++i) fprintf(stderr, "  argv[%d] = '%s'\n", i, g_argv[i]);
        char *inp = weave_get_input();
        char *oo = weave_get_option("o");
        char *iopt = weave_get_option("I");
        char *Oopt = weave_get_option("O");
        int e = weave_has_flag("emit-llvm");
        int S = weave_has_flag("S");
        int c = weave_has_flag("c");
        fprintf(stderr, "[weave-runtime] parsed: input='%s' -o='%s' -I='%s' -O='%s' --emit-llvm=%d -S=%d -c=%d\n",
                inp ? inp : "", oo ? oo : "", iopt ? iopt : "", Oopt ? Oopt : "", e, S, c);
    }
    return weave_main();
}

/* ---------------------- CLI helpers ---------------------- */

static int is_long_opt(const char *s) { return s && s[0] == '-' && s[1] == '-' && s[2] != '\0'; }
static int is_short_opt(const char *s) { return s && s[0] == '-' && s[1] != '-' && s[1] != '\0'; }

// Return first positional argument (non-flag), or NULL
char *weave_get_input(void) {
    for (int i = 1; i < g_argc; ++i) {
        const char *arg = g_argv[i];
        if (is_long_opt(arg)) {
            // If form is --name=value, no extra consumption; otherwise may be --name value
            // Only known long flag used is --emit-llvm (no value), so just skip
            continue;
        } else if (is_short_opt(arg)) {
            // Handle options that take a value: -o, -O, -I
            char opt = arg[1];
            if ((opt == 'o' || opt == 'O' || opt == 'I')) {
                if (arg[2] == '\0') {
                    // consume next as value if present
                    if (i + 1 < g_argc) { i += 1; }
                }
                // if value is attached (e.g., -O2 or -Iinc), nothing extra to consume
                continue;
            }
            // Clustered short flags like -Sc; skip
            continue;
        } else {
            // First positional arg: treat as input file
            return g_argv[i];
        }
    }
    return NULL;
}

// Return value for --name=V or --name V; NULL if not present
char *weave_get_option(char *name) {
    if (!name) return NULL;
    size_t nlen = strlen(name);
    if (nlen == 1) {
        // Short option: -o VAL or -oVAL; special-case -I to aggregate
        char opt = name[0];
        char *acc = NULL; size_t acclen = 0;
        int is_aggregate = (opt == 'I');
        for (int i = 1; i < g_argc; ++i) {
            const char *arg = g_argv[i];
            if (!is_short_opt(arg)) continue;
            if (arg[1] != opt) continue;
            const char *val = NULL;
            if (arg[2] != '\0') {
                val = arg + 2; // attached value
            } else if (i + 1 < g_argc) {
                val = g_argv[i + 1];
                // don't skip here; we only read
            }
            if (!val) continue;
            if (!is_aggregate) {
                return (char *)val;
            } else {
                size_t vlen = strlen(val);
                size_t newlen = acclen + vlen + (acclen ? 1 : 0);
                char *nacc = (char *)realloc(acc, newlen + 1);
                if (!nacc) { free(acc); return NULL; }
                acc = nacc;
                if (acclen) acc[acclen++] = ':';
                memcpy(acc + acclen, val, vlen);
                acclen += vlen;
                acc[acclen] = '\0';
            }
        }
        return acc ? acc : NULL;
    }
    // Long option: --name=VAL or --name VAL
    for (int i = 1; i < g_argc; ++i) {
        const char *arg = g_argv[i];
        if (!is_long_opt(arg)) continue;
        if (strncmp(arg + 2, name, nlen) == 0) {
            const char *p = arg + 2 + nlen;
            if (*p == '=') return (char *)(p + 1);
            if (*p == '\0' && i + 1 < g_argc) return g_argv[i + 1];
        }
    }
    return NULL;
}

// Check presence of --name flag
int weave_has_flag(char *name) {
    if (!name) return 0;
    size_t nlen = strlen(name);
    if (nlen == 1) {
        char opt = name[0];
        for (int i = 1; i < g_argc; ++i) {
            const char *arg = g_argv[i];
            if (!is_short_opt(arg)) continue;
            // exact -<opt>
            if (arg[1] == opt) {
                if (arg[2] == '\0') return 1; // -o would be option, but caller uses has_flag only for boolean like -S/-c
                // clustered flags like -Sc or -abc
                const char *p = arg + 1;
                while (*p) { if (*p == opt) return 1; p++; }
            }
        }
        return 0;
    }
    // Long flags: --name or --name=value
    for (int i = 1; i < g_argc; ++i) {
        const char *arg = g_argv[i];
        if (!is_long_opt(arg)) continue;
        if (strncmp(arg + 2, name, nlen) == 0) {
            const char *p = arg + 2 + nlen;
            if (*p == '\0' || *p == '=') return 1;
        }
    }
    return 0;
}

/* ---------------------- String helpers ---------------------- */

int weave_string_length(char *s) {
    return s ? (int)strlen(s) : 0;
}

char *weave_string_concat(char *a, char *b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *out = (char *)malloc(la + lb + 1);
    if (!out) return NULL;
    memcpy(out, a, la);
    memcpy(out + la, b, lb);
    out[la + lb] = '\0';
    return out;
}

char *weave_int_to_string(int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    size_t l = strlen(buf);
    char *out = (char *)malloc(l + 1);
    if (!out) return NULL;
    memcpy(out, buf, l + 1);
    return out;
}

int weave_string_to_int(char *s) {
    if (!s) return 0;
    return atoi(s);
}

int weave_string_char_at(char *s, int idx) {
    if (!s) return 0;
    int len = (int)strlen(s);
    if (idx < 0 || idx >= len) return 0;
    return (unsigned char)s[idx];
}

char *weave_string_slice(char *s, int start, int len) {
    if (!s) return NULL;
    int slen = (int)strlen(s);
    if (start < 0) start = 0;
    if (len < 0) len = 0;
    if (start > slen) start = slen;
    if (start + len > slen) len = slen - start;
    char *out = (char *)malloc((size_t)len + 1);
    if (!out) return NULL;
    memcpy(out, s + start, (size_t)len);
    out[len] = '\0';
    return out;
}

int weave_string_eq(char *a, char *b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}

/* ---------------------- File I/O ---------------------- */

char *weave_read_file(char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (rd != (size_t)size) { free(buf); return NULL; }
    buf[size] = '\0';
    return buf;
}

int weave_write_file(char *path, char *content) {
    if (!path || !content) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t len = strlen(content);
    size_t wr = fwrite(content, 1, len, f);
    fclose(f);
    return (wr == len) ? 0 : -1;
}

/* ---------------------- Buffer helpers ---------------------- */

typedef struct {
    char *data;
    int len;
    int cap;
} weave_buffer;

static weave_buffer *weave_buffer_from_handle(void *h) { return (weave_buffer *)h; }

void *weave_buffer_new(void) {
    weave_buffer *b = (weave_buffer *)malloc(sizeof(weave_buffer));
    if (!b) return NULL;
    b->data = NULL; b->len = 0; b->cap = 0;
    return b;
}

int weave_buffer_clear(void *handle) {
    weave_buffer *b = weave_buffer_from_handle(handle);
    if (!b) return -1;
    b->len = 0;
    return 0;
}

static int weave_buffer_grow(weave_buffer *b, int need) {
    if (b->cap >= need) return 0;
    int ncap = b->cap ? b->cap : 64;
    while (ncap < need) ncap *= 2;
    char *nd = (char *)realloc(b->data, (size_t)ncap);
    if (!nd) return -1;
    b->data = nd; b->cap = ncap;
    return 0;
}

int weave_buffer_append_byte(void *handle, int byte) {
    weave_buffer *b = weave_buffer_from_handle(handle);
    if (!b) return -1;
    if (weave_buffer_grow(b, b->len + 1) != 0) return -1;
    b->data[b->len++] = (char)(byte & 0xFF);
    return 0;
}

int weave_buffer_append_string(void *handle, char *str) {
    if (!str) return 0;
    weave_buffer *b = weave_buffer_from_handle(handle);
    if (!b) return -1;
    int sl = (int)strlen(str);
    if (weave_buffer_grow(b, b->len + sl) != 0) return -1;
    memcpy(b->data + b->len, str, (size_t)sl);
    b->len += sl;
    return 0;
}

char *weave_buffer_to_string(void *handle) {
    weave_buffer *b = weave_buffer_from_handle(handle);
    if (!b) return NULL;
    char *out = (char *)malloc((size_t)b->len + 1);
    if (!out) return NULL;
    memcpy(out, b->data, (size_t)b->len);
    out[b->len] = '\0';
    return out;
}

/* ---------------------- Array helpers ---------------------- */

typedef struct {
    int *data;
    int len;
    int cap;
} weave_array_i32;

static weave_array_i32 *weave_array_i32_from_handle(void *h) { return (weave_array_i32 *)h; }

void *weave_array_i32_new(void) {
    weave_array_i32 *a = (weave_array_i32 *)malloc(sizeof(weave_array_i32));
    if (!a) return NULL;
    a->data = NULL; a->len = 0; a->cap = 0;
    return a;
}

static int weave_array_i32_grow(weave_array_i32 *a, int need) {
    if (a->cap >= need) return 0;
    int ncap = a->cap ? a->cap : 16;
    while (ncap < need) ncap *= 2;
    int *nd = (int *)realloc(a->data, (size_t)ncap * sizeof(int));
    if (!nd) return -1;
    a->data = nd; a->cap = ncap;
    return 0;
}

int weave_array_i32_len(void *handle) {
    weave_array_i32 *a = weave_array_i32_from_handle(handle);
    return a ? a->len : 0;
}

int weave_array_i32_append(void *handle, int value) {
    weave_array_i32 *a = weave_array_i32_from_handle(handle);
    if (!a) return -1;
    if (weave_array_i32_grow(a, a->len + 1) != 0) return -1;
    a->data[a->len++] = value;
    return 0;
}

int weave_array_i32_get(void *handle, int idx) {
    weave_array_i32 *a = weave_array_i32_from_handle(handle);
    if (!a) return 0;
    if (idx < 0 || idx >= a->len) return 0;
    return a->data[idx];
}

int weave_array_i32_set(void *handle, int idx, int value) {
    weave_array_i32 *a = weave_array_i32_from_handle(handle);
    if (!a) return -1;
    if (idx < 0 || idx >= a->len) return -1;
    a->data[idx] = value;
    return 0;
}

typedef struct {
    char **data;
    int len;
    int cap;
} weave_array_str;

static weave_array_str *weave_array_str_from_handle(void *h) { return (weave_array_str *)h; }

void *weave_array_str_new(void) {
    weave_array_str *a = (weave_array_str *)malloc(sizeof(weave_array_str));
    if (!a) return NULL;
    a->data = NULL; a->len = 0; a->cap = 0;
    return a;
}

static int weave_array_str_grow(weave_array_str *a, int need) {
    if (a->cap >= need) return 0;
    int ncap = a->cap ? a->cap : 8;
    while (ncap < need) ncap *= 2;
    char **nd = (char **)realloc(a->data, (size_t)ncap * sizeof(char *));
    if (!nd) return -1;
    a->data = nd; a->cap = ncap;
    return 0;
}

int weave_array_str_len(void *handle) {
    weave_array_str *a = weave_array_str_from_handle(handle);
    return a ? a->len : 0;
}

int weave_array_str_append(void *handle, char *value) {
    weave_array_str *a = weave_array_str_from_handle(handle);
    if (!a) return -1;
    if (weave_array_str_grow(a, a->len + 1) != 0) return -1;
    a->data[a->len++] = value;
    return 0;
}

char *weave_array_str_get(void *handle, int idx) {
    weave_array_str *a = weave_array_str_from_handle(handle);
    if (!a) return NULL;
    if (idx < 0 || idx >= a->len) return NULL;
    return a->data[idx];
}
