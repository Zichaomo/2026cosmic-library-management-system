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
    }
}
