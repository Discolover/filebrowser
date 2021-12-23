#include <wchar.h>

#define WNAME_MAX (NAME_MAX / sizeof(wchar_t))
#define WPATH_MAX (PATH_MAX / sizeof(wchar_t))
#define NMEMB(ARRAY) (sizeof ARRAY / sizeof ARRAY[0])
#define WCWIDTH(WC) ((WC) != '\t' ? wcwidth((WC)) : 8)

enum {
    SORT_ALPHA_ASC,
    SORT_ALPHA_DESC,
    SORT_SIZE_ASC,
    SORT_SIZE_DESC,
    SORT_METHODS_NMEMB
};

struct Panel;

int mvprint(int x, int y, char *mbs, int width, struct tb_cell meta);

struct Panel *panel_new(float x, int y, float fwidth, int height, char *path);
void panel_draw(struct Panel *pnl);
void panel_free(struct Panel *pnl);
void panel_resize(struct Panel *pnl);
char *panel_get_cursor_path(struct Panel *pnl, char *out);
void panel_set_path(struct Panel *pnl, char *path);
char *panel_get_path(struct Panel *pnl);
void panel_set_cursor(struct Panel *pnl, int pos);
int panel_search_next(struct Panel *pnl);
int panel_search_prev(struct Panel *pnl);
void panel_set_search_pattern(struct Panel *pnl, wchar_t *pattern);
void panel_mark_cursor(struct Panel *pnl);
void panel_delete_marked();
void panel_set_sort_method(struct Panel *pnl, int sort_method);

void panel_finalize();
void panel_init();

int panel_get_cursor(struct Panel *pnl);

void panel_cursor_up(struct Panel *pnl);
void panel_cursor_down(struct Panel *pnl);
