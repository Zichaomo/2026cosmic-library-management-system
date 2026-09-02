/* 控制台平台封装（Win32）- 界面层 */
#include "console.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static HANDLE g_out = INVALID_HANDLE_VALUE;
static HANDLE g_in = INVALID_HANDLE_VALUE;
static bool g_ready = false;

Result console_init(void) {
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    g_in = GetStdHandle(STD_INPUT_HANDLE);
    if (g_out == INVALID_HANDLE_VALUE || g_in == INVALID_HANDLE_VALUE)
        return RES_ERR;

    /* UTF-8 输出（FR-207） */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    /* 启用鼠标 / 扩展 / 窗口事件（PRD §6.3） */
    SetConsoleMode(g_in, ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT);

    SetConsoleTitleW(L"宇宙图书馆管理系统");
    g_ready = true;
    return RES_OK;
}

void console_shutdown(void) {
    if (g_ready) SetConsoleMode(g_in, ENABLE_PROCESSED_INPUT);
    g_ready = false;
}

bool console_get_size(int *cols, int *rows) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!g_ready) return false;
    if (!GetConsoleScreenBufferInfo(g_out, &info)) return false;
    if (cols) *cols = info.srWindow.Right - info.srWindow.Left + 1;
    if (rows) *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    return true;
}

void console_set_cursor(int col, int row) {
    COORD c;
    if (!g_ready || col < 0 || row < 0) return;
    c.X = (SHORT)col;
    c.Y = (SHORT)row;
    SetConsoleCursorPosition(g_out, c);
}

void console_set_color(unsigned short attr) {
    if (!g_ready) return;
    SetConsoleTextAttribute(g_out, attr);
}

void console_write_at(int col, int row, const char *text) {
    DWORD written = 0;
    if (!g_ready || !text) return;
    console_set_cursor(col, row);
    WriteConsoleA(g_out, text, (DWORD)strlen(text), &written, NULL);
}

void console_draw_box(int x, int y, int w, int h) {
    char line[512];
    int i;
    if (!g_ready || w < 2 || h < 2 || w > 510) return;

    /* 顶边 ┌─...─┐ */
    strcpy(line, "┌");
    for (i = 1; i < w - 1; i++) strcat(line, "─");
    strcat(line, "┐");
    console_write_at(x, y, line);

    /* 中间行 │ ... │ */
    for (i = 1; i < h - 1; i++) {
        int j;
        strcpy(line, "│");
        for (j = 1; j < w - 1; j++) strcat(line, " ");
        strcat(line, "│");
        console_write_at(x, y + i, line);
    }

    /* 底边 └─...─┘ */
    strcpy(line, "└");
    for (i = 1; i < w - 1; i++) strcat(line, "─");
    strcat(line, "┘");
    console_write_at(x, y + h - 1, line);
}

void console_clear(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD home = {0, 0};
    DWORD written = 0;
    int w, h;
    if (!g_ready) return;
    if (!GetConsoleScreenBufferInfo(g_out, &info)) return;
    w = info.srWindow.Right - info.srWindow.Left + 1;
    h = info.srWindow.Bottom - info.srWindow.Top + 1;
    FillConsoleOutputCharacterA(g_out, ' ', (DWORD)(w * h), home, &written);
    FillConsoleOutputAttribute(g_out, 0x1F, (DWORD)(w * h), home, &written);
    SetConsoleCursorPosition(g_out, home);
}

bool console_is_tty(void) {
    DWORD mode = 0;
    return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0;
}
