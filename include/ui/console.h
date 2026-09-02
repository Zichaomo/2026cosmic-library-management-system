#ifndef COSMIC_LIBRARY_UI_CONSOLE_H
#define COSMIC_LIBRARY_UI_CONSOLE_H

#include <stdbool.h>
#include <windows.h>

typedef struct {
    HANDLE input;
    HANDLE output;
    DWORD original_input_mode;
    DWORD original_output_mode;
    bool has_input_mode;
    bool has_output_mode;
} Console;

bool console_init(Console *console);
void console_clear(const Console *console);
void console_write(const Console *console, const wchar_t *text);
void console_shutdown(Console *console);

#endif

