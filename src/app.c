#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "ui/screen.h"
#include "ui/widget.h"

enum {
    WELCOME_LOGIN = 0,
    WELCOME_REGISTER,
    WELCOME_HELP,
    WELCOME_EXIT,
    WELCOME_ITEM_COUNT
};

static bool get_welcome_buttons(
    const Console *console,
    UiRect buttons[WELCOME_ITEM_COUNT]
) {
    enum { BUTTON_WIDTH = 24, BUTTON_HEIGHT = 3, BUTTON_GAP = 1 };
    ScreenLayout layout;

    if (!screen_get_layout(console, &layout) ||
        layout.content_height < 19) {
        return false;
    }

    const short x = (short)((layout.width - BUTTON_WIDTH) / 2);
    const short start_y = (short)(layout.content_y + 3);
    for (int index = 0; index < WELCOME_ITEM_COUNT; ++index) {
        buttons[index] = (UiRect){
            x,
            (short)(start_y + index * (BUTTON_HEIGHT + BUTTON_GAP)),
            BUTTON_WIDTH,
            BUTTON_HEIGHT
        };
    }
    return true;
}

static void set_status(App *app, const wchar_t *message) {
    if (app == NULL || message == NULL) {
        return;
    }
    wcsncpy(app->status_message, message, 95);
    app->status_message[95] = L'\0';
}

static void draw_welcome_page(const App *app) {
    static const wchar_t *labels[WELCOME_ITEM_COUNT] = {
        L"登录",
        L"注册读者",
        L"帮助",
        L"退出"
    };
    UiRect buttons[WELCOME_ITEM_COUNT];

    screen_draw_shell(
        &app->console,
        L"宇宙图书馆管理系统",
        L" 欢迎页 ",
        app->status_message[0] != L'\0'
            ? app->status_message
            : L"方向键或 Tab 选择  |  Enter 确认  |  鼠标单击  |  Ctrl+Q 退出"
    );

    if (!get_welcome_buttons(&app->console, buttons)) {
        return;
    }

    for (int index = 0; index < WELCOME_ITEM_COUNT; ++index) {
        widget_draw_button(
            &app->console,
            &buttons[index],
            labels[index],
            app->welcome_selection == index
        );
    }
}

static void draw_admin_home(const App *app) {
    ScreenLayout layout;
    wchar_t status[128];

    swprintf(
        status,
        128,
        L"登录成功  |  当前用户：%ls  |  角色：管理员  |  Esc 退出登录",
        app->session.username
    );
    screen_draw_shell(
        &app->console,
        L"宇宙图书馆管理系统",
        L" 管理员主页 ",
        status
    );

    if (screen_get_layout(&app->console, &layout)) {
        console_write_at(
            &app->console,
            4,
            (short)(layout.content_y + 3),
            L"管理员登录验证成功，业务菜单将在下一阶段实现。",
            CONSOLE_COLOR_GREEN,
            CONSOLE_COLOR_BLACK
        );
    }
}

static void draw_current_page(const App *app) {
    console_clear(&app->console);
    switch (app->page) {
        case APP_PAGE_WELCOME:
            draw_welcome_page(app);
            break;
        case APP_PAGE_LOGIN:
            login_screen_draw(&app->console, &app->login_form);
            break;
        case APP_PAGE_ADMIN_HOME:
            draw_admin_home(app);
            break;
    }
}

static void activate_welcome_item(App *app) {
    switch (app->welcome_selection) {
        case WELCOME_LOGIN:
            login_form_init(&app->login_form);
            app->page = APP_PAGE_LOGIN;
            break;
        case WELCOME_REGISTER:
            set_status(app, L"读者注册功能将在后续阶段实现。");
            break;
        case WELCOME_HELP:
            set_status(
                app,
                L"帮助：使用方向键、Tab、Enter 或鼠标操作；Ctrl+Q 退出。"
            );
            break;
        case WELCOME_EXIT:
            app->running = false;
            break;
    }
}

