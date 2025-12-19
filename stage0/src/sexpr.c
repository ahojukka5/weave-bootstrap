#include "sexpr.h"

#include <string.h>

static Node *node_new(NodeKind k) {
    Node *n = (Node *)xmalloc(sizeof(Node));
    n->kind = k;
    n->text = NULL;
    n->items = NULL;
    n->count = 0;
    n->cap = 0;
    return n;
}

void node_list_push(Node *list, Node *child) {
    int cap;
    if (!list || list->kind != N_LIST) die("internal: push into non-list");
    if (list->count + 1 > list->cap) {
        cap = list->cap ? list->cap * 2 : 8;
        list->items = (Node **)xrealloc(list->items, (size_t)cap * sizeof(Node *));
        list->cap = cap;
    }
    list->items[list->count++] = child;
}

static Node *parse_list(Lexer *lx) {
    Node *list = node_new(N_LIST);
    for (;;) {
        Token t = lex_next(lx);
        if (t.kind == TOK_EOF) die("unexpected EOF inside list");
        if (t.kind == TOK_RPAREN) break;
        if (t.kind == TOK_LPAREN) {
            node_list_push(list, parse_list(lx));
        } else if (t.kind == TOK_ATOM) {
            Node *a = node_new(N_ATOM);
            a->text = t.text;
            node_list_push(list, a);
        } else if (t.kind == TOK_STRING) {
            Node *s = node_new(N_STRING);
            s->text = t.text;
            node_list_push(list, s);
        } else {
            die("unexpected token in list");
        }
    }
    return list;
}

static Node *parse_node(Lexer *lx) {
    Token t = lex_next(lx);
    if (t.kind == TOK_EOF) return NULL;
    if (t.kind == TOK_LPAREN) return parse_list(lx);
    if (t.kind == TOK_ATOM) {
        Node *a = node_new(N_ATOM);
        a->text = t.text;
        return a;
    }
    if (t.kind == TOK_STRING) {
        Node *s = node_new(N_STRING);
        s->text = t.text;
        return s;
    }
    die("unexpected token");
    return NULL;
}

Node *parse_top(const char *src) {
    Lexer lx;
    Node *top = node_new(N_LIST);
    lex_init(&lx, src);
    while (1) {
        Node *n = parse_node(&lx);
        if (!n) break;
        node_list_push(top, n);
    }
    return top;
}

int is_atom(Node *n, const char *s) {
    return n && n->kind == N_ATOM && n->text && strcmp(n->text, s) == 0;
}

Node *list_nth(Node *list, int idx) {
    if (!list || list->kind != N_LIST) return NULL;
    if (idx < 0 || idx >= list->count) return NULL;
    return list->items[idx];
}

const char *atom_text(Node *n) {
    if (!n) return "";
    if (n->kind == N_ATOM || n->kind == N_STRING) return n->text ? n->text : "";
    return "";
}
