#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#include <wchar.h>
#include <wctype.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

#include <termbox.h>

#include "tui.h"
#include "util.h"
#include "tree.h"
#include "charbuf.h"

#include "math.h"

enum {
    PANEL_VERTICAL_MARGINS = 2,
    PANEL_Y_POSITION = 1
};

extern FILE *LOG;
extern struct CharBuf TMPCHARBUF;

static void draw_file(struct Panel *pnl), draw_directory(struct Panel *pnl);

static int alpha_asc(const void *a, const void *b);
static int alpha_desc(const void *a, const void *b);
static int size_asc(const void *a, const void *b);
static int size_desc(const void *a, const void *b);

static int (*sort[SORT_METHODS_NMEMB]) (const void *, const void *) = {
    alpha_asc,
    alpha_desc,
    size_asc,
    size_desc
};

static int key_cmp(void *key1, void *key2);
static void key_free(void *key);

struct Tree *MARKS;

struct Entry {
    struct CharBuf *name;
    struct stat st;
};

struct PanelCommon {
    wchar_t search_pattern[WNAME_MAX];
    int sort_method;
    bool hide_dot_files;
};

struct Panel {
    struct CharBuf *path;
    float fwidth, fx;
    int height, width, y, x;
    DIR *dir;

    int sort_method;

    struct Entry *entries;
    int n;

    bool hide_dot_files;

    wchar_t search_pattern[WNAME_MAX];
    int *search, nsearch;

    int top, cur;
    FILE *fp;
    // void *draw_data; // @todo: consider generic like pointer as data to draw_fnc
    void (*draw_fnc)(struct Panel *pnl);
};

static int alpha_asc(const void *a, const void *b) {
    const struct Entry *e1 = a, *e2 = b;

    return strcmp(e1->name->buf, e2->name->buf);
}

static int alpha_desc(const void *a, const void *b) {
    return -1 * alpha_asc(a, b);
}

static int size_asc(const void *a, const void *b) {
    const struct Entry *e1 = a, *e2 = b;

    if (e1->st.st_size > e2->st.st_size) {
	return 1;
    } else if (e1->st.st_size < e2->st.st_size) {
	return -1;
    }
    return 0;
}

static int size_desc(const void *a, const void *b) {
    return -1 * size_asc(a, b);
}

int mvprint(int x, int y, struct CharBuf *cb, int width, struct tb_cell meta) {
    char *mbs;
    wchar_t wc;
    int n, len, w;

    mbtowc(NULL, NULL, 0);

    mbs = cb->buf;
    len = cb->len;

    while (len > 0) {
	n = mbtowc(&wc, mbs, len);
	if (n < 1) {
	    return -1;
	}

	w = WCWIDTH(wc);
	width -= w;
	if (width < 0) {
	    break;
	}

	tb_change_cell(x, y, wc, meta.fg, meta.bg);

	x += w;
	mbs += n;
	len -= n;
    }

    return x;
}

void panel_init() {
    MARKS = tree_new(key_cmp);
}

// @todo: AT LEAST TRY TO change `char` to `struct CharBuf`
struct Panel *panel_new(float fx, float fwidth, char *path) {
    struct Panel *pnl;

    pnl = ecalloc(1, sizeof(*pnl));

    pnl->hide_dot_files = true;

    panel_set_path(pnl, path);

    pnl->fx = fx;
    pnl->fwidth = fwidth;
    pnl->sort_method = SORT_ALPHA_ASC;

    panel_resize(pnl);

    return pnl;
}

void panel_set_sort_method(struct Panel *pnl, int sort_method) {
    assert(sort_method > -1 && sort_method < SORT_METHODS_NMEMB);
    pnl->sort_method = sort_method;
}


