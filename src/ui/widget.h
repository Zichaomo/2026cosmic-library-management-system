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

#endif /* LMS_UI_WIDGET_H */
