#include "ui/input.h"

#include <string.h>

static bool control_is_pressed(DWORD state) {
    return (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
}

static UiKey map_virtual_key(const KEY_EVENT_RECORD *key_event) {
    if (control_is_pressed(key_event->dwControlKeyState) &&
        key_event->wVirtualKeyCode == 'Q') {
        return UI_KEY_CTRL_Q;
    }

    switch (key_event->wVirtualKeyCode) {
        case VK_TAB:
            return (key_event->dwControlKeyState & SHIFT_PRESSED) != 0
                ? UI_KEY_SHIFT_TAB
                : UI_KEY_TAB;
        case VK_RETURN:
            return UI_KEY_ENTER;
        case VK_ESCAPE:
            return UI_KEY_ESCAPE;
        case VK_BACK:
            return UI_KEY_BACKSPACE;
        case VK_DELETE:
            return UI_KEY_DELETE;
        case VK_UP:
            return UI_KEY_UP;
        case VK_DOWN:
            return UI_KEY_DOWN;
        case VK_LEFT:
            return UI_KEY_LEFT;
        case VK_RIGHT:
            return UI_KEY_RIGHT;
        case VK_HOME:
            return UI_KEY_HOME;
        case VK_END:
            return UI_KEY_END;
        case VK_PRIOR:
            return UI_KEY_PAGE_UP;
        case VK_NEXT:
            return UI_KEY_PAGE_DOWN;
        case VK_F1:
            return UI_KEY_F1;
        case VK_F5:
            return UI_KEY_F5;
        default:
            break;
    }

    if (key_event->uChar.UnicodeChar >= L' ') {
        return UI_KEY_CHARACTER;
    }
    return UI_KEY_NONE;
}

static bool map_mouse_event(
    const Console *console,
    const MOUSE_EVENT_RECORD *mouse,
    UiEvent *event
) {
    if (console == NULL || mouse == NULL || event == NULL) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(console->output, &info)) {
        return false;
    }

    event->mouse_x = (short)(mouse->dwMousePosition.X - info.srWindow.Left);
    event->mouse_y = (short)(mouse->dwMousePosition.Y - info.srWindow.Top);
    event->type = UI_EVENT_MOUSE;

    if ((mouse->dwEventFlags & MOUSE_WHEELED) != 0) {
        const short delta = (short)HIWORD(mouse->dwButtonState);
        event->mouse_wheel = delta > 0 ? 1 : -1;
        return true;
    }

    if ((mouse->dwEventFlags & (MOUSE_MOVED | MOUSE_HWHEELED)) != 0) {
        return false;
    }

    if ((mouse->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0) {
        event->mouse_button = UI_MOUSE_LEFT;
        event->double_click =
            (mouse->dwEventFlags & DOUBLE_CLICK) != 0;
        return true;
    }
    if ((mouse->dwButtonState & RIGHTMOST_BUTTON_PRESSED) != 0) {
        event->mouse_button = UI_MOUSE_RIGHT;
        return true;
    }
    return false;
}

bool input_init(Input *input, Console *console) {
    if (input == NULL || console == NULL || !console->has_input_mode) {
        return false;
    }

    input->console = console;
    return true;
}

bool input_read(Input *input, UiEvent *event) {
    if (input == NULL || input->console == NULL || event == NULL) {
        return false;
    }

    for (;;) {
        INPUT_RECORD record;
        DWORD read_count = 0;
        memset(event, 0, sizeof(*event));

        const DWORD wait_result = WaitForSingleObject(
            input->console->input, 1000
        );
        if (wait_result == WAIT_TIMEOUT) {
            /*
             * This is a background synchronization tick, not an instruction
             * to repaint.  app.c redraws only if synchronized data changed.
             */
            event->type = UI_EVENT_REFRESH;
            return true;
        }
        if (wait_result != WAIT_OBJECT_0) {
            return false;
        }

        if (!ReadConsoleInputW(
                input->console->input, &record, 1, &read_count
            ) || read_count != 1) {
            return false;
        }

        if (record.EventType == KEY_EVENT &&
            record.Event.KeyEvent.bKeyDown) {
            event->type = UI_EVENT_KEY;
            event->key = map_virtual_key(&record.Event.KeyEvent);
            event->character = record.Event.KeyEvent.uChar.UnicodeChar;
            if (event->key != UI_KEY_NONE) {
                return true;
            }
            continue;
        }

        if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            event->type = UI_EVENT_RESIZE;
            console_get_size(
                input->console, &event->width, &event->height
            );
            return true;
        }

        if (record.EventType == MOUSE_EVENT &&
            map_mouse_event(
                input->console, &record.Event.MouseEvent, event
            )) {
            return true;
        }
    }
}
