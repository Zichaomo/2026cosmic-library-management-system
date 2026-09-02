/* 页面路由 - 界面层（骨架：路由状态可用，页面绘制待 M1） */
#include "screen.h"

#include "console.h"
#include "theme.h"

static ScreenId s_current = SCREEN_WELCOME;
static ScreenId s_history[32];
static int s_history_len = 0;

Result screen_goto(ScreenId id) {
    if (id < 0 || id >= SCREEN_COUNT) return RES_ERR_INVALID;
    if (s_current != id && s_history_len < 32) {
        s_history[s_history_len++] = s_current;
    }
    s_current = id;
    /* TODO(M1): 页面进入回调（布局初始化、菜单/按钮构建） */
    return RES_OK;
}

void screen_draw(void) {
    console_clear();
    /* TODO(M1): 按 s_current 绘制对应页面（标题栏/菜单栏/内容区/状态栏，FR-201） */
    console_set_color((unsigned short)(g_theme.bg | g_theme.fg));
    console_write_at(2, 2, "TODO(M1): 页面绘制未实现");
}

bool screen_handle(const InputEvent *ev) {
    (void)ev;
    /* TODO(M1): 命中当前页面控件并分发（FR-202），F1/F5/Esc/Ctrl+Q（FR-205） */
    return false;
}

void screen_back(void) {
    if (s_history_len > 0) s_current = s_history[--s_history_len];
}
