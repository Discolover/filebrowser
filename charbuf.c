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

    if (alloc > cb->n && alloc <= CB_MAX) {
	cb->n = alloc;
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

void charbuf_addstr(struct CharBuf *cb, char *s) {
    int l;

    l = strnlen(s, CB_MAX - 1);
    assert(cb->len + l < cb->n);

    memcpy(cb->buf + cb->len, s, l);
    cb->len += l;
    cb->buf[cb->len] = '\0';
}

void charbuf_free(struct CharBuf *cb) {
    free(cb->buf);
    free(cb);
}
