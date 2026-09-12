#include "ui/widget.h"

#include <stddef.h>
#include <wchar.h>

bool widget_contains_point(const UiRect *rect, short x, short y) {
    return rect != NULL && x >= rect->x && y >= rect->y &&
        x < rect->x + rect->width && y < rect->y + rect->height;
}

short widget_text_display_width(const wchar_t *text) {
    int width = 0;
    if (text == NULL) {
        return 0;
    }
    for (; *text != L'\0'; ++text) {
        width += *text <= 0x7f ? 1 : 2;
    }
    return width > 32767 ? 32767 : (short)width;
}

void widget_draw_box(
    const Console *console,
    const UiRect *rect,
    ConsoleColor color
) {
    if (console == NULL || rect == NULL ||
        rect->width < 2 || rect->height < 2) {
        return;
    }

    const short right = (short)(rect->x + rect->width - 1);
    const short bottom = (short)(rect->y + rect->height - 1);
    console_fill_rect(
        console, (short)(rect->x + 1), rect->y,
        (short)(rect->width - 2), 1, L'─', color, CONSOLE_COLOR_BLACK
    );
    console_fill_rect(
        console, (short)(rect->x + 1), bottom,
        (short)(rect->width - 2), 1, L'─', color, CONSOLE_COLOR_BLACK
    );
    console_fill_rect(
        console, rect->x, (short)(rect->y + 1), 1,
        (short)(rect->height - 2), L'│', color, CONSOLE_COLOR_BLACK
    );
    console_fill_rect(
        console, right, (short)(rect->y + 1), 1,
        (short)(rect->height - 2), L'│', color, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, rect->x, rect->y, L"┌", color, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, right, rect->y, L"┐", color, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, rect->x, bottom, L"└", color, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, right, bottom, L"┘", color, CONSOLE_COLOR_BLACK
    );
}

void widget_draw_button(
    const Console *console,
    const UiRect *rect,
    const wchar_t *label,
    bool focused
) {
    if (console == NULL || rect == NULL || label == NULL) {
        return;
    }

    const ConsoleColor foreground = focused
        ? CONSOLE_COLOR_BLACK
        : CONSOLE_COLOR_WHITE;
    const ConsoleColor background = focused
        ? CONSOLE_COLOR_CYAN
        : CONSOLE_COLOR_DARK_BLUE;

    console_fill_rect(
        console, rect->x, rect->y, rect->width, rect->height, L' ',
        foreground, background
    );

    const short label_display_width = widget_text_display_width(label);
    short label_x = (short)(rect->x +
        (rect->width - label_display_width) / 2);
    if (label_x < rect->x) {
        label_x = rect->x;
    }
    const short label_y = (short)(rect->y + rect->height / 2);
    console_write_at(
        console, label_x, label_y, label, foreground, background
    );
}

void widget_draw_input(
    const Console *console,
    const UiRect *rect,
    const wchar_t *value,
    bool password,
    bool focused
) {
    enum { DISPLAY_CAPACITY = 128 };
    wchar_t display[DISPLAY_CAPACITY];

    if (console == NULL || rect == NULL || value == NULL ||
        rect->width < 3 || rect->height < 3) {
        return;
    }

    const ConsoleColor border = focused
        ? CONSOLE_COLOR_YELLOW
        : CONSOLE_COLOR_DARK_CYAN;
    console_fill_rect(
        console, rect->x, rect->y, rect->width, rect->height, L' ',
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK
    );
    widget_draw_box(console, rect, border);

    size_t length = wcslen(value);
    const size_t max_visible = (size_t)(rect->width - 3);
    if (length > max_visible) {
        value += length - max_visible;
        length = max_visible;
    }
    if (length >= DISPLAY_CAPACITY) {
        length = DISPLAY_CAPACITY - 1;
    }

    for (size_t index = 0; index < length; ++index) {
        display[index] = password ? L'*' : value[index];
    }
    if (focused && length < DISPLAY_CAPACITY - 1 &&
        length < max_visible) {
        display[length++] = L'_';
    }
    display[length] = L'\0';

    console_write_at(
        console, (short)(rect->x + 2), (short)(rect->y + 1), display,
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK
    );
}