static void handle_welcome_event(App *app, const UiEvent *event) {
    UiRect buttons[WELCOME_ITEM_COUNT];

    if (event->type == UI_EVENT_KEY) {
        switch (event->key) {
            case UI_KEY_TAB:
            case UI_KEY_DOWN:
                app->welcome_selection =
                    (app->welcome_selection + 1) % WELCOME_ITEM_COUNT;
                app->status_message[0] = L'\0';
                break;
            case UI_KEY_UP:
                app->welcome_selection =
                    (app->welcome_selection + WELCOME_ITEM_COUNT - 1) %
                    WELCOME_ITEM_COUNT;
                app->status_message[0] = L'\0';
                break;
            case UI_KEY_ENTER:
                activate_welcome_item(app);
                break;
            case UI_KEY_F1:
                set_status(
                    app,
                    L"帮助：选择“登录”进入账号登录界面，选择“退出”关闭系统。"
                );
                break;
            default:
                break;
        }
        return;
    }

    if ((event->type == UI_EVENT_MOUSE_MOVE ||
         event->type == UI_EVENT_MOUSE_CLICK) &&
        get_welcome_buttons(&app->console, buttons)) {
        for (int index = 0; index < WELCOME_ITEM_COUNT; ++index) {
            if (ui_rect_contains(
                    &buttons[index], event->mouse_x, event->mouse_y
                )) {
                app->welcome_selection = index;
                if (event->type == UI_EVENT_MOUSE_CLICK &&
                    event->mouse_button == UI_MOUSE_BUTTON_LEFT) {
                    activate_welcome_item(app);
                }
                return;
            }
        }
    }
}

static void submit_login(App *app) {
    if (app->login_form.username_length == 0 ||
        app->login_form.password_length == 0) {
        login_form_set_message(
            &app->login_form, L"账号和密码不能为空。", true
        );
        return;
    }

    const AuthResult result = auth_login(
        app->login_form.username,
        app->login_form.password,
        &app->session
    );
    switch (result) {
        case AUTH_RESULT_SUCCESS:
            login_form_clear_password(&app->login_form);
            app->page = APP_PAGE_ADMIN_HOME;
            break;
        case AUTH_RESULT_ACCOUNT_NOT_FOUND:
            login_form_clear_password(&app->login_form);
            login_form_set_message(
                &app->login_form, L"登录失败：账号不存在。", true
            );
            break;
        case AUTH_RESULT_WRONG_PASSWORD:
            login_form_clear_password(&app->login_form);
            login_form_set_message(
                &app->login_form, L"登录失败：密码错误。", true
            );
            break;
        case AUTH_RESULT_ACCOUNT_FROZEN:
            login_form_clear_password(&app->login_form);
            login_form_set_message(
                &app->login_form, L"登录失败：账号已冻结。", true
            );
            break;
    }
}

static void handle_login_event(App *app, const UiEvent *event) {
    const LoginAction action = login_screen_handle_event(
        &app->login_form, &app->console, event
    );
    if (action == LOGIN_ACTION_SUBMIT) {
        submit_login(app);
    } else if (action == LOGIN_ACTION_BACK) {
        login_form_clear_password(&app->login_form);
        app->page = APP_PAGE_WELCOME;
        set_status(app, L"已返回欢迎页。");
    }
}

static void handle_admin_home_event(App *app, const UiEvent *event) {
    if (event->type == UI_EVENT_KEY && event->key == UI_KEY_ESCAPE) {
        auth_session_clear(&app->session);
        login_form_init(&app->login_form);
        app->page = APP_PAGE_WELCOME;
        set_status(app, L"已退出登录。");
    }
}

bool app_init(App *app) {
    if (app == NULL) {
        return false;
    }

    memset(app, 0, sizeof(*app));
    if (!console_init(&app->console)) {
        return false;
    }

    app->page = APP_PAGE_WELCOME;
    app->welcome_selection = WELCOME_LOGIN;
    app->running = true;
    login_form_init(&app->login_form);
    auth_session_clear(&app->session);

    if (app->console.has_input_mode &&
        !input_init(&app->input, &app->console)) {
        console_shutdown(&app->console);
        return false;
    }
    return true;
}

int app_run(App *app) {
    if (app == NULL) {
        return EXIT_FAILURE;
    }

    draw_current_page(app);
    if (!console_is_interactive(&app->console)) {
        return EXIT_SUCCESS;
    }

    while (app->running) {
        UiEvent event;
        if (!input_read(&app->input, &event)) {
            return EXIT_FAILURE;
        }

        if (event.type == UI_EVENT_KEY && event.key == UI_KEY_CTRL_Q) {
            app->running = false;
            continue;
        }

        switch (app->page) {
            case APP_PAGE_WELCOME:
                handle_welcome_event(app, &event);
                break;
            case APP_PAGE_LOGIN:
                handle_login_event(app, &event);
                break;
            case APP_PAGE_ADMIN_HOME:
                handle_admin_home_event(app, &event);
                break;
        }

        if (app->running) {
            draw_current_page(app);
        }
    }

    return EXIT_SUCCESS;
}

void app_shutdown(App *app) {
    if (app == NULL) {
        return;
    }

    auth_session_clear(&app->session);
    console_shutdown(&app->console);
    app->running = false;
}
