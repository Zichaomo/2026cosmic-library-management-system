#ifndef LMS_UI_INPUT_H
#define LMS_UI_INPUT_H

#include "../types.h"

/* ============ 键盘 + 鼠标事件解析（FR-202/FR-203，PRD §4.2） ============ */

typedef enum {
    EV_NONE,
    EV_KEY,          /* 按键 */
    EV_MOUSE,        /* 鼠标按下/抬起/移动 */
    EV_MOUSE_WHEEL,  /* 鼠标滚轮 */
    EV_RESIZE,       /* 窗口尺寸变化 */
    EV_QUIT          /* Ctrl+C 等退出信号 */
} EventType;

typedef struct {
    EventType type;
    /* 键盘 */
    bool key_down;      /* true=按下 false=抬起 */
    unsigned int vk;    /* 虚拟键码（VK_*） */
    wchar_t ch;         /* 字符（0 = 无） */
    /* 鼠标 */
    int x, y;           /* 字符坐标 */
    unsigned int button; /* 左/右/中键位掩码 */
    int wheel_delta;     /* 滚轮步数（正=向上） */
} InputEvent;

/* 读取一个输入事件（阻塞，最多等 timeout_ms；0 = 无限）。
   返回 false 表示读取失败/超时。 */
bool input_read(InputEvent *ev, unsigned int timeout_ms);

#endif /* LMS_UI_INPUT_H */
