#ifndef COSMIC_LIBRARY_UI_WIDGET_H
#define COSMIC_LIBRARY_UI_WIDGET_H

#include <stdbool.h>

#include "ui/console.h"

typedef struct {
    short x;
    short y;
    short width;
    short height;
} UiRect;

bool widget_contains_point(const UiRect *rect, short x, short y);
short widget_text_display_width(const wchar_t *text);
size_t widget_input_view_start(const wchar_t *value, bool password,
    size_t caret, short columns);
size_t widget_input_caret_from_click(const UiRect *rect, const wchar_t *value,
    bool password, size_t caret, short x);
void widget_draw_input_at_caret(const Console *console, const UiRect *rect,
    const wchar_t *value, bool password, bool focused, size_t caret);

void widget_draw_box(
    const Console *console,
    const UiRect *rect,
    ConsoleColor color
);
void widget_draw_button(
    const Console *console,
    const UiRect *rect,
    const wchar_t *label,
    bool focused
);
void widget_draw_input(
    const Console *console,
    const UiRect *rect,
    const wchar_t *value,
    bool password,
    bool focused
);

#endif
