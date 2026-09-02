/* 输入事件解析 - 界面层（Win32: ReadConsoleInputW，PRD §6.3） */
#include "input.h"

#include <string.h>
#include <windows.h>

/* 读取一个输入事件：
   - 等待 timeout_ms（0 = 无限）；超时返回 false
   - 跳过不关心的事件（菜单事件、按键抬起、无按键的鼠标移动等） */
bool input_read(InputEvent *ev, unsigned int timeout_ms) {
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD rec;
    DWORD n = 0;

    if (!ev) return false;
    memset(ev, 0, sizeof *ev);

    if (WaitForSingleObject(hin, timeout_ms == 0 ? INFINITE : timeout_ms) != WAIT_OBJECT_0)
        return false; /* 超时 */

    for (;;) {
        if (!ReadConsoleInputW(hin, &rec, 1, &n) || n == 0)
            return false;

        switch (rec.EventType) {
        case KEY_EVENT: {
            KEY_EVENT_RECORD *k = &rec.Event.KeyEvent;
            if (!k->bKeyDown) continue; /* 只处理按下，忽略抬起/自动重复 */
            ev->type = EV_KEY;
            ev->key_down = true;
            ev->vk = k->wVirtualKeyCode;
            ev->ch = k->uChar.UnicodeChar;
            return true;
        }
        case MOUSE_EVENT: {
            MOUSE_EVENT_RECORD *m = &rec.Event.MouseEvent;
            ev->x = m->dwMousePosition.X;
            ev->y = m->dwMousePosition.Y;
            ev->button = (unsigned int)(m->dwButtonState & 0xFFFF); /* 低字=按键状态 */
            if (m->dwEventFlags & MOUSE_WHEELED) {
                ev->type = EV_MOUSE_WHEEL;
                ev->wheel_delta = ((SHORT)(m->dwButtonState >> 16)) / WHEEL_DELTA;
            } else {
                ev->type = EV_MOUSE;
            }
            return true;
        }
        case WINDOW_BUFFER_SIZE_EVENT:
            ev->type = EV_RESIZE;
            return true;
        default:
            break; /* 其他事件跳过 */
        }
    }
}
