#include "ui/home_screen.h"

#include <stdio.h>
#include <wchar.h>

#include "ui/screen.h"
#include "ui/widget.h"

static void draw_menu(
    const Console *console,
    const wchar_t *const labels[],
    int item_count,
    int selection,
    const wchar_t *message
) {
    enum { MENU_WIDTH = 36 };
    ScreenLayout layout;

    if (!screen_get_layout(console, &layout)) {
        return;
    }

    const short x = (short)((layout.width - MENU_WIDTH) / 2);
    const short start_y = (short)(layout.content_y + 3);
    for (int index = 0; index < item_count; ++index) {
        const UiRect item = {
            x,
            (short)(start_y + index * 2),
            MENU_WIDTH,
            1
        };
        widget_draw_button(
            console, &item, labels[index], selection == index
        );
    }

    if (message != NULL && message[0] != L'\0') {
        console_write_at(
            console,
            4,
            (short)(layout.content_y + layout.content_height - 3),
            message,
            CONSOLE_COLOR_YELLOW,
            CONSOLE_COLOR_BLACK
        );
    }
}

void home_screen_draw_admin(
    const Console *console,
    const UserSession *session,
    int selection,
    const wchar_t *message
) {
    static const wchar_t *const labels[ADMIN_MENU_COUNT] = {
        L"图书管理",
        L"读者管理",
        L"借阅管理",
        L"预约管理",
        L"罚款管理",
        L"统计报表",
        L"退出登录"
    };
    wchar_t status[128];

    swprintf(
        status,
        128,
        L"当前用户：%ls  |  管理员  |  ↑↓/Tab 选择  Enter 确认  Esc 退出登录",
        session->username
    );
    screen_draw_shell(
        console,
        L"宇宙图书馆管理系统",
        L" 管理员工作台 ",
        status
    );
    draw_menu(console, labels, ADMIN_MENU_COUNT, selection, message);
}

void home_screen_draw_reader(
    const Console *console,
    const UserSession *session,
    int selection,
    const wchar_t *message
) {
    static const wchar_t *const labels[READER_MENU_COUNT] = {
        L"图书检索",
        L"我的借阅",
        L"我的预约",
        L"我的罚款",
        L"退出登录"
    };
    wchar_t status[128];

    swprintf(
        status,
        128,
        L"当前用户：%ls  |  读者  |  ↑↓/Tab 选择  Enter 确认  Esc 退出登录",
        session->username
    );
    screen_draw_shell(
        console,
        L"宇宙图书馆管理系统",
        L" 读者个人中心 ",
        status
    );
    draw_menu(console, labels, READER_MENU_COUNT, selection, message);
}

