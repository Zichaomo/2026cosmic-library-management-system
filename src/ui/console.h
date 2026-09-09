#ifndef LMS_UI_CONSOLE_H
#define LMS_UI_CONSOLE_H

#include "../types.h"

/* ============ 控制台平台封装（Win32，集中平台相关调用，PRD §6.4） ============ */

/* 初始化：UTF-8 代码页（FR-207）、鼠标输入模式、窗口标题 */
Result console_init(void);

/* 退出前恢复控制台状态 */
void console_shutdown(void);

/* 获取窗口尺寸（列 x 行），失败返回 false */
bool console_get_size(int *cols, int *rows);

/* 移动光标到 (col, row)，0 起始 */
void console_set_cursor(int col, int row);

/* 设置前景/背景颜色（Windows 控制台属性值） */
void console_set_color(unsigned short attr);

/* 在指定位置输出 UTF-8 文本 */
void console_write_at(int col, int row, const char *text);

/* 绘制框线矩形（┌ ─ ┐ │ └ ┘），x/y 左上角，w/h 含边框 */
void console_draw_box(int x, int y, int w, int h);

/* 清屏（填当前背景色） */
void console_clear(void);

/* 将后台缓冲一次性输出到屏幕（每帧绘制结束后调用，消除闪烁） */
void console_present(void);

/* 是否运行在真实控制台环境（标准输入为控制台句柄）。
   管道/重定向运行返回 false，用于降级为非交互模式。 */
bool console_is_tty(void);

#endif /* LMS_UI_CONSOLE_H */
