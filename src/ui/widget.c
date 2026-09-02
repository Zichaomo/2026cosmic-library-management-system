/* 控件库 - 界面层（按钮/标签/列表） */
#include "widget.h"

#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "theme.h"

#define MAX_LIST_ITEMS 512

struct Widget {
    WidgetType type;
    int x, y, w, h;
    char *label;
    bool hover;
    bool selected;
    /* 列表数据 */
    char **items;
    size_t item_count;
    int sel;    /* 选中索引（-1=无） */
    int scroll; /* 首行偏移 */
};

/* 前景/背景互换（反显） */
static unsigned short invert(unsigned short a) {
    return (unsigned short)(((a & 0x0F) << 4) | ((a & 0xF0) >> 4));
}

bool widget_hit(const Widget *w, int x, int y) {
    if (!w) return false;
    return x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h;
}

static void draw_button(const Widget *w) {
    char line[256];
    int n = w->w > 250 ? 250 : w->w;
    int avail, start;
    size_t len = w->label ? strlen(w->label) : 0;
    unsigned short attr = (unsigned short)(g_theme.bg | g_theme.fg);

    if (n < 3) n = 3;
    if (w->hover || w->selected) attr = invert(attr);

    memset(line, ' ', (size_t)n);
    line[0] = '[';
    line[n - 1] = ']';
    avail = n - 2;
    start = 1 + (avail - (int)len) / 2;
    if (start < 1) start = 1;
    if (len > (size_t)avail) len = (size_t)avail;
    memcpy(line + start, w->label, len);
    line[n] = '\0';

    console_set_color(attr);
    console_write_at(w->x, w->y, line);
    console_set_color((unsigned short)(g_theme.bg | g_theme.fg));
}

static void draw_label(const Widget *w) {
    if (!w->label) return;
    console_write_at(w->x, w->y, w->label);
}

static void draw_list(const Widget *w) {
    unsigned short base = (unsigned short)(g_theme.bg | g_theme.fg);
    unsigned short hl = invert(base);
    int r;

    for (r = 0; r < w->h; r++) {
        int idx = w->scroll + r;
        const char *txt = (idx >= 0 && idx < (int)w->item_count && w->items[idx])
                              ? w->items[idx]
                              : "";
        char line[512];
        size_t len = strlen(txt);
        int n = w->w > 500 ? 500 : w->w;
        int i;
        if (len > (size_t)n) len = (size_t)n;
        memcpy(line, txt, len);
        for (i = (int)len; i < n; i++) line[i] = ' ';
        line[n] = '\0';
        console_set_color(idx == w->sel ? hl : base);
        console_write_at(w->x, w->y + r, line);
    }
    console_set_color(base);
}

void widget_draw(const Widget *w) {
    if (!w) return;
    switch (w->type) {
    case W_BUTTON: draw_button(w); break;
    case W_LIST: draw_list(w); break;
    case W_LABEL: draw_label(w); break;
    default: break; /* W_FORM_FIELD / W_DIALOG 待实现 */
    }
}

bool widget_handle(Widget *w, const InputEvent *ev) {
    (void)w;
    (void)ev;
    /* 交互由页面层统一处理（命中检测 + 分发） */
    return false;
}

static Widget *widget_new(WidgetType type, int x, int y, int w, int h,
                          const char *label) {
    Widget *wd = calloc(1, sizeof *wd);
    if (!wd) return NULL;
    wd->type = type;
    wd->x = x;
    wd->y = y;
    wd->w = w;
    wd->h = h;
    wd->sel = -1;
    if (label) wd->label = strdup(label);
    return wd;
}

Widget *widget_button(int x, int y, int w, const char *label) {
    return widget_new(W_BUTTON, x, y, w, 1, label);
}

Widget *widget_list(int x, int y, int w, int h) {
    return widget_new(W_LIST, x, y, w, h, NULL);
}

Widget *widget_label(int x, int y, const char *text) {
    return widget_new(W_LABEL, x, y, text ? (int)strlen(text) : 0, 1, text);
}

void widget_free(Widget *w) {
    size_t i;
    if (!w) return;
    free(w->label);
    for (i = 0; i < w->item_count; i++) free(w->items[i]);
    free(w->items);
    free(w);
}

void widget_set_hover(Widget *w, bool hover) {
    if (w) w->hover = hover;
}

void widget_set_selected(Widget *w, bool selected) {
    if (w) w->selected = selected;
}

void widget_list_set_items(Widget *w, const char *const *items, size_t count) {
    size_t i;
    if (!w) return;
    for (i = 0; i < w->item_count; i++) free(w->items[i]);
    free(w->items);
    w->items = NULL;
    w->item_count = 0;
    w->sel = -1;
    w->scroll = 0;
    if (!items || count == 0) return;
    if (count > MAX_LIST_ITEMS) count = MAX_LIST_ITEMS;
    w->items = calloc(count, sizeof(char *));
    if (!w->items) return;
    for (i = 0; i < count; i++) w->items[i] = strdup(items[i] ? items[i] : "");
    w->item_count = count;
    w->sel = 0;
}

void widget_list_set_sel(Widget *w, int sel) {
    if (!w) return;
    if (sel < -1) sel = -1;
    if (sel >= (int)w->item_count) sel = (int)w->item_count - 1;
    w->sel = sel;
    if (sel < w->scroll) w->scroll = sel;
    else if (sel >= w->scroll + w->h) w->scroll = sel - w->h + 1;
}

int widget_list_sel(const Widget *w) {
    return w ? w->sel : -1;
}

void widget_list_scroll(Widget *w, int delta) {
    int max;
    if (!w) return;
    max = (int)w->item_count - w->h;
    if (max < 0) max = 0;
    w->scroll += delta;
    if (w->scroll < 0) w->scroll = 0;
    if (w->scroll > max) w->scroll = max;
}

void widget_get_rect(const Widget *wd, int *x, int *y, int *ww, int *hh) {
    if (!wd) {
        if (x) *x = 0;
        if (y) *y = 0;
        if (ww) *ww = 0;
        if (hh) *hh = 0;
        return;
    }
    if (x) *x = wd->x;
    if (y) *y = wd->y;
    if (ww) *ww = wd->w;
    if (hh) *hh = wd->h;
}
