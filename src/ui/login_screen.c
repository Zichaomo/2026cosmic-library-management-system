#include "ui/login_screen.h"

#include <string.h>
#include <wchar.h>

#include "ui/screen.h"
#include "ui/widget.h"

typedef struct {
    UiRect panel;
    UiRect username;
    UiRect password;
    UiRect submit;
    UiRect back;
} LoginLayout;

static bool get_login_layout(
    const Console *console,
    LoginLayout *login_layout
) {
    enum { PANEL_WIDTH = 58, PANEL_HEIGHT = 18 };
    ScreenLayout screen_layout;

    if (login_layout == NULL ||
        !screen_get_layout(console, &screen_layout) ||
        screen_layout.content_height < PANEL_HEIGHT) {
        return false;
    }

    login_layout->panel = (UiRect){
        (short)((screen_layout.width - PANEL_WIDTH) / 2),
        (short)(screen_layout.content_y +
                (screen_layout.content_height - PANEL_HEIGHT) / 2),
        PANEL_WIDTH,
        PANEL_HEIGHT
    };
    login_layout->username = (UiRect){
        (short)(login_layout->panel.x + 15),
        (short)(login_layout->panel.y + 3),
        38,
        3
    };
    login_layout->password = (UiRect){
        (short)(login_layout->panel.x + 15),
        (short)(login_layout->panel.y + 7),
        38,
        3
    };
    login_layout->submit = (UiRect){
        (short)(login_layout->panel.x + 13),
        (short)(login_layout->panel.y + 11),
        12,
        3
    };
    login_layout->back = (UiRect){
        (short)(login_layout->panel.x + 33),
        (short)(login_layout->panel.y + 11),
        12,
        3
    };
    return true;
}

static void append_character(LoginForm *form, wchar_t character) {
    wchar_t *buffer = form->active_field == LOGIN_FIELD_USERNAME
        ? form->username
        : form->password;
    size_t *length = form->active_field == LOGIN_FIELD_USERNAME
        ? &form->username_length
        : &form->password_length;
    const size_t capacity = form->active_field == LOGIN_FIELD_USERNAME
        ? LOGIN_USERNAME_CAPACITY
        : LOGIN_PASSWORD_CAPACITY;

    if (*length + 1 >= capacity) {
        login_form_set_message(form, L"输入内容已达到最大长度。", true);
        return;
    }

    buffer[(*length)++] = character;
    buffer[*length] = L'\0';
    form->message[0] = L'\0';
}

static void remove_character(LoginForm *form) {
    wchar_t *buffer = form->active_field == LOGIN_FIELD_USERNAME
        ? form->username
        : form->password;
    size_t *length = form->active_field == LOGIN_FIELD_USERNAME
        ? &form->username_length
        : &form->password_length;

    if (*length > 0) {
        buffer[--(*length)] = L'\0';
    }
    form->message[0] = L'\0';
}

void login_form_init(LoginForm *form) {
    if (form == NULL) {
        return;
    }
    memset(form, 0, sizeof(*form));
    form->active_field = LOGIN_FIELD_USERNAME;
}

void login_form_clear_password(LoginForm *form) {
    if (form == NULL) {
        return;
    }
    memset(form->password, 0, sizeof(form->password));
    form->password_length = 0;
    form->active_field = LOGIN_FIELD_PASSWORD;
}

void login_form_set_message(
    LoginForm *form,
    const wchar_t *message,
    bool is_error
) {
    if (form == NULL || message == NULL) {
        return;
    }
    wcsncpy(form->message, message, LOGIN_MESSAGE_CAPACITY - 1);
    form->message[LOGIN_MESSAGE_CAPACITY - 1] = L'\0';
    form->message_is_error = is_error;
}

void login_screen_draw(const Console *console, const LoginForm *form) {
    LoginLayout layout;

    if (console == NULL || form == NULL) {
        return;
    }

    screen_draw_shell(
        console,
        L"宇宙图书馆管理系统",
        L" 用户登录 ",
        L"Tab 切换输入框  |  Enter 登录  |  Esc 返回  |  Ctrl+Q 退出"
    );

    if (!get_login_layout(console, &layout)) {
        return;
    }

    widget_draw_box(console, &layout.panel, CONSOLE_COLOR_CYAN);
    console_write_at(
        console, (short)(layout.panel.x + 23),
        (short)(layout.panel.y + 1), L"账号登录",
        CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, (short)(layout.panel.x + 4),
        (short)(layout.panel.y + 4), L"账号：",
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK
    );
    console_write_at(
        console, (short)(layout.panel.x + 4),
        (short)(layout.panel.y + 8), L"密码：",
        CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK
    );

    widget_draw_input(
        console, &layout.username, form->username, false,
        form->active_field == LOGIN_FIELD_USERNAME
    );
    widget_draw_input(
        console, &layout.password, form->password, true,
        form->active_field == LOGIN_FIELD_PASSWORD
    );
    widget_draw_button(console, &layout.submit, L"登录", false);
    widget_draw_button(console, &layout.back, L"返回", false);

    if (form->message[0] != L'\0') {
        console_write_at(
            console, (short)(layout.panel.x + 4),
            (short)(layout.panel.y + 15), form->message,
            form->message_is_error
                ? CONSOLE_COLOR_RED
                : CONSOLE_COLOR_GREEN,
            CONSOLE_COLOR_BLACK
        );
    }
}

LoginAction login_screen_handle_event(
    LoginForm *form,
    const UiEvent *event
) {
    if (form == NULL || event == NULL) {
        return LOGIN_ACTION_NONE;
    }

    if (event->type == UI_EVENT_KEY) {
        switch (event->key) {
            case UI_KEY_TAB:
            case UI_KEY_SHIFT_TAB:
            case UI_KEY_UP:
            case UI_KEY_DOWN:
                form->active_field = form->active_field == LOGIN_FIELD_USERNAME
                    ? LOGIN_FIELD_PASSWORD
                    : LOGIN_FIELD_USERNAME;
                form->message[0] = L'\0';
                break;
            case UI_KEY_ENTER:
                return LOGIN_ACTION_SUBMIT;
            case UI_KEY_ESCAPE:
                return LOGIN_ACTION_BACK;
            case UI_KEY_BACKSPACE:
                remove_character(form);
                break;
            case UI_KEY_CHARACTER:
                append_character(form, event->character);
                break;
            default:
                break;
        }
    }

    return LOGIN_ACTION_NONE;
}
