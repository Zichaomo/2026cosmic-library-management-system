#ifndef LMS_UI_THEME_H
#define LMS_UI_THEME_H

#include "../types.h"

/* ============ 颜色与样式（FR-201，默认蓝底白字） ============ */

typedef struct {
    unsigned short bg;              /* 全局背景 */
    unsigned short fg;              /* 全局前景 */
    unsigned short title_bg, title_fg;   /* 标题栏 */
    unsigned short menu_bg, menu_fg;     /* 菜单栏 */
    unsigned short menu_hl_bg, menu_hl_fg; /* 菜单悬停/选中 */
    unsigned short content_bg, content_fg; /* 内容区 */
    unsigned short status_bg, status_fg;   /* 状态栏 */
    unsigned short warn_fg, error_fg, ok_fg; /* 提示色 */
} Theme;

/* 当前主题 */
extern Theme g_theme;

/* 恢复默认主题（蓝底白字） */
void theme_default(void);

/* 将主题应用到控制台（设置当前颜色等） */
void theme_apply(void);

#endif /* LMS_UI_THEME_H */
