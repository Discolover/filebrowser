#include "charbuf.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "util.h"

struct CharBuf *charbuf_new(char *s, int alloc) {
    struct CharBuf *cb;

    cb = ecalloc(1, sizeof(*cb));

    cb->len = s ? strnlen(s, CB_MAX - 1) : 0;
    cb->n = cb->len + 1;

    if (alloc > CB_MAX) {
	alloc = CB_MAX;
    }

    if (alloc > cb->n) {
	cb->n = alloc;
    }

    cb->buf = ecalloc(cb->n, sizeof(*cb->buf));

    if (s) {
	memcpy(cb->buf, s, cb->len);
    }

    cb->buf[cb->len] = '\0';

    return cb;
}


struct CharBuf *charbuf_new_v2(char *s, int len, int n) {
    struct CharBuf *cb;

    cb = ecalloc(1, sizeof(*cb));
    cb->len = s ? len : 0;
    cb->n = cb->len + 1;

    if (n > cb->n) {
	cb->n = n;
    }

    cb->buf = ecalloc(cb->n, sizeof(*cb->buf));

    if (s) {
	memcpy(cb->buf, s, cb->len);
    }

    cb->buf[cb->len] = '\0';

    return cb;
}

void charbuf_addch(struct CharBuf *cb, char ch) {
    assert(cb->len + 1 < cb->n);

    cb->buf[cb->len] = ch;
    ++cb->len;
    cb->buf[cb->len] = '\0';
}

void charbuf_expand(struct CharBuf *cb, int n) {
    if (n <= cb->n) {
	return;
    }

    cb->buf = realloc(cb->buf, sizeof(*cb->buf) * n);
    cb->n = n;
}

void charbuf_addstr(struct CharBuf *cb, char *s) {
    int l;

    l = strnlen(s, CB_MAX - 1);
    assert(cb->len + l < cb->n);

    memcpy(cb->buf + cb->len, s, l);
    cb->len += l;
    cb->buf[cb->len] = '\0';
}

void charbuf_addstr_v2(struct CharBuf *cb, char *s, int len) {
    assert(cb->len + len < cb->n);
    memcpy(cb->buf + cb->len, s, len);
    cb->len += len;
    cb->buf[cb->len] = '\0';
}

void charbuf_free(struct CharBuf *cb) {
    free(cb->buf);
    free(cb);
}

int charbuf_revstrcmp(struct CharBuf *a, struct CharBuf *b) {
    int min;

    if (a->len < b->len) {
	min = a->len;
    } else {
	min = b->len;
    }

    for (; min >= 0; --min) {
	if (a->buf[min] != b->buf[min]) {
	    return a->buf[min] - b->buf[min];
	}
    }

    return 0;
}

void charbuf_rstrip(struct CharBuf *cb) {
    int j;

    for (j = cb->len - 1; j >= 0 && cb->buf[j] == '\n'; --j) {
	;;
    }

    if (j < 0) {
	cb->len = 0;
    } else {
	cb->len = j + 1;
    }

    cb->buf[cb->len] = '\0';
}