void panel_set_path(struct Panel *pnl, char *path) {
    for (int i = 0; i < pnl->n; ++i) {
	charbuf_free(pnl->entries[i].name);
    }
    free(pnl->entries);
    pnl->entries = NULL;
    pnl->n = 0;

    if (pnl->dir) {
	if (closedir(pnl->dir)) {
	    eprintf("Fail closing directory: %s", pnl->path->buf);
	}
	pnl->dir = NULL;
    }

    if (pnl->fp) {
	if(fclose(pnl->fp)) {
	    eprintf("Fail closing file: %s", pnl->path->buf);
	}
	pnl->fp = NULL;
    }

    pnl->cur = 0;
    pnl->top = 0;
    pnl->nsearch = 0;

    struct CharBuf *old = pnl->path;

    pnl->path = charbuf_new(path, CB_STR_NMEMB);

    if (old) {
	charbuf_free(old);
    }

    struct stat st;
    lstat(pnl->path->buf, &st);
    if (S_ISDIR(st.st_mode)) {
	if (!(pnl->dir = opendir(pnl->path->buf))) {
	    eprintf("Fail opening directory `%s`:", path);
	}

	int n = 0;
	struct dirent *dp;
	errno = 0;
	while ((dp = readdir(pnl->dir))) {
	    if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
		continue;
	    }

	    if (pnl->hide_dot_files && dp->d_name[0] == '.') {
		continue;
	    }

	    ++n;
	}
	if (errno) {
	    eprintf("Fail reading directory `%s`:", path);
	}

	pnl->n = n;
	pnl->entries = ecalloc(n, sizeof(*pnl->entries));

	rewinddir(pnl->dir);
	int i = 0;
	struct CharBuf *entry_path = &TMPCHARBUF;
	errno = 0;
	while (i < pnl->n && (dp = readdir(pnl->dir))) {
	    if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
		continue;
	    }

	    if (pnl->hide_dot_files && dp->d_name[0] == '.') {
		continue;
	    }

	    pnl->entries[i].name = charbuf_new(dp->d_name, CB_STR_NMEMB);

	    charbuf_set_len(entry_path, 0);
	    charbuf_addstr(entry_path, pnl->path->buf);
	    charbuf_addch(entry_path, '/');
	    charbuf_addstr(entry_path, pnl->entries[i].name->buf);

	    lstat(entry_path->buf, &pnl->entries[i].st);

	    ++i;
	}
	if (errno) {
	    eprintf("Fail reading directory `%s`:", pnl->path);
	}

	qsort(pnl->entries, pnl->n, sizeof(*pnl->entries),
	      sort[pnl->sort_method]);

	pnl->draw_fnc = draw_directory;
	pnl->search = realloc(pnl->search, sizeof(*pnl->search) * pnl->n);
    } else {
	pnl->draw_fnc = draw_file;
    }
}

void panel_toggle_hide_dot_files(struct Panel *pnl) {
    pnl->hide_dot_files = !pnl->hide_dot_files;
}

int panel_get_entries_number(struct Panel *pnl) {
    return pnl->n;
}

void panel_cursor_up(struct Panel *pnl) {
    if (pnl->cur > 0) {
	pnl->cur -= 1;
	if ((pnl->top > 0) && (pnl->cur - pnl->top < 3)) {
	    pnl->top -= 1;
	}
    }
}

void panel_cursor_down(struct Panel *pnl) {
    if (pnl->cur < pnl->n - 1) {
	pnl->cur += 1;
	if ((pnl->top + pnl->height < pnl->n) &&
	    (pnl->height - (pnl->cur - pnl->top + 1) < 3)) {
	    pnl->top += 1;
	}
    }
}

void panel_set_cursor(struct Panel *pnl, int pos) {
    assert(pos >= 0 && pos < pnl->n);

    pnl->cur = pos;

    panel_update_top(pnl);
}

void panel_update_top(struct Panel *pnl) {
    pnl->top = pnl->cur - pnl->height / 2;

    if (pnl->top + pnl->height > pnl->n) {
	pnl->top = pnl->n - pnl->height;
    }

    if (pnl->top < 0) {
	pnl->top = 0;
    }
}

int panel_get_cursor(struct Panel *pnl) {
    return pnl->cur;
}

