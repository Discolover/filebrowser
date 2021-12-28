#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <pwd.h>

#include <termbox.h>

#include "tui.h"
#include "charbuf.h"
#include "util.h"

FILE *LOG;

char BUF[BUF_SZ];
struct CharBuf TMPCHARBUF = {.buf = BUF, .len = 0, .n = NMEMB(BUF)};

int MAX_VALUES[] = {
    PATH_MAX, NAME_MAX, HOST_NAME_MAX, LOGIN_NAME_MAX
};

char *sort_method_to_str[SORT_METHODS_NMEMB] = {
    "SORT_ALPHA_ASC",
    "SORT_ALPHA_DESC",
    "SORT_SIZE_ASC",
    "SORT_SIZE_DESC"
};

void init() {
    LOG = fopen("log", "w");
    setlocale(LC_ALL, "");
    tb_init();
    panel_init();

    for (size_t i = 0; i < NMEMB(MAX_VALUES); ++i) {
	fprintf(LOG, "%d\n", MAX_VALUES[i]);
	if (BUF_SZ < MAX_VALUES[i]) {
	    eprintf("BUG");
	}
    }
}

void finalize() {
    fclose(LOG);
    tb_shutdown();
    panel_finalize();
}

void get_input(wchar_t *buf, size_t n) {
    wchar_t *p = buf;
    int y = tb_height() - 1, x = 0;
    struct tb_event ev;

    for (int i = 0; i < tb_width(); ++i) {
	tb_change_cell(x + i, y, ' ', TB_DEFAULT, TB_DEFAULT);
    }

    tb_change_cell(x++, y, '/', TB_DEFAULT, TB_DEFAULT);
    tb_present();

    while (tb_poll_event(&ev) == TB_EVENT_KEY) {
	if (ev.key == TB_KEY_ENTER) {
	    break;
	}
	if (ev.key == TB_KEY_ESC) {
	    p = buf;
	    break;
	}
	if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) {
	    if (p > buf)
		--p;
	    if (x > 1)
		--x;
	    tb_change_cell(x, y, L' ', TB_DEFAULT, TB_DEFAULT);
	} else {
	    *p++ = ev.ch;
	    tb_change_cell(x++, y, ev.ch, TB_DEFAULT, TB_DEFAULT);
	}
	tb_present();
    }
    *p = '\0';
}

int get_sort_method() {
    struct tb_event ev;
    int sort_method = -1;

    if (tb_poll_event(&ev) == TB_EVENT_KEY) {
	switch (ev.ch) {
	case 'a':
	    sort_method = SORT_ALPHA_ASC;
	    break;
	case 'A':
	    sort_method = SORT_ALPHA_DESC;
	    break;
	case 's':
	    sort_method = SORT_SIZE_ASC;
	    break;
	case 'S':
	    sort_method = SORT_SIZE_DESC;
	    break;
	}
    }

    return sort_method;
}

