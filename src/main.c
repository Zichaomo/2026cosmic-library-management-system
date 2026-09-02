/* 宇宙图书馆管理系统 - 入口（M1：控制台框架 + 事件循环） */
#include <stdio.h>

#include "core/auth.h"
#include "data/repository.h"
#include "types.h"
#include "ui/console.h"
#include "ui/input.h"
#include "ui/screen.h"
#include "ui/theme.h"

int main(void) {
    Result r;

    if ((r = console_init()) != RES_OK) {
        fprintf(stderr, "控制台初始化失败 (code=%d)\n", r);
        return 1;
    }
    theme_default();
    theme_apply();

    if ((r = repo_load_all()) != RES_OK) {
        fprintf(stderr, "数据加载失败 (code=%d)\n", r);
        console_shutdown();
        return 1;
    }
    if ((r = auth_init()) != RES_OK) {
        fprintf(stderr, "系统初始化失败 (code=%d)\n", r);
        console_shutdown();
        return 1;
    }

    if (!console_is_tty()) {
        /* 管道/重定向运行：降级为非交互提示（便于自动化检查） */
        printf("非交互模式：请在真实控制台（终端）中运行以使用图形界面。\n");
        printf("数据已就绪。初始管理员账号: admin / admin123\n");
        console_shutdown();
        return 0;
    }

    screen_goto(SCREEN_WELCOME);
    screen_draw();

    while (!g_app_quit) {
        InputEvent ev;
        if (input_read(&ev, 200)) {
            if (screen_handle(&ev)) screen_draw();
        }
    }

    console_shutdown();
    return 0;
}
