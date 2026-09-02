#include "app.h"
#include "ui/screen.h"

#include <stdlib.h>

static void render_startup_screen(const App *app) {
    console_clear(&app->console);
    screen_draw_shell(
        &app->console,
        L"宇宙图书馆管理系统",
        L" 欢迎页 ",
        L"状态：系统就绪  |  当前用户：未登录  |  F1 帮助  |  Ctrl+Q 退出"
    );
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
