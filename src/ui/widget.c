#include "ui/widget.h"

#include <stddef.h>
#include <wchar.h>
#include "ui/text_edit.h"

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
    widget_draw_input_at_caret(console, rect, value, password, focused,
        value != NULL ? wcslen(value) : 0);
}

static int input_character_width(const wchar_t *value, size_t position,
    bool password) {
    return password ? 1 : (value[position] <= 0x7f ? 1 : 2);
}

size_t widget_input_view_start(const wchar_t *value, bool password,
    size_t caret, short columns) {
    if (value == NULL || columns <= 0) return 0;
    const size_t length = wcslen(value);
    if (caret > length) caret = length;
    int width = 0;
    size_t start = 0;
    for (size_t i = 0; i < caret; i = text_edit_next(value, i)) {
        width += input_character_width(value, i, password);
    }
    /* Reserve one terminal cell for the non-blinking vertical caret. */
    while (start < caret && width >= columns) {
        width -= input_character_width(value, start, password);
        start = text_edit_next(value, start);
    }
    return start;
}

size_t widget_input_caret_from_click(const UiRect *rect, const wchar_t *value,
    bool password, size_t caret, short x) {
    if (rect == NULL || value == NULL) return 0;
    size_t index = widget_input_view_start(value, password, caret,
        (short)(rect->width - 3));
    int cell = rect->x + 2;
    while (value[index] != L'\0') {
        if (index == caret) {
            if (x <= cell) return index;
            ++cell;
        }
        const int width = input_character_width(value, index, password);
        if (x < cell + width) return index;
        cell += width;
        index = text_edit_next(value, index);
        if (cell >= rect->x + rect->width - 1) break;
    }
    return index;
}

void widget_draw_input_at_caret(const Console *console, const UiRect *rect,
    const wchar_t *value, bool password, bool focused, size_t caret) {
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

    const size_t length = wcslen(value);
    if (caret > length) caret = length;
    size_t index = focused ? widget_input_view_start(value, password, caret,
        (short)(rect->width - 3)) : 0;
    size_t used = 0;
    int cells = 0;
    const int max_visible = rect->width - 3;
    while (used < DISPLAY_CAPACITY - 1 && cells < max_visible) {
        if (focused && index == caret) {
            display[used++] = L'│';
            ++cells;
        }
        if (value[index] == L'\0') break;
        const size_t next = text_edit_next(value, index);
        const int width = input_character_width(value, index, password);
        if (cells + width > max_visible ||
            used + (password ? 1 : next - index) >= DISPLAY_CAPACITY) break;
        if (password) display[used++] = L'*';
        else {
            for (size_t i = index; i < next; ++i) display[used++] = value[i];
        }
        cells += width;
        index = next;
    }
    display[used] = L'\0';

    console_write_at(
        console, (short)(rect->x + 2), (short)(rect->y + 1), display,
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK
    );
}
