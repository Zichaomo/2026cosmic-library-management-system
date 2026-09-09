#ifndef COSMIC_LIBRARY_UI_ADMIN_SCREEN_H
#define COSMIC_LIBRARY_UI_ADMIN_SCREEN_H

#include <stdbool.h>
#include <stddef.h>

#include "data/library_repository.h"
#include "data/reader_repository.h"
#include "ui/console.h"
#include "ui/input.h"

enum {
    ADMIN_COPIES_CAPACITY = 5,
    ADMIN_BOOK_FORM_FIELD_COUNT = 4,
    ADMIN_BORROW_FORM_FIELD_COUNT = 2
};

typedef struct {
    wchar_t isbn[BOOK_ISBN_CAPACITY];
    wchar_t title[BOOK_TITLE_CAPACITY];
    wchar_t author[BOOK_AUTHOR_CAPACITY];
    wchar_t copies[ADMIN_COPIES_CAPACITY];
    size_t lengths[ADMIN_BOOK_FORM_FIELD_COUNT];
    int active_field;
    wchar_t message[128];
    bool message_is_error;
} AdminBookForm;

typedef struct {
    wchar_t reader_no[READER_NUMBER_CAPACITY];
    wchar_t book_id[12];
    size_t lengths[ADMIN_BORROW_FORM_FIELD_COUNT];
    int active_field;
    wchar_t message[128];
    bool message_is_error;
} AdminBorrowForm;

typedef enum {
    ADMIN_FORM_ACTION_NONE = 0,
    ADMIN_FORM_ACTION_SUBMIT,
    ADMIN_FORM_ACTION_BACK
} AdminFormAction;

void admin_book_form_init(AdminBookForm *form);
void admin_borrow_form_init(AdminBorrowForm *form);
void admin_book_form_set_message(
    AdminBookForm *form,
    const wchar_t *message,
    bool is_error
);
void admin_borrow_form_set_message(
    AdminBorrowForm *form,
    const wchar_t *message,
    bool is_error
);
AdminFormAction admin_book_form_handle_event(
    AdminBookForm *form,
    const UiEvent *event
);
AdminFormAction admin_borrow_form_handle_event(
    AdminBorrowForm *form,
    const UiEvent *event
);
void admin_screen_draw_book_form(
    const Console *console,
    const AdminBookForm *form
);
void admin_screen_draw_borrow_form(
    const Console *console,
    const AdminBorrowForm *form
);
void admin_screen_draw_books(
    const Console *console,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
);
void admin_screen_draw_readers(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
);
void admin_screen_draw_stats(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library
);

#endif
