#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <pwd.h>

#include <termbox.h>

#include "tui.h"

FILE *LOG;

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

    tb_change_cell(x++, y, '/', TB_DEFAULT, TB_DEFAULT);
    tb_present();

    while (tb_poll_event(&ev) == TB_EVENT_KEY) {
	if (ev.key == TB_KEY_ENTER)
	    break;
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

int main() {
    char buf[PATH_MAX];
    wchar_t input[WNAME_MAX];
    struct Panel *left, *main, *right;
    bool quit = false;
    struct tb_event ev;

    init();

    int h = tb_height() - 2;
    int y = 1;
    left = panel_new(0, y, 0.2, h, dirname(getcwd(buf, NMEMB(buf))));
    main = panel_new(0.2, y, 0.5, h, getcwd(buf, NMEMB(buf)));
    right = panel_new(0.7, y, 0.3, h, panel_get_cursor_path(main, buf));

    while (!quit) {
	panel_draw(left);
	panel_draw(main);
	panel_draw(right);
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
	    case 'D':
		panel_delete_marked();
		// redraw directories
		panel_set_path(left, panel_get_path(left));
		panel_set_path(main, panel_get_path(main));
		panel_get_cursor_path(main, buf);
		panel_set_path(right, buf);
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