int main() {
    char buf[PATH_MAX];
    wchar_t input[WNAME_MAX];
    struct Panel *left, *main, *right;
    bool quit = false;
    struct tb_event ev;

    struct stat st;

    struct passwd *pwd;

    int len, n = 0;
    struct tb_cell meta = {.fg = TB_DEFAULT, .bg = TB_DEFAULT};

    init();

    pwd = getpwuid(getuid());

    len = strnlen(pwd->pw_name, LOGIN_NAME_MAX - 1);
    n += len;

    n += 1; // for '@'

    gethostname(TMPCHARBUF.buf, TMPCHARBUF.n);
    len = strnlen(TMPCHARBUF.buf, HOST_NAME_MAX - 1);
    TMPCHARBUF.buf[len] = '\0';
    TMPCHARBUF.len = len;
    n += TMPCHARBUF.len;

    n += 1; // for ':'

    n += PATH_MAX;

    struct CharBuf *topinfo = charbuf_new(NULL, n);
    struct CharBuf *fileinfo;
    struct CharBuf *entries_number_info = charbuf_new(NULL, 0);

    fileinfo = charbuf_new(NULL, MODE_STR_SZ + SIZE_STR_SZ + TIME_STR_SZ);

    charbuf_addstr(topinfo, pwd->pw_name);
    charbuf_addch(topinfo, '@');
    charbuf_addstr(topinfo, TMPCHARBUF.buf);
    charbuf_addch(topinfo, ':');

    len = topinfo->len;

    int h = tb_height() - 2;
    int y = 1;

    left = panel_new(0, y, 0.2, h, dirname(getcwd(buf, NMEMB(buf))));
    main = panel_new(0.2, y, 0.5, h, getcwd(buf, NMEMB(buf)));
    right = panel_new(0.7, y, 0.3, h, panel_get_cursor_path(main, buf));

    while (!quit) {
	panel_draw(left);
	panel_draw(main);
	panel_draw(right);

	topinfo->len = len;
	topinfo->buf[len] = '\0';

	panel_get_cursor_path(main, buf);
	charbuf_addstr(topinfo, buf);

	mvprint(0, 0, topinfo->buf, tb_width(), meta);

	fileinfo->len = 0;
	fileinfo->buf[0] = '\0';

	panel_get_cursor_stat(main, &st);

	charbuf_addstr(fileinfo, mode_to_str(st.st_mode));
	charbuf_addch(fileinfo, ' ');
	charbuf_addstr(fileinfo, size_to_str(st.st_size));
	charbuf_addch(fileinfo, ' ');
	charbuf_addstr(fileinfo, ctime(&st.st_mtim.tv_sec));
	charbuf_rstrip(fileinfo);
	mvprint(0, tb_height() - 1, fileinfo->buf, tb_width(), meta);

	snprintf(TMPCHARBUF.buf, TMPCHARBUF.n, "%d/%d",
		 panel_get_cursor(main) + 1,
		 panel_get_entries_number(main));
	TMPCHARBUF.len = strnlen(TMPCHARBUF.buf, TMPCHARBUF.n - 1);
	TMPCHARBUF.buf[TMPCHARBUF.len] = '\0';

	mvprint(tb_width() - TMPCHARBUF.len, tb_height() - 1,
		TMPCHARBUF.buf, TMPCHARBUF.len, meta);

	//charbuf_expand(entries_number_info, TMPCHARBUF.len + 1);


	tb_present();

	tb_poll_event(&ev);
	if (ev.type == TB_EVENT_KEY) {
	    switch (ev.ch) {
	    case 'q':
		quit = true;
		break;
	    case 'g':
		panel_set_cursor(main, 0);
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case 'G':
		panel_set_cursor(main, -1);
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case 'j':
		panel_cursor_down(main);
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case 'k':
		panel_cursor_up(main);
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case 'l':
		panel_set_path(left, panel_get_path(main));
		panel_set_path(main, panel_get_path(right));
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case 'h':
		panel_set_path(right, panel_get_path(main));
		panel_set_path(main, panel_get_path(left));
		strcpy(buf, panel_get_path(left));
		panel_set_path(left, dirname(buf));
		break;
	    case '/':
		get_input(input, NMEMB(input));
		panel_set_search_pattern(main, input);
		panel_set_cursor(main, panel_search_next(main));
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case 'n':
		panel_set_cursor(main, panel_search_next(main));
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case 'N':
		panel_set_cursor(main, panel_search_prev(main));
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case 'z':
		// trick to center relative to the cursor(current entry)
		panel_set_cursor(main, panel_get_cursor(main));
		break;
	    case 's':
		int sm = get_sort_method();
		if (sm > -1) {
		    panel_set_sort_method(left, sm);
		    panel_set_sort_method(main, sm);
		    panel_set_sort_method(right, sm);

		    strcpy(buf, panel_get_path(left));
		    panel_set_path(left, buf);

		    strcpy(buf, panel_get_path(main));
		    panel_set_path(main, buf);

		    strcpy(buf, panel_get_path(right));
		    panel_set_path(right, buf);
		}
		break;
	    case 'D':
		panel_delete_marked();
		// redraw directories
		strcpy(buf, panel_get_path(left));
		panel_set_path(left, buf);

		strcpy(buf, panel_get_path(main));
		panel_set_path(main, buf);

		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
		break;
	    case '.':
		panel_toggle_hide_dot_files(left);
		panel_toggle_hide_dot_files(main);
		panel_toggle_hide_dot_files(right);

		strcpy(buf, panel_get_path(left));
		panel_set_path(left, buf);

		strcpy(buf, panel_get_path(main));
		panel_set_path(main, buf);

		strcpy(buf, panel_get_path(right));
		panel_set_path(right, buf);
		break;
	    }
	    switch (ev.key) {
	    case TB_KEY_SPACE:
		panel_mark_cursor(main);
		break;
	    }
	} else if (ev.type == TB_EVENT_RESIZE) {
	    panel_resize(left);
	    panel_resize(main);
	    panel_resize(right);
	} else if (ev.type == TB_EVENT_MOUSE) {
	    ;
	}
	tb_clear();
    }

    panel_free(left);
    panel_free(main);
    panel_free(right);

    finalize();

    return 0;
}
