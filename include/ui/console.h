#ifndef COSMIC_LIBRARY_UI_CONSOLE_H
#define COSMIC_LIBRARY_UI_CONSOLE_H

#include <stdbool.h>
#include <windows.h>

typedef struct {
    HANDLE input;
    HANDLE output;
    DWORD original_input_mode;
    DWORD original_output_mode;
    WORD original_attributes;
    CONSOLE_CURSOR_INFO original_cursor_info;
    bool has_input_mode;
    bool has_output_mode;
    bool has_cursor_info;
} Console;

typedef enum {
    CONSOLE_COLOR_BLACK = 0,
    CONSOLE_COLOR_DARK_BLUE = 1,
    CONSOLE_COLOR_DARK_GREEN = 2,
    CONSOLE_COLOR_DARK_CYAN = 3,
    CONSOLE_COLOR_DARK_RED = 4,
    CONSOLE_COLOR_DARK_MAGENTA = 5,
    CONSOLE_COLOR_DARK_YELLOW = 6,
    CONSOLE_COLOR_LIGHT_GRAY = 7,
    CONSOLE_COLOR_DARK_GRAY = 8,
    CONSOLE_COLOR_BLUE = 9,
    CONSOLE_COLOR_GREEN = 10,
    CONSOLE_COLOR_CYAN = 11,
    CONSOLE_COLOR_RED = 12,
    CONSOLE_COLOR_MAGENTA = 13,
    CONSOLE_COLOR_YELLOW = 14,
    CONSOLE_COLOR_WHITE = 15
} ConsoleColor;

bool console_init(Console *console);
bool console_is_interactive(const Console *console);
bool console_get_size(const Console *console, short *width, short *height);
void console_clear(const Console *console);
void console_write(const Console *console, const wchar_t *text);
void console_write_at(
    const Console *console,
    short x,
    short y,
    const wchar_t *text,
    ConsoleColor foreground,
    ConsoleColor background
);
void console_fill_rect(
    const Console *console,
    short x,
    short y,
    short width,
    short height,
    wchar_t character,
    ConsoleColor foreground,
    ConsoleColor background
);
void console_shutdown(Console *console);

#endif
