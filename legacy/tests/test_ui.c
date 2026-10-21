#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "ui/widget.h"
#include "ui/text_edit.h"
#include "app.h"

static int passed_count = 0;
static int failed_count = 0;

static void check(bool condition, const char *name) {
    if (condition) {
        ++passed_count;
        printf("[PASS] %s\n", name);
    } else {
        ++failed_count;
        printf("[FAIL] %s\n", name);
    }
}

static void key_login(LoginForm *form, UiKey key, wchar_t character) {
    const UiEvent event = {.type = UI_EVENT_KEY, .key = key, .character = character};
    login_screen_handle_event(form, &event);
}

static time_t local_time_at(int day, int hour, int minute, int second) {
    struct tm value = {0};
    value.tm_year = 126;
    value.tm_mon = 9;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = -1;
    return mktime(&value);
}

static void test_editing(void) {
    LoginForm login;
    login_form_init(&login);
    key_login(&login, UI_KEY_CHARACTER, L'a');
    key_login(&login, UI_KEY_CHARACTER, L'c');
    key_login(&login, UI_KEY_LEFT, 0);
    key_login(&login, UI_KEY_CHARACTER, L'b');
    check(wcscmp(login.username, L"abc") == 0 && login.carets[0] == 2,
        "login inserts characters in the middle");
    key_login(&login, UI_KEY_BACKSPACE, 0);
    check(wcscmp(login.username, L"ac") == 0 && login.carets[0] == 1,
        "backspace removes character before caret");
    key_login(&login, UI_KEY_DELETE, 0);
    check(wcscmp(login.username, L"a") == 0 && login.carets[0] == 1,
        "delete removes character after caret");
    key_login(&login, UI_KEY_HOME, 0);
    key_login(&login, UI_KEY_CHARACTER, L'X');
    key_login(&login, UI_KEY_END, 0);
    key_login(&login, UI_KEY_RIGHT, 0);
    check(wcscmp(login.username, L"Xa") == 0 && login.carets[0] == 2,
        "home end and boundary movement keep caret in range");
    key_login(&login, UI_KEY_TAB, 0);
    key_login(&login, UI_KEY_CHARACTER, L'p');
    key_login(&login, UI_KEY_CHARACTER, L'w');
    key_login(&login, UI_KEY_LEFT, 0);
    key_login(&login, UI_KEY_CHARACTER, L'a');
    check(wcscmp(login.password, L"paw") == 0 && login.carets[1] == 2,
        "password supports insertion with separate caret");
    login_form_clear_password(&login);
    check(login.password_length == 0 && login.carets[1] == 0,
        "failed-login password reset also resets caret");

    RegisterForm registration;
    register_form_init(&registration);
    const UiEvent chinese = {.type = UI_EVENT_KEY, .key = UI_KEY_CHARACTER,
        .character = L'图'};
    const UiEvent left = {.type = UI_EVENT_KEY, .key = UI_KEY_LEFT};
    register_screen_handle_event(&registration, &chinese);
    register_screen_handle_event(&registration, &left);
    const UiEvent book = {.type = UI_EVENT_KEY, .key = UI_KEY_CHARACTER,
        .character = L'书'};
    register_screen_handle_event(&registration, &book);
    check(wcscmp(registration.name, L"书图") == 0 && registration.carets[0] == 1,
        "registration can edit Chinese text before existing character");

    AdminBookForm admin;
    admin_book_form_init(&admin);
    admin_book_form_handle_event(&admin, &chinese);
    admin_book_form_handle_event(&admin, &left);
    admin_book_form_handle_event(&admin, &book);
    check(wcscmp(admin.isbn, L"书图") == 0 && admin.carets[0] == 1,
        "admin book form shares positional editing");
    AdminBorrowForm borrow;
    admin_borrow_form_init(&borrow);
    admin_borrow_form_handle_event(&borrow, &chinese);
    admin_borrow_form_handle_event(&borrow, &left);
    admin_borrow_form_handle_event(&borrow, &book);
    check(wcscmp(borrow.reader_no, L"书图") == 0,
        "admin borrowing form shares positional editing");

    wchar_t full[4] = L"abc";
    size_t length = 3;
    size_t caret = 1;
    check(!text_edit_handle(full, 4, &length, &caret, &book) &&
        wcscmp(full, L"abc") == 0 && caret == 1,
        "full buffer rejects insertion without overwriting text");
    wchar_t supplementary[5] = {L'A', 0xd83d, 0xde00, L'B', 0};
    length = 4;
    caret = 3;
    const UiEvent backspace = {.type = UI_EVENT_KEY, .key = UI_KEY_BACKSPACE};
    text_edit_handle(supplementary, 5, &length, &caret, &backspace);
    check(wcscmp(supplementary, L"AB") == 0 && caret == 1 && length == 2,
        "UTF16 surrogate pair deleted as one character");

    const UiRect field = {10, 5, 12, 3};
    check(widget_input_caret_from_click(&field, L"abc", false, 3, 13) == 1,
        "click places caret inside existing ASCII input");
    check(widget_input_caret_from_click(&field, L"图书A", false, 3, 14) == 1,
        "click uses terminal column widths for Chinese input");
    check(widget_input_view_start(L"abcdefghijk", false, 11, 5) == 7 &&
        widget_input_view_start(L"图书管理", false, 4, 5) == 2,
        "long inputs scroll to keep caret visible");
}

static void test_redraw_revision(void) {
    App app = {0};
    Loan loan = {0};
    strcpy(loan.due_date, "2026-10-09");
    app.library.loans = &loan;
    app.library.loan_count = 1;
    const unsigned long long before = app_render_revision(&app,
        local_time_at(9, 23, 59, 59));
    const unsigned long long midnight = app_render_revision(&app,
        local_time_at(10, 0, 0, 0));
    check(before != midnight, "midnight overdue change triggers redraw without CSV write");
    check(midnight == app_render_revision(&app, local_time_at(10, 0, 0, 1)),
        "unchanged one-second tick does not trigger redraw");
    ++app.login_form.carets[0];
    check(midnight != app_render_revision(&app, local_time_at(10, 0, 0, 1)),
        "caret movement triggers redraw");
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    const UiRect rect = {10, 5, 20, 3};

    check(widget_contains_point(&rect, 10, 5), "mouse hits top-left edge");
    check(widget_contains_point(&rect, 29, 7), "mouse hits bottom-right edge");
    check(!widget_contains_point(&rect, 30, 7), "mouse rejects right outside");
    check(!widget_contains_point(&rect, 9, 5), "mouse rejects left outside");
    check(widget_text_display_width(L"图书A") == 5,
          "mixed Chinese and ASCII display width");
    check(widget_text_display_width(L"") == 0, "empty text display width");

    test_editing();
    test_redraw_revision();

    printf(
        "\nUI test result: %d passed, %d failed.\n",
        passed_count, failed_count
    );
    return failed_count == 0 ? 0 : 1;
}
