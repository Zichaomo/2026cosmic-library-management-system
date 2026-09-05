#ifndef COSMIC_LIBRARY_APP_H
#define COSMIC_LIBRARY_APP_H

#include <stdbool.h>

#include "core/auth.h"
#include "ui/console.h"
#include "ui/input.h"
#include "ui/login_screen.h"

typedef enum {
    APP_PAGE_WELCOME = 0,
    APP_PAGE_LOGIN,
    APP_PAGE_ADMIN_HOME,
    APP_PAGE_READER_HOME
} AppPage;

typedef struct {
    Console console;
    Input input;
    LoginForm login_form;
    UserSession session;
    AppPage page;
    int welcome_selection;
    int home_selection;
    wchar_t status_message[96];
    bool running;
} App;

bool app_init(App *app);
int app_run(App *app);
void app_shutdown(App *app);

#endif
