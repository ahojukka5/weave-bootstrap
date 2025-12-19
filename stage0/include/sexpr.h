#ifndef WEAVE_BOOTSTRAP_STAGE0C_SEXPR_H
#define WEAVE_BOOTSTRAP_STAGE0C_SEXPR_H

#include "common.h"
#include "lexer.h"

typedef enum { N_ATOM, N_STRING, N_LIST } NodeKind;

typedef struct {
    const char *filename;
} ParseCtx;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    char *text;      /* for ATOM/STRING */
    Node **items;    /* for LIST */
    int count;
    int cap;
};

Node *parse_top(const char *src, const char *filename);
int is_atom(Node *n, const char *s);
Node *list_nth(Node *list, int idx);
const char *atom_text(Node *n);
void node_list_push(Node *list, Node *child);

#endif

