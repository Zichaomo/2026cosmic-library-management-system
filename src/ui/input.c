/* 输入事件解析 - 界面层（Win32: ReadConsoleInputW，PRD §6.3） */
#include "input.h"

#include <string.h>
#include <windows.h>

/* 把一条 INPUT_RECORD 翻译为 InputEvent；返回 false 表示该事件可忽略 */
static bool translate(const INPUT_RECORD *rec, InputEvent *ev) {
    switch (rec->EventType) {
    case KEY_EVENT: {
        const KEY_EVENT_RECORD *k = &rec->Event.KeyEvent;
        if (!k->bKeyDown) return false; /* 只处理按下，忽略抬起 */
        ev->type = EV_KEY;
        ev->key_down = true;
        ev->vk = k->wVirtualKeyCode;
        ev->ch = k->uChar.UnicodeChar;
        return true;
    }
    case MOUSE_EVENT: {
        const MOUSE_EVENT_RECORD *m = &rec->Event.MouseEvent;
        ev->x = m->dwMousePosition.X;
        ev->y = m->dwMousePosition.Y;
        ev->button = (unsigned int)(m->dwButtonState & 0xFFFF);
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
        return false; /* 菜单/焦点等事件忽略 */
    }
}

/* 等待并读取一个事件；wait_ms 为等待毫秒数，infinite 表示无限等待 */
static bool read_event(InputEvent *ev, unsigned int wait_ms, bool infinite) {
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD rec;
    DWORD n = 0;

    if (!ev) return false;
    memset(ev, 0, sizeof *ev);

    if (WaitForSingleObject(hin, infinite ? INFINITE : wait_ms) != WAIT_OBJECT_0)
        return false; /* 超时 / 无事件 */

    for (;;) {
        if (!ReadConsoleInputW(hin, &rec, 1, &n) || n == 0)
            return false;
        if (translate(&rec, ev)) return true;

        /* 忽略的事件：非阻塞模式直接返回，否则继续等下一个 */
        if (!infinite &&
            WaitForSingleObject(hin, wait_ms) != WAIT_OBJECT_0)
            return false;
    }
}

/* 阻塞读取（timeout_ms = 0 表示无限等待） */
bool input_read(InputEvent *ev, unsigned int timeout_ms) {
    return read_event(ev, timeout_ms, timeout_ms == 0);
}

/* 非阻塞读取：队列无事件立即返回 false */
bool input_poll(InputEvent *ev) {
    return read_event(ev, 0, false);
}
