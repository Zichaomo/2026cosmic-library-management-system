#ifndef COSMIC_LIBRARY_UI_REGISTER_SCREEN_H
#define COSMIC_LIBRARY_UI_REGISTER_SCREEN_H

#include <stdbool.h>
#include <stddef.h>

#include "data/reader_repository.h"
#include "ui/console.h"
#include "ui/input.h"
#include "ui/widget.h"

enum {
    REGISTER_PASSWORD_CAPACITY = 32,
    REGISTER_MESSAGE_CAPACITY = 96
};

typedef enum {
    REGISTER_FIELD_NAME = 0,
    REGISTER_FIELD_READER_NO,
    REGISTER_FIELD_PASSWORD,
    REGISTER_FIELD_PASSWORD_CONFIRMATION,
    REGISTER_FIELD_CONTACT,
    REGISTER_FIELD_COUNT
} RegisterField;

typedef struct {
    wchar_t name[READER_NAME_CAPACITY];
    wchar_t reader_no[READER_NUMBER_CAPACITY];
    wchar_t password[REGISTER_PASSWORD_CAPACITY];
    wchar_t password_confirmation[REGISTER_PASSWORD_CAPACITY];
    wchar_t contact[READER_CONTACT_CAPACITY];
    size_t lengths[REGISTER_FIELD_COUNT];
    size_t carets[REGISTER_FIELD_COUNT];
    RegisterField active_field;
    wchar_t message[REGISTER_MESSAGE_CAPACITY];
    bool message_is_error;
} RegisterForm;

typedef enum {
    REGISTER_ACTION_NONE = 0,
    REGISTER_ACTION_SUBMIT,
    REGISTER_ACTION_BACK
} RegisterAction;

void register_form_init(RegisterForm *form);
void register_form_place_caret(RegisterForm *form, const UiRect *rect, short x);
void register_form_set_message(
    RegisterForm *form,
    const wchar_t *message,
    bool is_error
);
void register_screen_draw(
    const Console *console,
    const RegisterForm *form
);
RegisterAction register_screen_handle_event(
    RegisterForm *form,
    const UiEvent *event
);

#endif

