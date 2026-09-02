#ifndef COSMIC_LIBRARY_UI_SCREEN_H
#define COSMIC_LIBRARY_UI_SCREEN_H

#include <stdbool.h>

#include "ui/console.h"

typedef struct {
    short width;
    short height;
    short title_y;
    short content_y;
    short content_height;
    short status_y;
} ScreenLayout;

bool screen_get_layout(const Console *console, ScreenLayout *layout);
void screen_draw_title_bar(
    const Console *console,
    const ScreenLayout *layout,
    const wchar_t *title
);
void screen_draw_content_area(
    const Console *console,
    const ScreenLayout *layout,
    const wchar_t *page_name
);
void screen_draw_status_bar(
    const Console *console,
    const ScreenLayout *layout,
    const wchar_t *status
);
bool screen_draw_shell(
    const Console *console,
    const wchar_t *title,
    const wchar_t *page_name,
    const wchar_t *status
);

#endif

