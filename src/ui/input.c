/* 输入事件解析 - 界面层（骨架 stub） */
#include "input.h"

#include <string.h>

/* TODO(M1): 用 ReadConsoleInputW 读取 MOUSE_EVENT / KEY_EVENT /
   WINDOW_BUFFER_SIZE_EVENT（PRD §6.3），映射为 InputEvent；
   鼠标滚轮检测 dwEventFlags & MOUSE_WHEELED（FR-203）。 */
bool input_read(InputEvent *ev, unsigned int timeout_ms) {
    (void)timeout_ms;
    if (!ev) return false;
    memset(ev, 0, sizeof *ev);
    ev->type = EV_NONE;
    return false; /* 骨架阶段：暂无事件源 */
}
