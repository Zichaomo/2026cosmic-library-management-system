#include "app.h"

#include <stdlib.h>

static void render_startup_screen(const App *app) {
    static const wchar_t screen[] =
        L"┌──────────────────────────────────────────────────────────┐\r\n"
        L"│                    宇宙图书馆管理系统                    │\r\n"
        L"├──────────────────────────────────────────────────────────┤\r\n"
        L"│  工程骨架初始化成功。                                    │\r\n"
        L"│  下一步：实现欢迎页面和输入事件循环。                    │\r\n"
        L"└──────────────────────────────────────────────────────────┘\r\n";

    console_clear(&app->console);
    console_write(&app->console, screen);
}

bool app_init(App *app) {
    if (app == NULL) {
        return false;
    }

    if (!console_init(&app->console)) {
        return false;
    }

    app->running = true;
    return true;
}

int app_run(App *app) {
    if (app == NULL) {
        return EXIT_FAILURE;
    }

    while (app->running) {
        render_startup_screen(app);

        /* The input-driven screen loop will replace this stop condition. */
        app->running = false;
    }

    return EXIT_SUCCESS;
}

void app_shutdown(App *app) {
    if (app == NULL) {
        return;
    }

    console_shutdown(&app->console);
    app->running = false;
}
