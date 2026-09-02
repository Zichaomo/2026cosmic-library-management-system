/* 主题 - 界面层（默认蓝底白字，PRD §4.1） */
#include "theme.h"

#include "console.h"

Theme g_theme;

void theme_default(void) {
    g_theme.bg = 0x10; /* 蓝底 */
    g_theme.fg = 0x0F; /* 白字 */

    g_theme.title_bg = 0x10;
    g_theme.title_fg = 0x0F;
    g_theme.menu_bg = 0x10;
    g_theme.menu_fg = 0x0E;
    g_theme.menu_hl_bg = 0x1E;
    g_theme.menu_hl_fg = 0x0F;
    g_theme.content_bg = 0x10;
    g_theme.content_fg = 0x0F;
    g_theme.status_bg = 0x70;
    g_theme.status_fg = 0x00;
    g_theme.warn_fg = 0x0E;
    g_theme.error_fg = 0x0C;
    g_theme.ok_fg = 0x0A;
}

void theme_apply(void) {
    console_set_color((unsigned short)(g_theme.bg | g_theme.fg));
}
