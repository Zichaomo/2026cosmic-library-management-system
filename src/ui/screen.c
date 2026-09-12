#include "ui/screen.h"

#include <stddef.h>

enum {
    MIN_SCREEN_WIDTH = 60,
    MIN_SCREEN_HEIGHT = 12,
    RECOMMENDED_SCREEN_WIDTH = 100,
    RECOMMENDED_SCREEN_HEIGHT = 30,
    TITLE_DISPLAY_WIDTH = 18
};

static void draw_content_border(
    const Console *console,
    const ScreenLayout *layout
) {
    const short top = layout->content_y;
    const short bottom =
        (short)(layout->content_y + layout->content_height - 1);
    const short right = (short)(layout->width - 1);

    console_fill_rect(
        console, 1, top, (short)(layout->width - 2), 1, L'─',
        CONSOLE_COLOR_DARK_CYAN, CONSOLE_COLOR_BLACK
    );
    console_fill_rect(
        console, 1, bottom, (short)(layout->width - 2), 1, L'─',
        CONSOLE_COLOR_DARK_CYAN, CONSOLE_COLOR_BLACK
    );
    console_fill_rect(
        console, 0, (short)(top + 1), 1,
        (short)(layout->content_height - 2), L'│',
        CONSOLE_COLOR_DARK_CYAN, CONSOLE_COLOR_BLACK
    );
    console_fill_rect(
        console, right, (short)(top + 1), 1,
        (short)(layout->content_height - 2), L'│',
        CONSOLE_COLOR_DARK_CYAN, CONSOLE_COLOR_BLACK
    );

    console_write_at(
        console, 0, top, L"┌", CONSOLE_COLOR_CYAN, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, right, top, L"┐", CONSOLE_COLOR_CYAN, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, 0, bottom, L"└", CONSOLE_COLOR_CYAN, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, right, bottom, L"┘", CONSOLE_COLOR_CYAN, CONSOLE_COLOR_BLACK
    );
}

bool screen_get_layout(const Console *console, ScreenLayout *layout) {
    if (layout == NULL ||
        !console_get_size(console, &layout->width, &layout->height)) {
        return false;
    }

    if (layout->width < MIN_SCREEN_WIDTH ||
        layout->height < MIN_SCREEN_HEIGHT) {
        return false;
    }

    layout->title_y = 0;
    layout->content_y = 1;
    layout->content_height = (short)(layout->height - 2);
    layout->status_y = (short)(layout->height - 1);
    return true;
}

void screen_draw_title_bar(
    const Console *console,
    const ScreenLayout *layout,
    const wchar_t *title
) {
    if (console == NULL || layout == NULL || title == NULL) {
        return;
    }

    console_fill_rect(
        console, 0, layout->title_y, layout->width, 1, L' ',
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_DARK_BLUE
    );

    if (layout->width >= 76) {
        console_write_at(
            console, 2, layout->title_y, L"BUCT / 0Error",
            CONSOLE_COLOR_CYAN, CONSOLE_COLOR_DARK_BLUE
        );
        console_write_at(
            console, (short)(layout->width - 15), layout->title_y,
            L"FINAL  v1.0.0",
            CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_DARK_BLUE
        );
    }

    short title_x = (short)((layout->width - TITLE_DISPLAY_WIDTH) / 2);
    if (title_x < 1) {
        title_x = 1;
    }
    console_write_at(
        console, title_x, layout->title_y, title,
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_DARK_BLUE
    );
}

void screen_draw_content_area(
    const Console *console,
    const ScreenLayout *layout,
    const wchar_t *page_name
) {
    if (console == NULL || layout == NULL || page_name == NULL) {
        return;
    }

    console_fill_rect(
        console, 0, layout->content_y, layout->width,
        layout->content_height, L' ',
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    draw_content_border(console, layout);
    console_write_at(
        console, 2, layout->content_y, page_name,
        CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_BLACK
    );

    if (layout->width < RECOMMENDED_SCREEN_WIDTH ||
        layout->height < RECOMMENDED_SCREEN_HEIGHT) {
        console_write_at(
            console, 4,
            (short)(layout->content_y + layout->content_height - 2),
            L"提示：建议将终端窗口调整到至少 100 × 30。",
            CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_BLACK
        );
    }
}

void screen_draw_status_bar(
    const Console *console,
    const ScreenLayout *layout,
    const wchar_t *status
) {
    if (console == NULL || layout == NULL || status == NULL) {
        return;
    }

    console_fill_rect(
        console, 0, layout->status_y, layout->width, 1, L' ',
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_DARK_BLUE
    );
    console_write_at(
        console, 1, layout->status_y, status,
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_DARK_BLUE
    );
}

bool screen_draw_shell(
    const Console *console,
    const wchar_t *title,
    const wchar_t *page_name,
    const wchar_t *status
) {
    ScreenLayout layout;

    if (!console_is_interactive(console)) {
        console_write(
            console,
            L"宇宙图书馆管理系统\r\n"
            L"界面框架已就绪。\r\n"
            L"状态：系统就绪；当前用户：未登录。\r\n"
        );
        return true;
    }

    if (!screen_get_layout(console, &layout)) {
        console_clear(console);
        console_write(
            console,
            L"终端窗口过小，请调整到至少 60 × 12；建议使用 100 × 30。\r\n"
        );
        return false;
    }

    screen_draw_title_bar(console, &layout, title);
    screen_draw_content_area(console, &layout, page_name);
    screen_draw_status_bar(console, &layout, status);
    return true;
}
