#ifndef LMS_UI_WIDGET_H
#define LMS_UI_WIDGET_H

#include "../types.h"
#include "input.h"

/* ============ 控件：按钮 / 列表 / 表单 / 标签 / 弹窗（FR-202/FR-203） ============ */

typedef enum {
    W_BUTTON,
    W_LIST,
    W_FORM_FIELD,
    W_LABEL,
    W_DIALOG
} WidgetType;

typedef struct Widget Widget;

/* 命中检测：点 (x,y) 是否落在控件矩形内 */
bool widget_hit(const Widget *w, int x, int y);

/* 绘制控件 */
void widget_draw(const Widget *w);

/* 处理输入事件，返回 true 表示已消费 */
bool widget_handle(Widget *w, const InputEvent *ev);

/* ---- 便捷构造函数（实现后续填充） ---- */
Widget *widget_button(int x, int y, int w, const char *label);
Widget *widget_list(int x, int y, int w, int h);
Widget *widget_label(int x, int y, const char *text);

/* 释放控件 */
void widget_free(Widget *w);

/* ---- 控件状态 ---- */
/* 设置悬停/选中高亮（由页面在绘制前设置） */
void widget_set_hover(Widget *w, bool hover);
bool widget_get_hover(const Widget *w); /* 查询悬停状态（用于判断是否需要重绘） */
void widget_set_selected(Widget *w, bool selected);

/* 列表数据（复制字符串；items 可为 NULL 表示清空） */
void widget_list_set_items(Widget *w, const char *const *items, size_t count);
void widget_list_set_sel(Widget *w, int sel);
int widget_list_sel(const Widget *w);
/* 滚动（正=向下），自动夹取 */
void widget_list_scroll(Widget *w, int delta);

/* 获取控件位置与尺寸（用于命中检测/布局） */
void widget_get_rect(const Widget *wd, int *x, int *y, int *w, int *h);

#endif /* LMS_UI_WIDGET_H */
