#include "ui/console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static WORD make_attributes(
    ConsoleColor foreground,
    ConsoleColor background
) {
    return (WORD)((WORD)foreground | ((WORD)background << 4));
}

static bool get_viewport(
    const Console *console,
    SMALL_RECT *viewport
) {
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (console == NULL || viewport == NULL || !console->has_output_mode ||
        !GetConsoleScreenBufferInfo(console->output, &info)) {
        return false;
    }

    *viewport = info.srWindow;
    return true;
}

static void write_utf8_fallback(const wchar_t *text) {
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL
    );

    if (size <= 1) {
        return;
    }

    char *utf8 = malloc((size_t)size);
    if (utf8 == NULL) {
        return;
    }

    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, size, NULL, NULL);
    fwrite(utf8, 1, (size_t)(size - 1), stdout);
    free(utf8);
}

bool console_init(Console *console) {
    if (console == NULL) {
        return false;
    }

    memset(console, 0, sizeof(*console));
    console->input = GetStdHandle(STD_INPUT_HANDLE);
    console->output = GetStdHandle(STD_OUTPUT_HANDLE);

    if (console->input == INVALID_HANDLE_VALUE ||
        console->output == INVALID_HANDLE_VALUE) {
        return false;
    }

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    console->has_input_mode = GetConsoleMode(
        console->input, &console->original_input_mode
    ) != 0;
    console->has_output_mode = GetConsoleMode(
        console->output, &console->original_output_mode
    ) != 0;

    CONSOLE_SCREEN_BUFFER_INFO screen_info;
    if (console->has_output_mode && GetConsoleScreenBufferInfo(
            console->output, &screen_info
        )) {
        console->original_attributes = screen_info.wAttributes;
    } else {
        console->original_attributes = make_attributes(
            CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
        );
    }

    console->has_cursor_info = GetConsoleCursorInfo(
        console->output, &console->original_cursor_info
    ) != 0;

    if (console->has_input_mode) {
        DWORD input_mode = console->original_input_mode;
        input_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
        input_mode &= ~ENABLE_MOUSE_INPUT;
        input_mode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_LINE_INPUT |
                        ENABLE_ECHO_INPUT);
        SetConsoleMode(console->input, input_mode);
    }

    if (console->has_cursor_info) {
        CONSOLE_CURSOR_INFO cursor_info = console->original_cursor_info;
        cursor_info.bVisible = FALSE;
        SetConsoleCursorInfo(console->output, &cursor_info);
    }

    return true;
}

bool console_is_interactive(const Console *console) {
    return console != NULL && console->has_output_mode;
}

bool console_get_size(const Console *console, short *width, short *height) {
    SMALL_RECT viewport;

    if (width == NULL || height == NULL ||
        !get_viewport(console, &viewport)) {
        return false;
    }

    *width = (short)(viewport.Right - viewport.Left + 1);
    *height = (short)(viewport.Bottom - viewport.Top + 1);
    return true;
}

void console_clear(const Console *console) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD origin = {0, 0};
    DWORD written = 0;

    if (console == NULL || !console->has_output_mode ||
        !GetConsoleScreenBufferInfo(console->output, &info)) {
        return;
    }

    const DWORD cells = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
    FillConsoleOutputCharacterW(console->output, L' ', cells, origin, &written);
    FillConsoleOutputAttribute(
        console->output, info.wAttributes, cells, origin, &written
    );
    SetConsoleCursorPosition(console->output, origin);
}

void console_write(const Console *console, const wchar_t *text) {
    if (console == NULL || text == NULL) {
        return;
    }

    if (console->has_output_mode) {
        DWORD written = 0;
        WriteConsoleW(
            console->output,
            text,
            (DWORD)wcslen(text),
            &written,
            NULL
        );
        return;
    }

    write_utf8_fallback(text);
}

void console_write_at(
    const Console *console,
    short x,
    short y,
    const wchar_t *text,
    ConsoleColor foreground,
    ConsoleColor background
) {
    SMALL_RECT viewport;

    if (text == NULL || !get_viewport(console, &viewport)) {
        return;
    }

    const short width = (short)(viewport.Right - viewport.Left + 1);
    const short height = (short)(viewport.Bottom - viewport.Top + 1);
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }

    const COORD position = {
        (short)(viewport.Left + x),
        (short)(viewport.Top + y)
    };
    DWORD written = 0;
    size_t length = wcslen(text);
    const size_t remaining = (size_t)(width - x);
    if (length > remaining) {
        length = remaining;
    }

    SetConsoleTextAttribute(
        console->output, make_attributes(foreground, background)
    );
    SetConsoleCursorPosition(console->output, position);
    WriteConsoleW(console->output, text, (DWORD)length, &written, NULL);
}

void console_fill_rect(
    const Console *console,
    short x,
    short y,
    short width,
    short height,
    wchar_t character,
    ConsoleColor foreground,
    ConsoleColor background
) {
    SMALL_RECT viewport;

    if (width <= 0 || height <= 0 || !get_viewport(console, &viewport)) {
        return;
    }

    const short viewport_width =
        (short)(viewport.Right - viewport.Left + 1);
    const short viewport_height =
        (short)(viewport.Bottom - viewport.Top + 1);

    if (x < 0 || y < 0 || x >= viewport_width || y >= viewport_height) {
        return;
    }

    if (x + width > viewport_width) {
        width = (short)(viewport_width - x);
    }
    if (y + height > viewport_height) {
        height = (short)(viewport_height - y);
    }

    const WORD attributes = make_attributes(foreground, background);
    for (short row = 0; row < height; ++row) {
        const COORD position = {
            (short)(viewport.Left + x),
            (short)(viewport.Top + y + row)
        };
        DWORD written = 0;
        FillConsoleOutputCharacterW(
            console->output, character, (DWORD)width, position, &written
        );
        FillConsoleOutputAttribute(
            console->output, attributes, (DWORD)width, position, &written
        );
    }
}

void console_shutdown(Console *console) {
    if (console == NULL) {
        return;
    }

    if (console->has_input_mode) {
        SetConsoleMode(console->input, console->original_input_mode);
    }
    if (console->has_output_mode) {
        SetConsoleTextAttribute(
            console->output, console->original_attributes
        );
        SetConsoleMode(console->output, console->original_output_mode);
    }
    if (console->has_cursor_info) {
        SetConsoleCursorInfo(console->output, &console->original_cursor_info);
    }

    memset(console, 0, sizeof(*console));
}
