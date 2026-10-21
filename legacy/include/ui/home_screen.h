#ifndef COSMIC_LIBRARY_UI_HOME_SCREEN_H
#define COSMIC_LIBRARY_UI_HOME_SCREEN_H

#include "core/auth.h"
#include "ui/console.h"

typedef enum {
    ADMIN_MENU_BOOKS = 0,
    ADMIN_MENU_READERS,
    ADMIN_MENU_LOANS,
    ADMIN_MENU_RESERVATIONS,
    ADMIN_MENU_FINES,
    ADMIN_MENU_STATS,
    ADMIN_MENU_LOGOUT,
    ADMIN_MENU_COUNT
} AdminMenuItem;

typedef enum {
    READER_MENU_SEARCH = 0,
    READER_MENU_MY_LOANS,
    READER_MENU_MY_RESERVATIONS,
    READER_MENU_MY_FINES,
    READER_MENU_LOGOUT,
    READER_MENU_COUNT
} ReaderMenuItem;

void home_screen_draw_admin(
    const Console *console,
    const UserSession *session,
    int selection,
    const wchar_t *message
);
void home_screen_draw_reader(
    const Console *console,
    const UserSession *session,
    int selection,
    const wchar_t *message
);

#endif

