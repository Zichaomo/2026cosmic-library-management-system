#ifndef COSMIC_LIBRARY_APP_H
#define COSMIC_LIBRARY_APP_H

#include <stdbool.h>

#include "core/auth.h"
#include "data/library_repository.h"
#include "data/reader_repository.h"
#include "ui/admin_screen.h"
#include "ui/console.h"
#include "ui/input.h"
#include "ui/login_screen.h"
#include "ui/register_screen.h"

typedef enum {
    APP_PAGE_WELCOME = 0,
    APP_PAGE_LOGIN,
    APP_PAGE_REGISTER,
    APP_PAGE_ADMIN_HOME,
    APP_PAGE_READER_HOME,
    APP_PAGE_BOOK_CATALOG,
    APP_PAGE_READER_LOANS,
    APP_PAGE_READER_FINES,
    APP_PAGE_ADMIN_BOOKS,
    APP_PAGE_ADMIN_ADD_BOOK,
    APP_PAGE_ADMIN_READERS,
    APP_PAGE_ADMIN_LOANS,
    APP_PAGE_ADMIN_BORROW,
    APP_PAGE_ADMIN_FINES,
    APP_PAGE_ADMIN_STATS
} AppPage;

typedef struct {
    Console console;
    Input input;
    LoginForm login_form;
    RegisterForm register_form;
    AdminBookForm admin_book_form;
    AdminBorrowForm admin_borrow_form;
    ReaderRepository readers;
    LibraryRepository library;
    UserSession session;
    AppPage page;
    int welcome_selection;
    int home_selection;
    int list_selection;
    wchar_t status_message[192];
    bool running;
} App;

bool app_init(App *app);
int app_run(App *app);
void app_shutdown(App *app);

#endif
