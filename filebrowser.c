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

void init() {
    LOG = fopen("log", "w");
    setlocale(LC_ALL, "");
    tb_init();
    panel_init();
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
	    if (p <= buf) {
		break;
	    }
	    --p;

	    int w = WCWIDTH(*p);
	    x -= w;

	    for (int i = 0; i < w; ++i) {
		tb_change_cell(x + i, y, L' ', TB_DEFAULT, TB_DEFAULT);
	    }
	} else {
	    *p++ = ev.ch;
	    tb_change_cell(x, y, ev.ch, TB_DEFAULT, TB_DEFAULT);
	    x += WCWIDTH(ev.ch);
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
    wchar_t input[WNAME_MAX];
    struct Panel *left, *main, *right;
    bool quit = false;
    struct tb_event ev;
    struct stat st;
    struct passwd *pwd;
    int n = 0, x;
    struct tb_cell meta = {.fg = TB_DEFAULT, .bg = TB_DEFAULT};
    struct CharBuf *tmp = &TMPCHARBUF, *idinfo;

    init();

    pwd = getpwuid(getuid());
    n += strnlen(pwd->pw_name, LOGIN_NAME_MAX - 1);
    gethostname(tmp->buf, tmp->n);
    charbuf_set_len(tmp, strnlen(tmp->buf, HOST_NAME_MAX - 1));
    n += tmp->len;
    n += 3; // for '@', ':' and '\0'
    idinfo = charbuf_new(NULL, n);
    charbuf_addstr(idinfo, pwd->pw_name);
    charbuf_addch(idinfo, '@');
    charbuf_addstr(idinfo, tmp->buf);
    charbuf_addch(idinfo, ':');

    left = panel_new(0, 0.2, dirname(getcwd(tmp->buf, tmp->n)));
    main = panel_new(0.2, 0.5, getcwd(tmp->buf, tmp->n));
    right = panel_new(0.7, 0.3, panel_get_cursor_path(main, tmp));

    while (!quit) {
	panel_draw(left);
	panel_draw(main);
	panel_draw(right);

	x = mvprint(0, 0, idinfo, tb_width(), meta);
	panel_get_cursor_path(main, tmp);
	mvprint(x, 0, tmp, tb_width() - x, meta);

	charbuf_set_len(tmp, 0);
	panel_get_cursor_stat(main, &st);
	charbuf_addstr(tmp, mode_to_str(st.st_mode));
	charbuf_addch(tmp, ' ');
	charbuf_addstr(tmp, size_to_str(st.st_size));
	charbuf_addch(tmp, ' ');
	charbuf_addstr(tmp, ctime(&st.st_mtim.tv_sec));
	charbuf_rstrip(tmp);
	mvprint(0, tb_height() - 1, tmp, tb_width(), meta);

	snprintf(tmp->buf, tmp->n, "%d/%d", panel_get_cursor(main) + 1,
		 panel_get_entries_number(main));
	charbuf_set_len(tmp, strnlen(tmp->buf, tmp->n - 1));
	mvprint(tb_width() - tmp->len, tb_height() - 1, tmp,
		tmp->len, meta);

	tb_present();

	tb_poll_event(&ev);
	if (ev.type == TB_EVENT_KEY) {
	    switch (ev.ch) {
	    case 'q':
		quit = true;
		break;
	    case 'g':
		panel_set_cursor(main, 0);
		panel_set_path(right, panel_get_cursor_path(main, tmp));
		break;
	    case 'G':
		panel_set_cursor(main, panel_get_entries_number(main) - 1);
		panel_set_path(right, panel_get_cursor_path(main, tmp));
		break;
	    case 'j':
		panel_cursor_down(main);
		panel_set_path(right, panel_get_cursor_path(main, tmp));
		break;
	    case 'k':
		panel_cursor_up(main);
		panel_set_path(right, panel_get_cursor_path(main, tmp));
		break;
	    case 'l':
		panel_set_path(left, panel_get_path(main));
		panel_set_path(main, panel_get_path(right));
		panel_set_path(right, panel_get_cursor_path(main, tmp));
		break;
	    case 'h':
		panel_set_path(right, panel_get_path(main));
		panel_set_path(main, panel_get_path(left));
		panel_set_path(left, dirname(panel_get_path(left)));
		break;
	    case '/':
		get_input(input, NMEMB(input));
		panel_set_search_pattern(main, input);
		panel_set_cursor(main, panel_search_next(main));
		panel_set_path(right, panel_get_cursor_path(main, tmp));
		break;
	    case 'n':
		panel_set_cursor(main, panel_search_next(main));
		panel_set_path(right, panel_get_cursor_path(main, tmp));
		break;
	    case 'N':
		panel_set_cursor(main, panel_search_prev(main));
		panel_set_path(right, panel_get_cursor_path(main, tmp));
		break;
	    case 'z':
		panel_update_top(main);
		break;
	    case 's':
		int sm = get_sort_method();
		if (sm < 0) {
		    break;
		}
		panel_set_sort_method(left, sm);
		panel_set_sort_method(main, sm);
		panel_set_sort_method(right, sm);
		panel_set_path(left, panel_get_path(left));
		panel_set_path(main, panel_get_path(main));
		panel_set_path(right, panel_get_path(right));
		break;
	    case 'D':
		panel_delete_marked();
		panel_set_path(left, panel_get_path(left));
		panel_set_path(main, panel_get_path(main));
		panel_set_path(right, panel_get_path(right));
		break;
	    case '.':
		panel_toggle_hide_dot_files(left);
		panel_toggle_hide_dot_files(main);
		panel_toggle_hide_dot_files(right);
		panel_set_path(left, panel_get_path(left));
		panel_set_path(main, panel_get_path(main));
		panel_set_path(right, panel_get_path(right));
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

    charbuf_free(idinfo);

    panel_free(left);
    panel_free(main);
    panel_free(right);

    finalize();

    return 0;
}
