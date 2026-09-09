/* 控制台平台封装（Win32）- 界面层
   渲染策略：所有绘制先写入后台 CHAR_INFO 缓冲，最后用一次
   WriteConsoleOutputW 整屏刷新，避免"逐字符直写 + 全屏擦除"造成的闪烁。 */
#include "console.h"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

#include <stdlib.h>
#include <string.h>

static HANDLE g_out = INVALID_HANDLE_VALUE;
static HANDLE g_in = INVALID_HANDLE_VALUE;
static bool g_ready = false;

/* ---- 后台缓冲 ---- */
static CHAR_INFO *g_buf = NULL;
static int g_buf_w = 0, g_buf_h = 0;
static unsigned short g_attr = 0x1F; /* 当前前景/背景属性（蓝底白字） */

#define DEFAULT_ATTR 0x1F

/* ---------- 内部：缓冲操作 ---------- */

static bool buf_resize(int w, int h) {
    CHAR_INFO *nb;
    if (w <= 0 || h <= 0) return false;
    if (g_buf && g_buf_w == w && g_buf_h == h) return true;
    nb = realloc(g_buf, (size_t)w * (size_t)h * sizeof(CHAR_INFO));
    if (!nb) return false;
    g_buf = nb;
    g_buf_w = w;
    g_buf_h = h;
    return true;
}

static void buf_put(int x, int y, wchar_t ch) {
    size_t idx;
    if (!g_buf || x < 0 || y < 0 || x >= g_buf_w || y >= g_buf_h) return;
    idx = (size_t)y * (size_t)g_buf_w + (size_t)x;
    g_buf[idx].Char.UnicodeChar = (WCHAR)ch;
    g_buf[idx].Attributes = (WORD)g_attr;
}

static void buf_fill(wchar_t ch, unsigned short attr) {
    size_t i, n;
    if (!g_buf) return;
    n = (size_t)g_buf_w * (size_t)g_buf_h;
    for (i = 0; i < n; i++) {
        g_buf[i].Char.UnicodeChar = (WCHAR)ch;
        g_buf[i].Attributes = (WORD)attr;
    }
}

/* 宽字符（CJK 等占两列）判定：用于保证列对齐 */
static bool is_wide(wchar_t c) {
    unsigned int u = (unsigned int)c;
    return (u >= 0x1100 && u <= 0x115F) ||   /* 谚文字母 */
           (u >= 0x2E80 && u <= 0xA4CF) ||   /* CJK 部首/汉字/假名等 */
           (u >= 0xAC00 && u <= 0xD7A3) ||   /* 谚文音节 */
           (u >= 0xF900 && u <= 0xFAFF) ||   /* CJK 兼容汉字 */
           (u >= 0xFE30 && u <= 0xFE6F) ||   /* CJK 兼容形式 */
           (u >= 0xFF00 && u <= 0xFF60) ||   /* 全角形式 */
           (u >= 0xFFE0 && u <= 0xFFE6) ||   /* 全角符号 */
           (u >= 0x20000 && u <= 0x3FFFD);   /* 扩展区（代理对） */
}

/* UTF-8 → UTF-16 单字符；返回下一个字节位置 */
static const char *utf8_next(const char *s, wchar_t *out) {
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] < 0x80) {
        *out = (wchar_t)p[0];
        return s + 1;
    }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *out = (wchar_t)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F));
        return s + 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *out = (wchar_t)(((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) |
                         (p[2] & 0x3F));
        return s + 3;
    }
    if ((p[0] & 0xF8) == 0xF0) { /* 4 字节（BMP 之外）暂以占位符显示 */
        *out = L'?';
        return s + 4;
    }
    *out = L'?';
    return s + 1;
}