// @todo: change `char` to `struct CharBuf`
char *panel_get_cursor_path(struct Panel *pnl, char *out) {
    size_t len;

    len = pnl->path->len;
    memcpy(out, pnl->path->buf, len);
    out[len] = '/';
    ++len;
    out[len] = '\0';
    memcpy(out + len, pnl->entries[pnl->cur].name->buf,
	   pnl->entries[pnl->cur].name->len + 1);

    return out;
}

void panel_get_cursor_stat(struct Panel *pnl, struct stat *out) {
    memcpy(out, &pnl->entries[pnl->cur].st, sizeof(pnl->entries[pnl->cur].st));
}

char *panel_get_path(struct Panel *pnl) {
    return pnl->path->buf;
}

void panel_draw(struct Panel *pnl) {
    pnl->draw_fnc(pnl);
}

static void draw_directory(struct Panel *pnl) {
    int top = pnl->top;
    struct tb_cell meta = {.bg = TB_DEFAULT};
    uint16_t mark;
    int len;

    struct CharBuf *entry_path = &TMPCHARBUF;

    charbuf_set_len(entry_path, 0);
    charbuf_addstr(entry_path, pnl->path->buf);
    charbuf_addch(entry_path, '/');
    len = entry_path->len;

    for (int y = 0; y < pnl->n && y < pnl->height; ++y) {
	if (y + top == pnl->cur) {
	    meta.fg = TB_RED | TB_REVERSE;
	} else {
	    meta.fg = TB_DEFAULT;
	}

	charbuf_set_len(entry_path, len);
	charbuf_addstr(entry_path, pnl->entries[y + top].name->buf);

	if (tree_exists(MARKS, entry_path)) {
	    mark = TB_MAGENTA;
	} else {
	    mark = TB_DEFAULT;
	}

	tb_change_cell(pnl->x, pnl->y + y, ' ', TB_DEFAULT, mark);

	mvprint(pnl->x + 1, pnl->y + y, pnl->entries[y + top].name,
		pnl->width, meta);
    }
}

static int panel_search(struct Panel *pnl) {
    int *p, i;
    static wchar_t name[WNAME_MAX];
    bool case_sensitive = false;

    if (pnl->search_pattern[0] == '\0') {
	pnl->nsearch = 0;
	return pnl->nsearch;
    }

    for (i = 0, p = pnl->search; i < pnl->n; ++i) {
	mbstowcs(name, pnl->entries[i].name->buf, NMEMB(name));
	// @todo: Move this logic in ds(exact_search variable)
	for (wchar_t *wp = pnl->search_pattern; *wp != '\0'; ++wp) {
	    if (iswupper(*wp)) {
		case_sensitive = true;
		break;
	    }
	}
	if (!case_sensitive) {
	    for (wchar_t *wp = name; *wp != '\0'; ++wp) {
		*wp = towlower(*wp);
	    }
	}
	if (wcsstr(name, pnl->search_pattern)) {
	    *p = i;
	    ++p;
	}
    }

    pnl->nsearch = p - pnl->search;
    return pnl->nsearch;
}

void panel_set_search_pattern(struct Panel *pnl, wchar_t *pattern) {
    wcscpy(pnl->search_pattern, pattern);
    panel_search(pnl);
}

int panel_search_next(struct Panel *pnl) {
    int *p;

    if (!pnl->nsearch && !panel_search(pnl)) {
	return pnl->cur;
    }

    for (p = pnl->search; p < pnl->search + pnl->nsearch; ++p) {
	if (*p > pnl->cur) {
	    return *p;
	}
    }

    return pnl->search[0];
}

int panel_search_prev(struct Panel *pnl) {
    int *p;

    if (!pnl->nsearch && !panel_search(pnl)) {
	return pnl->cur;
    }

    for (p = pnl->search + pnl->nsearch - 1; p >= pnl->search; --p) {
	if (*p < pnl->cur) {
	    return *p;
	}
    }

    return pnl->search[pnl->nsearch - 1];
}

static int key_cmp(void *key1, void *key2) {
    return charbuf_revstrcmp(key1, key2);
}

void key_free(void *key) {
    charbuf_free(key);
}

void key_delete(void *key) {
    remove(((struct CharBuf *)key)->buf);
    key_free(key);
}

