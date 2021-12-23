struct CharBuf {
    char *buf;
    int n;
    int len;
};

#include <limits.h>
#define CB_MAX PATH_MAX
#define CB_STR_NMEMB -1

struct CharBuf *charbuf_new(char *s, int alloc);
void charbuf_addch(struct CharBuf *cb, char ch);
void charbuf_addstr(struct CharBuf *cb, char *s);
int charbuf_revstrcmp(struct CharBuf *a, struct CharBuf *b);
void charbuf_free(struct CharBuf *cb);
