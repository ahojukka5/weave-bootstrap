#include "lexer.h"

#include <ctype.h>
#include <string.h>

static int lex_peek(Lexer *lx) {
    if (lx->pos >= lx->len) return -1;
    return (unsigned char)lx->src[lx->pos];
}

static int lex_get(Lexer *lx) {
    if (lx->pos >= lx->len) return -1;
    return (unsigned char)lx->src[lx->pos++];
}

static void lex_skip_ws_and_comments(Lexer *lx) {
    for (;;) {
        int ch = lex_peek(lx);
        if (ch < 0) return;
        if (isspace(ch)) {
            (void)lex_get(lx);
            continue;
        }
        if (ch == ';') {
            /* Line comment until newline. */
            while (ch >= 0 && ch != '\n') ch = lex_get(lx);
            continue;
        }
        return;
    }
}

static int is_atom_char(int ch) {
    if (ch < 0) return 0;
    if (isspace(ch)) return 0;
    if (ch == '(' || ch == ')' || ch == '"' || ch == ';') return 0;
    return 1;
}

static char *lex_read_while(Lexer *lx, int (*pred)(int)) {
    StrBuf b;
    sb_init(&b);
    while (1) {
        int ch = lex_peek(lx);
        if (ch < 0 || !pred(ch)) break;
        sb_append_ch(&b, (char)lex_get(lx));
    }
    if (!b.data) return xstrdup("");
    return b.data;
}

static char *lex_read_string(Lexer *lx) {
    StrBuf b;
    sb_init(&b);
    if (lex_get(lx) != '"') die("expected '\"' to start string literal");
    while (1) {
        int ch = lex_get(lx);
        if (ch < 0) die("unterminated string literal");
        if (ch == '"') break;
        if (ch == '\\') {
            int esc = lex_get(lx);
            if (esc < 0) die("unterminated string escape");
            if (esc == 'n') sb_append_ch(&b, '\n');
            else if (esc == 't') sb_append_ch(&b, '\t');
            else if (esc == 'r') sb_append_ch(&b, '\r');
            else sb_append_ch(&b, (char)esc);
        } else {
            sb_append_ch(&b, (char)ch);
        }
    }
    if (!b.data) return xstrdup("");
    return b.data;
}

void lex_init(Lexer *lx, const char *src) {
    lx->src = src;
    lx->pos = 0;
    lx->len = strlen(src);
}

Token lex_next(Lexer *lx) {
    Token t;
    t.kind = TOK_EOF;
    t.text = NULL;

    lex_skip_ws_and_comments(lx);
    {
        int ch = lex_peek(lx);
        if (ch < 0) {
            t.kind = TOK_EOF;
            return t;
        }
        if (ch == '(') {
            (void)lex_get(lx);
            t.kind = TOK_LPAREN;
            return t;
        }
        if (ch == ')') {
            (void)lex_get(lx);
            t.kind = TOK_RPAREN;
            return t;
        }
        if (ch == '"') {
            t.kind = TOK_STRING;
            t.text = lex_read_string(lx);
            return t;
        }
        t.kind = TOK_ATOM;
        t.text = lex_read_while(lx, is_atom_char);
        return t;
    }
}
