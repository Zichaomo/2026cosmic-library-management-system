#include "ui/register_screen.h"

#include <string.h>
#include <wchar.h>

#include "ui/screen.h"
#include "ui/widget.h"

typedef struct {
    UiRect panel;
    UiRect fields[REGISTER_FIELD_COUNT];
    UiRect submit;
    UiRect back;
} RegisterLayout;

static bool get_register_layout(
    const Console *console,
    RegisterLayout *register_layout
) {
    enum { PANEL_HEIGHT = 22 };
    ScreenLayout screen_layout;

    if (register_layout == NULL ||
        !screen_get_layout(console, &screen_layout) ||
        screen_layout.content_height < PANEL_HEIGHT ||
        screen_layout.width < 62) {
        return false;
    }

    const short panel_width = screen_layout.width >= 70
        ? 66
        : (short)(screen_layout.width - 4);
    register_layout->panel = (UiRect){
        (short)((screen_layout.width - panel_width) / 2),
        (short)(screen_layout.content_y +
                (screen_layout.content_height - PANEL_HEIGHT) / 2),
        panel_width,
        PANEL_HEIGHT
    };

    for (int index = 0; index < REGISTER_FIELD_COUNT; ++index) {
        register_layout->fields[index] = (UiRect){
            (short)(register_layout->panel.x + 18),
            (short)(register_layout->panel.y + 2 + index * 3),
            (short)(register_layout->panel.width - 23),
            3
        };
    }
    register_layout->submit = (UiRect){
        (short)(register_layout->panel.x + 14),
        (short)(register_layout->panel.y + 17), 14, 3
    };
    register_layout->back = (UiRect){
        (short)(register_layout->panel.x + register_layout->panel.width - 28),
        (short)(register_layout->panel.y + 17), 14, 3
    };
    return true;
}

static wchar_t *get_field_buffer(
    RegisterForm *form,
    RegisterField field,
    size_t *capacity
) {
    switch (field) {
        case REGISTER_FIELD_NAME:
            *capacity = READER_NAME_CAPACITY;
            return form->name;
        case REGISTER_FIELD_READER_NO:
            *capacity = READER_NUMBER_CAPACITY;
            return form->reader_no;
        case REGISTER_FIELD_PASSWORD:
            *capacity = REGISTER_PASSWORD_CAPACITY;
            return form->password;
        case REGISTER_FIELD_PASSWORD_CONFIRMATION:
            *capacity = REGISTER_PASSWORD_CAPACITY;
            return form->password_confirmation;
        case REGISTER_FIELD_CONTACT:
            *capacity = READER_CONTACT_CAPACITY;
            return form->contact;
        case REGISTER_FIELD_COUNT:
            break;
    }
    *capacity = 0;
    return NULL;
}

static void append_character(RegisterForm *form, wchar_t character) {
    size_t capacity = 0;
    wchar_t *buffer = get_field_buffer(
        form, form->active_field, &capacity
    );
    size_t *length = &form->lengths[form->active_field];
    if (buffer == NULL || *length + 1 >= capacity) {
        register_form_set_message(form, L"输入内容已达到最大长度。", true);
        return;
    }

    buffer[(*length)++] = character;
    buffer[*length] = L'\0';
    form->message[0] = L'\0';
}

static void remove_character(RegisterForm *form) {
    size_t capacity = 0;
    wchar_t *buffer = get_field_buffer(
        form, form->active_field, &capacity
    );
    size_t *length = &form->lengths[form->active_field];
    (void)capacity;
    if (buffer != NULL && *length > 0) {
        buffer[--(*length)] = L'\0';
    }
    form->message[0] = L'\0';
}

void register_form_init(RegisterForm *form) {
    if (form == NULL) {
        return;
    }
    memset(form, 0, sizeof(*form));
    form->active_field = REGISTER_FIELD_NAME;
}

