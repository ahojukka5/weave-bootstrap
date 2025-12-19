#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char *msg) {
    fprintf(stderr, "weavec0c: %s\n", msg);
    exit(1);
}

void die_at(const char *filename, int line, int col, const char *msg) {
    if (filename) {
        fprintf(stderr, "weavec0c: %s:%d:%d: %s\n", filename, line, col, msg);
    } else {
        fprintf(stderr, "weavec0c: %d:%d: %s\n", line, col, msg);
    }
    exit(1);
}

void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) die("out of memory");
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) die("out of memory");
    return q;
}

char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = (char *)xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

void sb_init(StrBuf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void sb_reserve(StrBuf *b, size_t need) {
    size_t cap;
    if (need <= b->cap) return;
    cap = b->cap ? b->cap : 256;
    while (cap < need) cap *= 2;
    b->data = (char *)xrealloc(b->data, cap);
    b->cap = cap;
}

void sb_append_n(StrBuf *b, const char *s, size_t n) {
    sb_reserve(b, b->len + n + 1);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void sb_append(StrBuf *b, const char *s) {
    sb_append_n(b, s, strlen(s));
}

void sb_append_ch(StrBuf *b, char ch) {
    sb_reserve(b, b->len + 2);
    b->data[b->len++] = ch;
    b->data[b->len] = '\0';
}

void sb_printf_i32(StrBuf *b, int v) {
    char tmp[32];
    sprintf(tmp, "%d", v);
    sb_append(b, tmp);
}

void sl_init(StrList *sl) {
    sl->items = NULL;
    sl->len = 0;
    sl->cap = 0;
}

int sl_contains(StrList *sl, const char *s) {
    int i;
    for (i = 0; i < sl->len; i++) {
        if (strcmp(sl->items[i], s) == 0) return 1;
    }
    return 0;
}

void sl_push(StrList *sl, const char *s) {
    int cap;
    if (sl->len + 1 > sl->cap) {
        cap = sl->cap ? sl->cap * 2 : 16;
        sl->items = (char **)xrealloc(sl->items, (size_t)cap * sizeof(char *));
        sl->cap = cap;
    }
    sl->items[sl->len++] = xstrdup(s);
}

