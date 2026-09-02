#include "ui/console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    if (console->has_input_mode) {
        DWORD input_mode = console->original_input_mode;
        input_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT |
                      ENABLE_WINDOW_INPUT;
        input_mode &= ~ENABLE_QUICK_EDIT_MODE;
        SetConsoleMode(console->input, input_mode);
    }

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

void console_shutdown(Console *console) {
    if (console == NULL) {
        return;
    }

    if (console->has_input_mode) {
        SetConsoleMode(console->input, console->original_input_mode);
    }
    if (console->has_output_mode) {
        SetConsoleMode(console->output, console->original_output_mode);
    }

    memset(console, 0, sizeof(*console));
}

