#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "util.h"

void *ecalloc(size_t nmemb, size_t size) {
    void *p;

    if (!(p = calloc(nmemb, size))) {
	eprintf("Error allocating memory:");
    }

    return p;
}

void *ereallocarray(void *ptr, size_t nmemb, size_t size) {

}

void eprintf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
	fputc(' ', stderr);
	perror(NULL);
    } else {
	fputc('\n', stderr);
    }

    exit(1);
}