void panel_delete_marked() {
    tree_foreach_free(MARKS, key_delete);
}

void panel_mark_cursor(struct Panel *pnl) {
    struct CharBuf *entry_path;
    int n;

    n = pnl->path->len + 1 + pnl->entries[pnl->cur].name->len + 1;
    entry_path = charbuf_new(NULL, n);

    charbuf_addstr(entry_path, pnl->path->buf);
    charbuf_addch(entry_path, '/');
    charbuf_addstr(entry_path, pnl->entries[pnl->cur].name->buf);

    if (!tree_exists(MARKS, entry_path)) {
	tree_insert(MARKS, entry_path);
    } else {
	tree_delete(MARKS, entry_path);
	charbuf_free(entry_path);
    }
}

static void draw_file(struct Panel *pnl) {
}

void panel_resize(struct Panel *pnl) {
    int h, w;

    h = tb_height();
    w = tb_width();

    pnl->width = (float)w * pnl->fwidth;
    pnl->x = (float)w * pnl->fx;

    pnl->height = h - PANEL_VERTICAL_MARGINS;
    pnl->y = PANEL_Y_POSITION;

    panel_update_top(pnl);
}

void panel_free(struct Panel *pnl) {
    int i;

    closedir(pnl->dir); // @todo: add error return codes
    if (pnl->fp) {
	fclose(pnl->fp);
    }
    if (pnl->search) {
	free(pnl->search);
    }
    for (i = 0; i < pnl->n; ++i) {
	charbuf_free(pnl->entries[i].name);
    }
    charbuf_free(pnl->path);
    free(pnl->entries);
    free(pnl);
}

void panel_finalize() {
    tree_foreach_free(MARKS, key_free);
    tree_free(MARKS);
}

char *mode_to_str(mode_t mode) {
    static char str[sizeof(SAMPLE_MODE_STR)];

    snprintf(str, NMEMB(str), "%c%c%c%c%c%c%c%c%c%c",
	     S_ISDIR(mode) ? 'd' : '-',
	     S_IRUSR & mode ? 'r' : '-', S_IWUSR & mode ? 'w' : '-',
	     S_IXUSR & mode ?  (S_ISUID & mode ? 's' : 'x') :
	 	(S_ISUID & mode ? 'S' : '-'),
	     S_IRGRP & mode ? 'r' : '-', S_IWGRP & mode ? 'w' : '-',
	     S_IXGRP & mode ?
	 	(S_ISGID & mode ? 's' : 'x') :
	 	(S_ISGID & mode ? 'S' : '-'),
	     S_IROTH & mode ? 'r' : '-', S_IWOTH & mode ? 'w' : '-',
	     S_IXOTH & mode ?
	 	(S_ISVTX & mode ? 't' : 'x') :
	 	(S_ISVTX & mode ? 'T' : '-'));
    return str;
}

struct Unit {
    long long measure;
    char prefix;
};

#define KIB 1024LL

struct Unit UNITS[] = {
    {1LL, 'B'},
    {KIB, 'K'},
    {KIB * KIB, 'M'},
    {KIB * KIB * KIB, 'G'},
    {KIB * KIB * KIB * KIB, 'T'},
    {KIB * KIB * KIB * KIB * KIB, 'P'},
    {KIB * KIB * KIB * KIB * KIB * KIB, 'E'}
};

char *size_to_str(off_t sz) {
    static char str[sizeof(SAMPLE_SIZE_STR)];
    double val;
    int n;

    for (size_t i = 0; i < NMEMB(UNITS) - 1; ++i) {
	if (sz < UNITS[i + 1].measure) {
	    val = (double)sz / UNITS[i].measure;

	    // Only 3 symbos per a number, also
	    // we don't want a traling decimal point.
	    if (val >= 10) {
	        val = round(val);
		n = 0;
	    } else {
		n = 1;
	    }

	    snprintf(str, NMEMB(str), "%3.*f%c", n, val, UNITS[i].prefix);
	    return str;
	}
    }

    return "huge";
}
