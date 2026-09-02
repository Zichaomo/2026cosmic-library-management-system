/* 宇宙图书馆管理系统 - 入口（骨架版） */
#include <stdio.h>

#include "data/repository.h"
#include "types.h"
#include "ui/console.h"
#include "ui/theme.h"

int main(void) {
    Result r;

    if ((r = console_init()) != RES_OK) {
        fprintf(stderr, "控制台初始化失败 (code=%d)\n", r);
        return 1;
    }
    theme_default();
    theme_apply();

    r = repo_load_all();
    printf("[-] 数据仓库加载 -> %s (code=%d)\n", r == RES_OK ? "OK" : "未完成", r);

    printf("\n");
    printf("  ┌──────────────────────────────────────┐\n");
    printf("  │   宇宙图书馆管理系统（骨架版 v0.1）   │\n");
    printf("  └──────────────────────────────────────┘\n");
    printf("  分支: wdy_branch    构建时间: %s %s\n", __DATE__, __TIME__);
    printf("  模块: ui/ core/ data/ utils/ 均已就位\n");
    printf("  TODO(M1): 欢迎页 / 登录注册 / 事件循环 / CSV 持久化\n");
    printf("\n  按 Enter 退出...\n");

    getchar();

    console_shutdown();
    return 0;
}
