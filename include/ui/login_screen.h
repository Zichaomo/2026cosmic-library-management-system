#ifndef COSMIC_LIBRARY_UI_LOGIN_SCREEN_H
#define COSMIC_LIBRARY_UI_LOGIN_SCREEN_H

#include <stdbool.h>
#include <stddef.h>

#include "ui/console.h"
#include "ui/input.h"

enum {
    LOGIN_USERNAME_CAPACITY = 32,
    LOGIN_PASSWORD_CAPACITY = 32,
    LOGIN_MESSAGE_CAPACITY = 96
};

typedef enum {
    LOGIN_FIELD_USERNAME = 0,
    LOGIN_FIELD_PASSWORD
} LoginField;

typedef struct {
    wchar_t username[LOGIN_USERNAME_CAPACITY];
    wchar_t password[LOGIN_PASSWORD_CAPACITY];
    size_t username_length;
    size_t password_length;
    LoginField active_field;
    wchar_t message[LOGIN_MESSAGE_CAPACITY];
    bool message_is_error;
} LoginForm;

typedef enum {
    LOGIN_ACTION_NONE = 0,
    LOGIN_ACTION_SUBMIT,
    LOGIN_ACTION_BACK
} LoginAction;

void login_form_init(LoginForm *form);
void login_form_clear_password(LoginForm *form);
void login_form_set_message(
    LoginForm *form,
    const wchar_t *message,
    bool is_error
);
void login_screen_draw(const Console *console, const LoginForm *form);
LoginAction login_screen_handle_event(
    LoginForm *form,
    const Console *console,
    const UiEvent *event
);

#endif