void register_form_set_message(
    RegisterForm *form,
    const wchar_t *message,
    bool is_error
) {
    if (form == NULL || message == NULL) {
        return;
    }
    wcsncpy(form->message, message, REGISTER_MESSAGE_CAPACITY - 1);
    form->message[REGISTER_MESSAGE_CAPACITY - 1] = L'\0';
    form->message_is_error = is_error;
}

void register_screen_draw(
    const Console *console,
    const RegisterForm *form
) {
    static const wchar_t *const labels[REGISTER_FIELD_COUNT] = {
        L"姓名：",
        L"学号/工号：",
        L"密码：",
        L"确认密码：",
        L"联系方式："
    };
    RegisterLayout layout;

    if (console == NULL || form == NULL) {
        return;
    }

    screen_draw_shell(
        console,
        L"宇宙图书馆管理系统",
        L" 读者注册 ",
        L"左键选择输入框/按钮  |  Enter 提交注册  |  右键返回"
    );
    if (!get_register_layout(console, &layout)) {
        return;
    }

    widget_draw_box(console, &layout.panel, CONSOLE_COLOR_CYAN);
    for (int index = 0; index < REGISTER_FIELD_COUNT; ++index) {
        console_write_at(
            console,
            (short)(layout.panel.x + 3),
            (short)(layout.fields[index].y + 1),
            labels[index],
            CONSOLE_COLOR_WHITE,
            CONSOLE_COLOR_BLACK
        );

        const wchar_t *value = L"";
        switch ((RegisterField)index) {
            case REGISTER_FIELD_NAME:
                value = form->name;
                break;
            case REGISTER_FIELD_READER_NO:
                value = form->reader_no;
                break;
            case REGISTER_FIELD_PASSWORD:
                value = form->password;
                break;
            case REGISTER_FIELD_PASSWORD_CONFIRMATION:
                value = form->password_confirmation;
                break;
            case REGISTER_FIELD_CONTACT:
                value = form->contact;
                break;
            case REGISTER_FIELD_COUNT:
                break;
        }
        widget_draw_input(
            console,
            &layout.fields[index],
            value,
            index == REGISTER_FIELD_PASSWORD ||
                index == REGISTER_FIELD_PASSWORD_CONFIRMATION,
            form->active_field == (RegisterField)index
        );
    }

    widget_draw_button(console, &layout.submit, L"提交注册", false);
    widget_draw_button(console, &layout.back, L"返回", false);

    if (form->message[0] != L'\0') {
        console_write_at(
            console,
            (short)(layout.panel.x + 3),
            (short)(layout.panel.y + 20),
            form->message,
            form->message_is_error
                ? CONSOLE_COLOR_RED
                : CONSOLE_COLOR_GREEN,
            CONSOLE_COLOR_BLACK
        );
    }
}

RegisterAction register_screen_handle_event(
    RegisterForm *form,
    const UiEvent *event
) {
    if (form == NULL || event == NULL || event->type != UI_EVENT_KEY) {
        return REGISTER_ACTION_NONE;
    }

    switch (event->key) {
        case UI_KEY_TAB:
        case UI_KEY_DOWN:
            form->active_field = (RegisterField)(
                (form->active_field + 1) % REGISTER_FIELD_COUNT
            );
            form->message[0] = L'\0';
            break;
        case UI_KEY_SHIFT_TAB:
        case UI_KEY_UP:
            form->active_field = (RegisterField)(
                (form->active_field + REGISTER_FIELD_COUNT - 1) %
                REGISTER_FIELD_COUNT
            );
            form->message[0] = L'\0';
            break;
        case UI_KEY_ENTER:
            return REGISTER_ACTION_SUBMIT;
        case UI_KEY_ESCAPE:
            return REGISTER_ACTION_BACK;
        case UI_KEY_BACKSPACE:
            remove_character(form);
            break;
        case UI_KEY_CHARACTER:
            append_character(form, event->character);
            break;
        default:
            break;
    }
    return REGISTER_ACTION_NONE;
}

