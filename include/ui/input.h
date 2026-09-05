#ifndef COSMIC_LIBRARY_UI_INPUT_H
#define COSMIC_LIBRARY_UI_INPUT_H

#include <stdbool.h>
#include <windows.h>

#include "ui/console.h"

typedef enum {
    UI_EVENT_NONE = 0,
    UI_EVENT_KEY,
    UI_EVENT_RESIZE
} UiEventType;

typedef enum {
    UI_KEY_NONE = 0,
    UI_KEY_CHARACTER,
    UI_KEY_TAB,
    UI_KEY_SHIFT_TAB,
    UI_KEY_ENTER,
    UI_KEY_ESCAPE,
    UI_KEY_BACKSPACE,
    UI_KEY_UP,
    UI_KEY_DOWN,
    UI_KEY_LEFT,
    UI_KEY_RIGHT,
    UI_KEY_HOME,
    UI_KEY_END,
    UI_KEY_PAGE_UP,
    UI_KEY_PAGE_DOWN,
    UI_KEY_F1,
    UI_KEY_F5,
    UI_KEY_CTRL_Q
} UiKey;

typedef struct {
    UiEventType type;
    UiKey key;
    wchar_t character;
    short width;
    short height;
} UiEvent;

typedef struct {
    Console *console;
} Input;

bool input_init(Input *input, Console *console);
bool input_read(Input *input, UiEvent *event);

#endif
