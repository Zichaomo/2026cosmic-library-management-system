/* 控件库 - 界面层（骨架：结构与命中检测可用，绘制/交互待 M1） */
#include "widget.h"

#include <stdlib.h>
#include <string.h>

struct Widget {
    WidgetType type;
    int x, y, w, h;
    char *label; /* 按钮文本 / 标签文本 */
    /* TODO(M1): 列表数据与滚动位置、表单字段、弹窗内容 */
};

bool widget_hit(const Widget *w, int x, int y) {
    if (!w) return false;
    return x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h;
}

void widget_draw(const Widget *w) {
    (void)w;
    /* TODO(M1): 按 type 绘制（button 画框+文字；list 画行；label 画文本） */
}

bool widget_handle(Widget *w, const InputEvent *ev) {
    (void)w;
    (void)ev;
    /* TODO(M1): 悬停高亮、单击命中、滚轮滚动（FR-202/FR-203） */
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
    Widget *wd = widget_new(W_LABEL, x, y, text ? (int)strlen(text) : 0, 1, text);
    return wd;
}

void widget_free(Widget *w) {
    if (!w) return;
    free(w->label);
    free(w);
}
