#ifndef WEAVE_BOOTSTRAP_STAGE0C_COMMON_H
#define WEAVE_BOOTSTRAP_STAGE0C_COMMON_H

/* Keep this seed compiler maximally portable: C89 + libc.
 * Enable GNU extensions for realpath() in fs.c.
 * Must be defined before any system headers are included. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stddef.h>

void die(const char *msg);
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

void sb_init(StrBuf *b);
void sb_reserve(StrBuf *b, size_t need);
void sb_append_n(StrBuf *b, const char *s, size_t n);
void sb_append(StrBuf *b, const char *s);
void sb_append_ch(StrBuf *b, char ch);
void sb_printf_i32(StrBuf *b, int v);

typedef struct {
    char **items;
    int len;
    int cap;
} StrList;

void sl_init(StrList *sl);
int sl_contains(StrList *sl, const char *s);
void sl_push(StrList *sl, const char *s);

#endif
