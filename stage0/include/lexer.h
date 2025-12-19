#ifndef WEAVE_BOOTSTRAP_STAGE0C_LEXER_H
#define WEAVE_BOOTSTRAP_STAGE0C_LEXER_H

#include "common.h"

#include <stddef.h>

typedef enum {
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_ATOM,
    TOK_STRING,
    TOK_EOF
} TokKind;

typedef struct {
    TokKind kind;
    char *text; /* for ATOM/STRING */
    int line;
    int col;
} Token;

typedef struct {
    const char *src;
    size_t pos;
    size_t len;
    int line;
    int col;
} Lexer;

void lex_init(Lexer *lx, const char *src);
Token lex_next(Lexer *lx);

#endif

