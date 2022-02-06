#include <stdio.h>
#include "utf8.h"

#define UTF8_NBYTES 4

int byte[UTF8_NBYTES + 1] = {0x80    , 0   , 0xC0 , 0xE0  , 0xF0    };
int mask[UTF8_NBYTES + 1] = {0xC0    , 0x80, 0xE0 , 0xF0  , 0xF8    };
int minv[UTF8_NBYTES + 1] = {0       , 0   , 0x80 , 0x800 , 0x10000 };
int maxv[UTF8_NBYTES + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static int decode_utf8byte(FILE *f, uint32_t *rune, int *cnt) {
    int c;

    c = fgetc(f);
    if (c == EOF) {
	return -1;
    }

    *cnt += 1;

    for (int i = 0; i < UTF8_NBYTES + 1; ++i) {
	if ((c & mask[i]) == byte[i]) {
	    *rune = (*rune << 6) | (c & ~mask[i]);
	    return i;
	}
    }

    return -1;
}

int fget_rune(FILE *f, uint32_t *rune) {
    int nbytes_read = 0, len;
    int res = 0;

    *wc = UTF8_INVALID;

    len = decode_utf8byte(f, &res, &nbytes_read);
    if (len < 1) {
	return nbytes_read;
    }

    for (int i = 1; i < len; ++i) {
	if (decode_utf8byte(f, &res, &nbytes_read) != 0) {
	    return nbytes_read;
	}
    }

    if (minv[len] <= res && res <= maxv[len]) {
	*wc = res;
    }

    return nbytes_read;
}
