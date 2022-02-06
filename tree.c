#include <stdlib.h>

#define REPLACE(root, p, x, y) do {	    \
    if (!p) {				    \
	root = y;			    \
    } else if (p->l == x) {		    \
	p->l = y;			    \
    } else {				    \
	p->r = y;			    \
    }					    \
} while (0)

struct Tree {
    int (*cmp)(void *key1, void *key2);
    struct Node *root;
};

struct Node {
    struct Node *r;
    struct Node *l;
    void *key;
};

static void foreach(struct Node *nd, void (*fnc)(void *key));
static void foreach_free(struct Node *nd, void (*fnc)(void *key));

struct Tree *tree_new(int (*cmp)(void *key1, void *key2)) {
    struct Tree *t;

    t = calloc(1, sizeof(*t));
    t->cmp = cmp;

    return t;
}

void tree_insert(struct Tree *t, void *key) {
    struct Node *new, *i, *p;
    int res;

    new = calloc(1, sizeof(*new));
    new->key = key;

    i = t->root;
    p = NULL;
    while (i) {
	p = i;
	if ((res = t->cmp(new->key, i->key)) < 0) {
	    i = i->l;
	} else {
	    i = i->r;
	}
    }

    if (!p) {
	t->root = new;
    } else if (res < 0) {
	p->l = new;
    } else {
	p->r = new;
    }
}

// @todo: should also call a custom free function or something...
void tree_delete(struct Tree *t, void *key) {
    struct Node *i, *p;
    int res;

    i = t->root;
    p = NULL;
    while ((res = t->cmp(key, i->key))) {
	p = i;
	if (res < 0) {
	    i = i->l;
	} else {
	    i = i->r;
	}
    }

    if (!i->l) {
	REPLACE(t->root, p, i, i->r);
    } else if (!i->r) {
	REPLACE(t->root, p, i, i->l);
    } else {
	struct Node *nd, *nd_p = NULL;

	nd = i->r;
	while (nd->l) {
	    nd_p = nd;
	    nd = nd->l;
	}

	if (i->r != nd) {
	    nd_p->l = nd->r;
	    nd->r = i->r;
	}

	nd->l = i->l;

	REPLACE(t->root, p, i, nd);
    }

    free(i);
}

int tree_exists(struct Tree *t, void *key) {
    struct Node *i;
    int res;

    i = t->root;
    while (i) {
	res = t->cmp(key, i->key);
	if (!res) {
	    return 1;
	} else if (res < 0) {
	    i = i->l;
	} else {
	    i = i->r;
	}
    }

    return 0;
}

void tree_foreach(struct Tree *t, void (*fnc)(void *key)) {
    foreach(t->root, fnc);
}

static void foreach(struct Node *nd, void (*fnc)(void *key)) {
    if (nd) {
	foreach(nd->l, fnc);
	fnc(nd->key);
	foreach(nd->r, fnc);
    }
}

void tree_foreach_free(struct Tree *t, void (*fnc)(void *key)) {
    foreach_free(t->root, fnc);
    t->root = NULL;
}

static void foreach_free(struct Node *nd, void (*fnc)(void *key)) {
    if (nd) {
	foreach_free(nd->l, fnc);
	nd->l = NULL;
	fnc(nd->key);
	foreach_free(nd->r, fnc);
	nd->r = NULL;
	free(nd);
    }
}

static void noop(void *key) {
    ;
}

void tree_free(struct Tree *t) {
    tree_foreach_free(t, noop);
    free(t);
}