static void buf_write(int x, int y, const char *text) {
    wchar_t wc;
    if (!text) return;
    while (*text) {
        const char *next = utf8_next(text, &wc);
        if (is_wide(wc)) {
            buf_put(x, y, wc);
            buf_put(x + 1, y, L' '); /* 宽字符第二格 */
            x += 2;
        } else {
            buf_put(x, y, wc);
            x += 1;
        }
        text = next;
    }
}

/* ---------- 对外接口 ---------- */

Result console_init(void) {
    CONSOLE_CURSOR_INFO ci;
    int w = 0, h = 0;

    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    g_in = GetStdHandle(STD_INPUT_HANDLE);
    if (g_out == INVALID_HANDLE_VALUE || g_in == INVALID_HANDLE_VALUE)
        return RES_ERR;

    /* UTF-8 输出（FR-207） */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    /* 启用鼠标 / 扩展 / 窗口事件（PRD §6.3） */
    SetConsoleMode(g_in, ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS |
                             ENABLE_WINDOW_INPUT);

    SetConsoleTitleW(L"宇宙图书馆管理系统");

    /* 隐藏光标：后台缓冲渲染下不需要光标，且可减少视觉闪烁 */
    ci.dwSize = 1;
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(g_out, &ci);

    g_attr = DEFAULT_ATTR;
    g_ready = true;

    if (console_get_size(&w, &h)) buf_resize(w, h);
    return RES_OK;
}

void console_shutdown(void) {
    CONSOLE_CURSOR_INFO ci;
    if (!g_ready) return;
    SetConsoleMode(g_in, ENABLE_PROCESSED_INPUT);
    ci.dwSize = 25;
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(g_out, &ci);
    free(g_buf);
    g_buf = NULL;
    g_buf_w = g_buf_h = 0;
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

/* 后台缓冲模式下光标位置由缓冲决定，此接口保留为空实现 */
void console_set_cursor(int col, int row) {
    (void)col;
    (void)row;
}

void console_set_color(unsigned short attr) {
    g_attr = attr;
}

void console_write_at(int col, int row, const char *text) {
    if (!g_ready) return;
    buf_write(col, row, text);
}

void console_draw_box(int x, int y, int w, int h) {
    int i;
    if (!g_ready || w < 2 || h < 2) return;
    buf_put(x, y, L'┌');
    for (i = 1; i < w - 1; i++) buf_put(x + i, y, L'─');
    buf_put(x + w - 1, y, L'┐');
    for (i = 1; i < h - 1; i++) {
        buf_put(x, y + i, L'│');
        buf_put(x + w - 1, y + i, L'│');
    }
    buf_put(x, y + h - 1, L'└');
    for (i = 1; i < w - 1; i++) buf_put(x + i, y + h - 1, L'─');
    buf_put(x + w - 1, y + h - 1, L'┘');
}

/* 清屏：只清后台缓冲（不碰屏幕），随后由 console_present 一次性刷新 */
void console_clear(void) {
    int w = 0, h = 0;
    if (!g_ready) return;
    if (!console_get_size(&w, &h)) return;
    if (!buf_resize(w, h)) return;
    buf_fill(L' ', DEFAULT_ATTR);
}

/* 将后台缓冲整屏输出（消除逐字符刷新带来的闪烁） */
void console_present(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD size, src = {0, 0};
    SMALL_RECT region;
    if (!g_ready || !g_buf) return;
    if (!GetConsoleScreenBufferInfo(g_out, &info)) return;
    size.X = (SHORT)g_buf_w;
    size.Y = (SHORT)g_buf_h;
    /* 写入区域以当前可见窗口原点为基准（缓冲区可能被滚动） */
    region.Left = info.srWindow.Left;
    region.Top = info.srWindow.Top;
    region.Right = (SHORT)(info.srWindow.Left + g_buf_w - 1);
    region.Bottom = (SHORT)(info.srWindow.Top + g_buf_h - 1);
    WriteConsoleOutputW(g_out, g_buf, size, src, &region);
}

bool console_is_tty(void) {
    DWORD mode = 0;
    return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0;
}
