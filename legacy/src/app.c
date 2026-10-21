#include "app.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "ui/admin_screen.h"
#include "ui/circulation_screen.h"
#include "ui/home_screen.h"
#include "ui/screen.h"
#include "ui/widget.h"

enum {
    WELCOME_LOGIN = 0,
    WELCOME_REGISTER,
    WELCOME_HELP,
    WELCOME_EXIT,
    WELCOME_ITEM_COUNT
};

enum { MOUSE_LIST_VISIBLE_ROWS = 15 };

static void route_mouse_event(
    App *app,
    const UiEvent *source,
    UiEvent *routed
);

static void hash_render_bytes(
    unsigned long long *hash,
    const void *data,
    size_t size
) {
    const unsigned char *bytes = data;
    for (size_t index = 0; index < size; ++index) {
        *hash ^= bytes[index];
        *hash *= 1099511628211ULL;
    }
}

static void hash_render_array(
    unsigned long long *hash,
    const void *items,
    size_t count,
    size_t item_size
) {
    hash_render_bytes(hash, &count, sizeof(count));
    if (items != NULL && count > 0) {
        hash_render_bytes(hash, items, count * item_size);
    }
}

/*
 * The input layer emits a one-second refresh event so separate terminals can
 * synchronize their CSV data.  That event must not force a repaint when the
 * user-visible state is unchanged, otherwise the console visibly flickers.
 */
unsigned long long app_render_revision(const App *app, time_t now) {
    unsigned long long hash = 1469598103934665603ULL;

    hash_render_bytes(&hash, &app->login_form, sizeof(app->login_form));
    hash_render_bytes(&hash, &app->register_form, sizeof(app->register_form));
    hash_render_bytes(
        &hash, &app->admin_book_form, sizeof(app->admin_book_form)
    );
    hash_render_bytes(
        &hash, &app->admin_borrow_form, sizeof(app->admin_borrow_form)
    );
    hash_render_bytes(&hash, &app->session, sizeof(app->session));
    hash_render_bytes(&hash, &app->page, sizeof(app->page));
    hash_render_bytes(
        &hash, &app->welcome_selection, sizeof(app->welcome_selection)
    );
    hash_render_bytes(
        &hash, &app->home_selection, sizeof(app->home_selection)
    );
    hash_render_bytes(
        &hash, &app->list_selection, sizeof(app->list_selection)
    );
    hash_render_bytes(
        &hash, app->status_message, sizeof(app->status_message)
    );

    hash_render_array(
        &hash, app->readers.items, app->readers.count,
        sizeof(*app->readers.items)
    );
    hash_render_array(
        &hash, app->library.books, app->library.book_count,
        sizeof(*app->library.books)
    );
    hash_render_array(
        &hash, app->library.loans, app->library.loan_count,
        sizeof(*app->library.loans)
    );
    hash_render_array(
        &hash, app->library.fines, app->library.fine_count,
        sizeof(*app->library.fines)
    );
    hash_render_array(
        &hash, app->library.reservations,
        app->library.reservation_count,
        sizeof(*app->library.reservations)
    );
    /* Loan labels and statistics change at midnight even without CSV writes. */
    for (size_t i = 0; i < app->library.loan_count; ++i) {
        const int overdue_days = library_loan_overdue_days(
            &app->library.loans[i], now);
        hash_render_bytes(&hash, &overdue_days, sizeof(overdue_days));
    }
    return hash;
}

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
    wcsncpy(app->status_message, message, 191);
    app->status_message[191] = L'\0';
}

static int current_reader_id(const App *app) {
    const ReaderAccount *reader = reader_repository_find_by_number(
        &app->readers, app->session.username
    );
    return reader != NULL ? reader->id : 0;
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
            : L"左键/方向键选择  |  Enter 确认  |  右键返回  |  Ctrl+Q 退出"
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

static void draw_current_page(const App *app) {
    console_clear(&app->console);
    switch (app->page) {
        case APP_PAGE_WELCOME:
            draw_welcome_page(app);
            break;
        case APP_PAGE_LOGIN:
            login_screen_draw(&app->console, &app->login_form);
            break;
        case APP_PAGE_REGISTER:
            register_screen_draw(&app->console, &app->register_form);
            break;
        case APP_PAGE_ADMIN_HOME:
            home_screen_draw_admin(
                &app->console,
                &app->session,
                app->home_selection,
                app->status_message
            );
            break;
        case APP_PAGE_READER_HOME:
            home_screen_draw_reader(
                &app->console,
                &app->session,
                app->home_selection,
                app->status_message
            );
            break;
        case APP_PAGE_BOOK_CATALOG:
            circulation_screen_draw_books(
                &app->console, &app->library,
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_READER_LOANS:
            circulation_screen_draw_reader_loans(
                &app->console, &app->library, current_reader_id(app),
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_READER_RESERVATIONS:
            circulation_screen_draw_reader_reservations(
                &app->console, &app->library, current_reader_id(app),
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_READER_FINES:
            circulation_screen_draw_reader_fines(
                &app->console, &app->library, current_reader_id(app),
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_ADMIN_BOOKS:
            admin_screen_draw_books(
                &app->console, &app->library,
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_ADMIN_ADD_BOOK:
            admin_screen_draw_book_form(
                &app->console, &app->admin_book_form
            );
            break;
        case APP_PAGE_ADMIN_READERS:
            admin_screen_draw_readers(
                &app->console, &app->readers, &app->library,
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_ADMIN_LOANS:
            circulation_screen_draw_admin_loans(
                &app->console, &app->readers, &app->library,
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_ADMIN_BORROW:
            admin_screen_draw_borrow_form(
                &app->console, &app->admin_borrow_form
            );
            break;
        case APP_PAGE_ADMIN_RESERVATIONS:
            circulation_screen_draw_admin_reservations(
                &app->console, &app->readers, &app->library,
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_ADMIN_FINES:
            circulation_screen_draw_admin_fines(
                &app->console, &app->readers, &app->library,
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_ADMIN_STATS:
            admin_screen_draw_stats(
                &app->console, &app->readers, &app->library,
                app->list_selection, app->status_message
            );
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
            register_form_init(&app->register_form);
            app->page = APP_PAGE_REGISTER;
            break;
        case WELCOME_HELP:
            set_status(
                app,
                L"帮助：使用方向键、Tab 和 Enter 操作；Ctrl+Q 退出。"
            );
            break;
        case WELCOME_EXIT:
            app->running = false;
            break;
    }
}

static void handle_welcome_event(App *app, const UiEvent *event) {
    if (event->type == UI_EVENT_KEY) {
        switch (event->key) {
            case UI_KEY_TAB:
            case UI_KEY_DOWN:
                app->welcome_selection =
                    (app->welcome_selection + 1) % WELCOME_ITEM_COUNT;
                app->status_message[0] = L'\0';
                break;
            case UI_KEY_SHIFT_TAB:
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
                    L"帮助：支持左键点击、列表双击、滚轮翻页和右键返回。"
                );
                break;
            default:
                break;
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
        &app->readers,
        app->login_form.username,
        app->login_form.password,
        &app->session
    );
    switch (result) {
        case AUTH_RESULT_SUCCESS:
            login_form_clear_password(&app->login_form);
            app->home_selection = 0;
            app->status_message[0] = L'\0';
            app->page = app->session.role == USER_ROLE_ADMIN
                ? APP_PAGE_ADMIN_HOME
                : APP_PAGE_READER_HOME;
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

static void open_login_after_registration(App *app) {
    login_form_init(&app->login_form);
    wcsncpy(
        app->login_form.username,
        app->register_form.reader_no,
        LOGIN_USERNAME_CAPACITY - 1
    );
    app->login_form.username[LOGIN_USERNAME_CAPACITY - 1] = L'\0';
    app->login_form.username_length = wcslen(app->login_form.username);
    app->login_form.carets[LOGIN_FIELD_USERNAME] =
        app->login_form.username_length;
    app->login_form.active_field = LOGIN_FIELD_PASSWORD;
    login_form_set_message(
        &app->login_form,
        L"注册成功，请输入密码登录。",
        false
    );
    register_form_init(&app->register_form);
    app->page = APP_PAGE_LOGIN;
}

static void submit_registration(App *app) {
    const ReaderRegistrationRequest request = {
        app->register_form.reader_no,
        app->register_form.name,
        app->register_form.password,
        app->register_form.password_confirmation,
        app->register_form.contact
    };

    const RegisterResult result = auth_register_reader(
        &app->readers, &request
    );
    switch (result) {
        case REGISTER_RESULT_SUCCESS:
            open_login_after_registration(app);
            break;
        case REGISTER_RESULT_EMPTY_FIELD:
            register_form_set_message(
                &app->register_form,
                L"注册失败：字段不能为空或内容过长。",
                true
            );
            break;
        case REGISTER_RESULT_INVALID_READER_NO:
            register_form_set_message(
                &app->register_form,
                L"学号/工号需至少 3 位，只能含文字、数字、_ 或 -。",
                true
            );
            break;
        case REGISTER_RESULT_WEAK_PASSWORD:
            register_form_set_message(
                &app->register_form,
                L"注册失败：密码不能少于 6 位。",
                true
            );
            break;
        case REGISTER_RESULT_INVALID_CONTACT:
            app->register_form.active_field = REGISTER_FIELD_CONTACT;
            register_form_set_message(
                &app->register_form, L"手机号须为 11 位数字，以 13～19 开头。", true
            );
            break;
        case REGISTER_RESULT_PASSWORD_MISMATCH:
            register_form_set_message(
                &app->register_form,
                L"注册失败：两次输入的密码不一致。",
                true
            );
            break;
        case REGISTER_RESULT_DUPLICATE_READER_NO:
            register_form_set_message(
                &app->register_form,
                L"注册失败：该学号/工号已经存在。",
                true
            );
            break;
        case REGISTER_RESULT_STORAGE_ERROR:
            register_form_set_message(
                &app->register_form,
                L"注册失败：账户数据保存失败。",
                true
            );
            break;
    }
}

static void handle_register_event(App *app, const UiEvent *event) {
    const RegisterAction action = register_screen_handle_event(
        &app->register_form, event
    );
    if (action == REGISTER_ACTION_SUBMIT) {
        submit_registration(app);
    } else if (action == REGISTER_ACTION_BACK) {
        register_form_init(&app->register_form);
        app->page = APP_PAGE_WELCOME;
        set_status(app, L"已取消注册。");
    }
}

static void handle_login_event(App *app, const UiEvent *event) {
    const LoginAction action = login_screen_handle_event(
        &app->login_form, event
    );
    if (action == LOGIN_ACTION_SUBMIT) {
        submit_login(app);
    } else if (action == LOGIN_ACTION_BACK) {
        login_form_clear_password(&app->login_form);
        app->page = APP_PAGE_WELCOME;
        set_status(app, L"已返回欢迎页。");
    }
}

static void logout_to_welcome(App *app) {
    auth_session_clear(&app->session);
    login_form_init(&app->login_form);
    app->page = APP_PAGE_WELCOME;
    app->welcome_selection = WELCOME_LOGIN;
    app->home_selection = 0;
    set_status(app, L"已退出登录。");
}

static void activate_admin_item(App *app) {
    switch ((AdminMenuItem)app->home_selection) {
        case ADMIN_MENU_BOOKS:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_ADMIN_BOOKS;
            break;
        case ADMIN_MENU_READERS:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_ADMIN_READERS;
            break;
        case ADMIN_MENU_LOANS:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_ADMIN_LOANS;
            break;
        case ADMIN_MENU_RESERVATIONS:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_ADMIN_RESERVATIONS;
            break;
        case ADMIN_MENU_FINES:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_ADMIN_FINES;
            break;
        case ADMIN_MENU_STATS:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_ADMIN_STATS;
            break;
        case ADMIN_MENU_LOGOUT:
            logout_to_welcome(app);
            break;
        case ADMIN_MENU_COUNT:
            break;
    }
}

static void activate_reader_item(App *app) {
    switch ((ReaderMenuItem)app->home_selection) {
        case READER_MENU_SEARCH:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_BOOK_CATALOG;
            break;
        case READER_MENU_MY_LOANS:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_READER_LOANS;
            break;
        case READER_MENU_MY_RESERVATIONS:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_READER_RESERVATIONS;
            break;
        case READER_MENU_MY_FINES:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_READER_FINES;
            break;
        case READER_MENU_LOGOUT:
            logout_to_welcome(app);
            break;
        case READER_MENU_COUNT:
            break;
    }
}

static void handle_home_event(App *app, const UiEvent *event) {
    if (event->type != UI_EVENT_KEY) {
        return;
    }

    const int item_count = app->page == APP_PAGE_ADMIN_HOME
        ? ADMIN_MENU_COUNT
        : READER_MENU_COUNT;
    switch (event->key) {
        case UI_KEY_TAB:
        case UI_KEY_DOWN:
            app->home_selection = (app->home_selection + 1) % item_count;
            app->status_message[0] = L'\0';
            break;
        case UI_KEY_SHIFT_TAB:
        case UI_KEY_UP:
            app->home_selection =
                (app->home_selection + item_count - 1) % item_count;
            app->status_message[0] = L'\0';
            break;
        case UI_KEY_ENTER:
            if (app->page == APP_PAGE_ADMIN_HOME) {
                activate_admin_item(app);
            } else {
                activate_reader_item(app);
            }
            break;
        case UI_KEY_ESCAPE:
            logout_to_welcome(app);
            break;
        case UI_KEY_F1:
            set_status(
                app,
                L"帮助：↑↓或 Tab 选择功能，Enter 进入，Esc 退出登录。"
            );
            break;
        case UI_KEY_F5:
            set_status(app, L"当前页面已刷新。");
            break;
        default:
            break;
    }
}

static int circulation_item_count(const App *app) {
    const int reader_id = current_reader_id(app);
    int count = 0;
    switch (app->page) {
        case APP_PAGE_BOOK_CATALOG:
            return (int)app->library.book_count;
        case APP_PAGE_ADMIN_LOANS:
            return (int)app->library.loan_count;
        case APP_PAGE_ADMIN_FINES:
            return (int)app->library.fine_count;
        case APP_PAGE_ADMIN_RESERVATIONS:
            return (int)app->library.reservation_count;
        case APP_PAGE_READER_LOANS:
            for (size_t i = 0; i < app->library.loan_count; ++i) {
                if (app->library.loans[i].reader_id == reader_id) {
                    ++count;
                }
            }
            return count;
        case APP_PAGE_READER_FINES:
            for (size_t i = 0; i < app->library.fine_count; ++i) {
                if (app->library.fines[i].reader_id == reader_id) {
                    ++count;
                }
            }
            return count;
        case APP_PAGE_READER_RESERVATIONS:
            for (size_t i = 0; i < app->library.reservation_count; ++i) {
                if (app->library.reservations[i].reader_id == reader_id) {
                    ++count;
                }
            }
            return count;
        default:
            return 0;
    }
}

static bool get_home_button_rect(
    const Console *console,
    int index,
    UiRect *rect
) {
    enum { MENU_WIDTH = 44 };
    ScreenLayout layout;
    if (rect == NULL || !screen_get_layout(console, &layout)) {
        return false;
    }
    rect->x = (short)((layout.width - MENU_WIDTH) / 2);
    rect->y = (short)(layout.content_y + 3 + index * 2);
    rect->width = MENU_WIDTH;
    rect->height = 1;
    return true;
}

static bool route_form_mouse(
    App *app,
    const UiEvent *source,
    UiEvent *routed
) {
    ScreenLayout screen;
    if (!screen_get_layout(&app->console, &screen)) {
        return false;
    }

    if (app->page == APP_PAGE_LOGIN) {
        enum { PANEL_WIDTH = 58, PANEL_HEIGHT = 18 };
        const UiRect panel = {
            (short)((screen.width - PANEL_WIDTH) / 2),
            (short)(screen.content_y +
                (screen.content_height - PANEL_HEIGHT) / 2),
            PANEL_WIDTH, PANEL_HEIGHT
        };
        const UiRect username = {
            (short)(panel.x + 15), (short)(panel.y + 3), 38, 3
        };
        const UiRect password = {
            (short)(panel.x + 15), (short)(panel.y + 7), 38, 3
        };
        const UiRect submit = {
            (short)(panel.x + 13), (short)(panel.y + 11), 12, 3
        };
        const UiRect back = {
            (short)(panel.x + 33), (short)(panel.y + 11), 12, 3
        };
        if (widget_contains_point(
                &username, source->mouse_x, source->mouse_y
            )) {
            app->login_form.active_field = LOGIN_FIELD_USERNAME;
            app->login_form.carets[LOGIN_FIELD_USERNAME] =
                widget_input_caret_from_click(&username,
                    app->login_form.username, false,
                    app->login_form.carets[LOGIN_FIELD_USERNAME], source->mouse_x);
            app->login_form.message[0] = L'\0';
            return true;
        }
        if (widget_contains_point(
                &password, source->mouse_x, source->mouse_y
            )) {
            app->login_form.active_field = LOGIN_FIELD_PASSWORD;
            app->login_form.carets[LOGIN_FIELD_PASSWORD] =
                widget_input_caret_from_click(&password,
                    app->login_form.password, true,
                    app->login_form.carets[LOGIN_FIELD_PASSWORD], source->mouse_x);
            app->login_form.message[0] = L'\0';
            return true;
        }
        if (widget_contains_point(
                &submit, source->mouse_x, source->mouse_y
            )) {
            routed->type = UI_EVENT_KEY;
            routed->key = UI_KEY_ENTER;
            return true;
        }
        if (widget_contains_point(&back, source->mouse_x, source->mouse_y)) {
            routed->type = UI_EVENT_KEY;
            routed->key = UI_KEY_ESCAPE;
            return true;
        }
        return false;
    }

    if (app->page == APP_PAGE_REGISTER) {
        enum { PANEL_HEIGHT = 22 };
        if (screen.content_height < PANEL_HEIGHT || screen.width < 62) {
            return false;
        }
        const short panel_width = screen.width >= 70
            ? 66 : (short)(screen.width - 4);
        const short panel_x = (short)((screen.width - panel_width) / 2);
        const short panel_y = (short)(screen.content_y +
            (screen.content_height - PANEL_HEIGHT) / 2);
        for (int index = 0; index < REGISTER_FIELD_COUNT; ++index) {
            const UiRect field = {
                (short)(panel_x + 18),
                (short)(panel_y + 2 + index * 3),
                (short)(panel_width - 23), 3
            };
            if (widget_contains_point(
                    &field, source->mouse_x, source->mouse_y
                )) {
                app->register_form.active_field = (RegisterField)index;
                register_form_place_caret(&app->register_form, &field,
                    source->mouse_x);
                app->register_form.message[0] = L'\0';
                return true;
            }
        }
        const UiRect submit = {
            (short)(panel_x + 14), (short)(panel_y + 17), 14, 3
        };
        const UiRect back = {
            (short)(panel_x + panel_width - 28),
            (short)(panel_y + 17), 14, 3
        };
        if (widget_contains_point(
                &submit, source->mouse_x, source->mouse_y
            )) {
            routed->type = UI_EVENT_KEY;
            routed->key = UI_KEY_ENTER;
            return true;
        }
        if (widget_contains_point(&back, source->mouse_x, source->mouse_y)) {
            routed->type = UI_EVENT_KEY;
            routed->key = UI_KEY_ESCAPE;
            return true;
        }
        return false;
    }

    if (app->page == APP_PAGE_ADMIN_ADD_BOOK ||
        app->page == APP_PAGE_ADMIN_BORROW) {
        const int field_count = app->page == APP_PAGE_ADMIN_ADD_BOOK
            ? ADMIN_BOOK_FORM_FIELD_COUNT : ADMIN_BORROW_FORM_FIELD_COUNT;
        const short panel_width = screen.width >= 76
            ? 70 : (short)(screen.width - 4);
        const short panel_x = (short)((screen.width - panel_width) / 2);
        const short panel_y = (short)(screen.content_y + 2);
        for (int index = 0; index < field_count; ++index) {
            const UiRect field = {
                (short)(panel_x + 17),
                (short)(panel_y + 1 + index * 3),
                (short)(panel_width - 21), 3
            };
            if (widget_contains_point(
                    &field, source->mouse_x, source->mouse_y
                )) {
                if (app->page == APP_PAGE_ADMIN_ADD_BOOK) {
                    app->admin_book_form.active_field = index;
                    admin_book_form_place_caret(&app->admin_book_form, &field,
                        source->mouse_x);
                    app->admin_book_form.message[0] = L'\0';
                } else {
                    app->admin_borrow_form.active_field = index;
                    admin_borrow_form_place_caret(&app->admin_borrow_form, &field,
                        source->mouse_x);
                    app->admin_borrow_form.message[0] = L'\0';
                }
                return true;
            }
        }
        const short action_y = (short)(panel_y + 2 + field_count * 3);
        const UiRect submit = {
            (short)(panel_x + 14), action_y, 14, 3
        };
        const UiRect back = {
            (short)(panel_x + panel_width - 28), action_y, 14, 3
        };
        if (widget_contains_point(
                &submit, source->mouse_x, source->mouse_y
            )) {
            routed->type = UI_EVENT_KEY;
            routed->key = UI_KEY_ENTER;
            return true;
        }
        if (widget_contains_point(&back, source->mouse_x, source->mouse_y)) {
            routed->type = UI_EVENT_KEY;
            routed->key = UI_KEY_ESCAPE;
            return true;
        }
    }
    return false;
}

static void route_mouse_event(
    App *app,
    const UiEvent *source,
    UiEvent *routed
) {
    if (source->type != UI_EVENT_MOUSE) {
        return;
    }

    routed->type = UI_EVENT_NONE;
    routed->key = UI_KEY_NONE;
    if (source->mouse_wheel != 0) {
        routed->type = UI_EVENT_KEY;
        routed->key = source->mouse_wheel > 0 ? UI_KEY_UP : UI_KEY_DOWN;
        return;
    }
    if (source->mouse_button == UI_MOUSE_RIGHT) {
        routed->type = UI_EVENT_KEY;
        routed->key = UI_KEY_ESCAPE;
        return;
    }
    if (source->mouse_button != UI_MOUSE_LEFT) {
        return;
    }

    if (app->page == APP_PAGE_WELCOME) {
        UiRect buttons[WELCOME_ITEM_COUNT];
        if (!get_welcome_buttons(&app->console, buttons)) {
            return;
        }
        for (int index = 0; index < WELCOME_ITEM_COUNT; ++index) {
            if (widget_contains_point(
                    &buttons[index], source->mouse_x, source->mouse_y
                )) {
                app->welcome_selection = index;
                routed->type = UI_EVENT_KEY;
                routed->key = UI_KEY_ENTER;
                return;
            }
        }
        return;
    }

    if (app->page == APP_PAGE_ADMIN_HOME ||
        app->page == APP_PAGE_READER_HOME) {
        const int count = app->page == APP_PAGE_ADMIN_HOME
            ? ADMIN_MENU_COUNT : READER_MENU_COUNT;
        for (int index = 0; index < count; ++index) {
            UiRect button;
            if (get_home_button_rect(&app->console, index, &button) &&
                widget_contains_point(
                    &button, source->mouse_x, source->mouse_y
                )) {
                app->home_selection = index;
                routed->type = UI_EVENT_KEY;
                routed->key = UI_KEY_ENTER;
                return;
            }
        }
        return;
    }

    if (app->page == APP_PAGE_LOGIN || app->page == APP_PAGE_REGISTER ||
        app->page == APP_PAGE_ADMIN_ADD_BOOK ||
        app->page == APP_PAGE_ADMIN_BORROW) {
        route_form_mouse(app, source, routed);
        return;
    }

    if (app->page == APP_PAGE_ADMIN_STATS) {
        const int hit = admin_stats_hit_test(
            &app->console, source->mouse_x, source->mouse_y
        );
        if (hit >= 0) {
            app->list_selection = hit;
            app->status_message[0] = L'\0';
            if (source->double_click) {
                routed->type = UI_EVENT_KEY;
                routed->key = UI_KEY_ENTER;
            }
        }
        return;
    }

    int count = 0;
    if (app->page == APP_PAGE_ADMIN_BOOKS) {
        count = (int)app->library.book_count;
    } else if (app->page == APP_PAGE_ADMIN_READERS) {
        count = (int)app->readers.count;
    } else {
        count = circulation_item_count(app);
    }
    if (count <= 0) {
        return;
    }

    ScreenLayout layout;
    if (!screen_get_layout(&app->console, &layout)) {
        return;
    }
    const short first_row = (short)(layout.content_y + 4);
    const int visible_row = source->mouse_y - first_row;
    if (source->mouse_x < 2 || source->mouse_x >= layout.width - 2 ||
        visible_row < 0 || visible_row >= MOUSE_LIST_VISIBLE_ROWS) {
        return;
    }
    const int start = app->list_selection >= MOUSE_LIST_VISIBLE_ROWS
        ? app->list_selection - MOUSE_LIST_VISIBLE_ROWS + 1 : 0;
    const int clicked = start + visible_row;
    if (clicked >= count) {
        return;
    }
    app->list_selection = clicked;
    app->status_message[0] = L'\0';
    if (source->double_click &&
        app->page != APP_PAGE_ADMIN_BOOKS &&
        app->page != APP_PAGE_ADMIN_READERS &&
        app->page != APP_PAGE_ADMIN_STATS) {
        routed->type = UI_EVENT_KEY;
        routed->key = UI_KEY_ENTER;
    }
}

static const Loan *selected_reader_loan(const App *app) {
    const int reader_id = current_reader_id(app);
    int item = 0;
    for (size_t i = 0; i < app->library.loan_count; ++i) {
        if (app->library.loans[i].reader_id != reader_id) {
            continue;
        }
        if (item++ == app->list_selection) {
            return &app->library.loans[i];
        }
    }
    return NULL;
}

static const Fine *selected_reader_fine(const App *app) {
    const int reader_id = current_reader_id(app);
    int item = 0;
    for (size_t i = 0; i < app->library.fine_count; ++i) {
        if (app->library.fines[i].reader_id != reader_id) {
            continue;
        }
        if (item++ == app->list_selection) {
            return &app->library.fines[i];
        }
    }
    return NULL;
}

static const Reservation *selected_reader_reservation(const App *app) {
    const int reader_id = current_reader_id(app);
    int item = 0;
    for (size_t i = 0; i < app->library.reservation_count; ++i) {
        if (app->library.reservations[i].reader_id != reader_id) {
            continue;
        }
        if (item++ == app->list_selection) {
            return &app->library.reservations[i];
        }
    }
    return NULL;
}

static void show_library_result(App *app, LibraryResult result) {
    switch (result) {
        case LIBRARY_RESULT_SUCCESS:
            set_status(app, L"操作成功。");
            break;
        case LIBRARY_RESULT_BOOK_NOT_FOUND:
            set_status(app, L"操作失败：图书不存在。");
            break;
        case LIBRARY_RESULT_BOOK_UNAVAILABLE:
            set_status(app, L"操作失败：该书无可借副本或已停借。");
            break;
        case LIBRARY_RESULT_READER_NOT_FOUND:
            set_status(app, L"操作失败：读者不存在。");
            break;
        case LIBRARY_RESULT_READER_BLOCKED:
            set_status(app, L"操作失败：账号受限或欠款已达到 10 元。");
            break;
        case LIBRARY_RESULT_LOAN_LIMIT:
            set_status(app, L"操作失败：最多同时借阅 5 本图书。");
            break;
        case LIBRARY_RESULT_ALREADY_BORROWED:
            set_status(app, L"操作失败：你已经借阅了这本书。");
            break;
        case LIBRARY_RESULT_LOAN_NOT_FOUND:
            set_status(app, L"操作失败：借阅记录不存在。");
            break;
        case LIBRARY_RESULT_LOAN_RETURNED:
            set_status(app, L"操作失败：该借阅记录已经归还。");
            break;
        case LIBRARY_RESULT_RENEW_LIMIT:
            set_status(app, L"操作失败：每条借阅记录只能续借一次。");
            break;
        case LIBRARY_RESULT_LOAN_OVERDUE:
            set_status(app, L"操作失败：图书已经逾期，不能续借。");
            break;
        case LIBRARY_RESULT_FINE_NOT_FOUND:
            set_status(app, L"操作失败：罚款单不存在。");
            break;
        case LIBRARY_RESULT_FINE_PAID:
            set_status(app, L"操作失败：该罚款已经缴纳。");
            break;
        case LIBRARY_RESULT_INVALID_INPUT:
            set_status(app, L"操作失败：输入内容无效。");
            break;
        case LIBRARY_RESULT_DUPLICATE_ISBN:
            set_status(app, L"操作失败：ISBN 已存在。");
            break;
        case LIBRARY_RESULT_BOOK_STILL_AVAILABLE:
            set_status(app, L"该书目前仍有可借副本，请直接借书。");
            break;
        case LIBRARY_RESULT_RESERVATION_DUPLICATE:
            set_status(app, L"操作失败：你已经预约了这本书。");
            break;
        case LIBRARY_RESULT_RESERVATION_NOT_FOUND:
            set_status(app, L"操作失败：预约记录不存在。");
            break;
        case LIBRARY_RESULT_RESERVATION_NOT_READY:
            set_status(app, L"操作失败：该预约尚未进入可借状态。");
            break;
        case LIBRARY_RESULT_RESERVATION_CLOSED:
            set_status(app, L"操作失败：该预约已经结束。");
            break;
        case LIBRARY_RESULT_RESERVATION_PENDING:
            set_status(app, L"操作失败：该书有人预约排队，不能续借。");
            break;
        case LIBRARY_RESULT_STORAGE_ERROR:
            set_status(app, L"操作失败：数据保存失败。");
            break;
    }
}

static void activate_circulation_item(App *app, bool renew) {
    LibraryResult result;
    if (app->page == APP_PAGE_BOOK_CATALOG) {
        if (app->list_selection < 0 ||
            (size_t)app->list_selection >= app->library.book_count) {
            set_status(app, L"当前没有可选择的图书。");
            return;
        }
        int loan_id = 0;
        result = library_borrow_book(
            &app->library, &app->readers, current_reader_id(app),
            app->library.books[app->list_selection].id, &loan_id
        );
        if (result == LIBRARY_RESULT_SUCCESS) {
            wchar_t message[192];
            swprintf(message, 192, L"借书成功，借阅单号：%d，借期 30 天。", loan_id);
            set_status(app, message);
        } else {
            show_library_result(app, result);
        }
        return;
    }

    if (app->page == APP_PAGE_READER_LOANS) {
        const Loan *selected = selected_reader_loan(app);
        if (selected == NULL) {
            set_status(app, L"当前没有借阅记录。");
            return;
        }
        const int loan_id = selected->id;
        if (renew) {
            result = library_renew_loan(&app->library, loan_id);
            if (result == LIBRARY_RESULT_SUCCESS) {
                set_status(app, L"续借成功，应还日期已延长 15 天。");
            } else {
                show_library_result(app, result);
            }
        } else {
            int fine_cents = 0;
            result = library_return_loan(&app->library, loan_id, &fine_cents);
            if (result == LIBRARY_RESULT_SUCCESS && fine_cents > 0) {
                wchar_t message[192];
                swprintf(
                    message, 192,
                    L"还书成功；因逾期已生成 %.2f 元罚款。",
                    fine_cents / 100.0
                );
                set_status(app, message);
            } else if (result == LIBRARY_RESULT_SUCCESS) {
                set_status(app, L"还书成功，馆藏可借数量已恢复。");
            } else {
                show_library_result(app, result);
            }
        }
        return;
    }

    if (app->page == APP_PAGE_READER_FINES) {
        const Fine *selected = selected_reader_fine(app);
        if (selected == NULL) {
            set_status(app, L"当前没有罚款记录。");
            return;
        }
        result = library_pay_fine(&app->library, selected->id);
        if (result == LIBRARY_RESULT_SUCCESS) {
            set_status(app, L"模拟缴费成功，缴费时间已记录。");
        } else {
            show_library_result(app, result);
        }
        return;
    }

    if (app->page == APP_PAGE_READER_RESERVATIONS) {
        const Reservation *selected = selected_reader_reservation(app);
        if (selected == NULL) {
            set_status(app, L"当前没有预约记录。");
            return;
        }
        if (selected->status != RESERVATION_STATUS_READY) {
            show_library_result(app, LIBRARY_RESULT_RESERVATION_NOT_READY);
            return;
        }
        int loan_id = 0;
        result = library_borrow_book(
            &app->library, &app->readers, selected->reader_id,
            selected->book_id, &loan_id
        );
        if (result == LIBRARY_RESULT_SUCCESS) {
            wchar_t message[192];
            swprintf(
                message, 192, L"预约图书领取成功，借阅单号：%d。", loan_id
            );
            set_status(app, message);
        } else {
            show_library_result(app, result);
        }
        return;
    }

    if (app->page == APP_PAGE_ADMIN_LOANS) {
        if (app->list_selection < 0 ||
            (size_t)app->list_selection >= app->library.loan_count) {
            set_status(app, L"当前没有借阅记录。");
            return;
        }
        int fine_cents = 0;
        result = library_return_loan(
            &app->library,
            app->library.loans[app->list_selection].id,
            &fine_cents
        );
        if (result == LIBRARY_RESULT_SUCCESS && fine_cents > 0) {
            wchar_t message[192];
            swprintf(
                message, 192, L"代办还书成功，生成罚款 %.2f 元。",
                fine_cents / 100.0
            );
            set_status(app, message);
        } else if (result == LIBRARY_RESULT_SUCCESS) {
            set_status(app, L"管理员代办还书成功。");
        } else {
            show_library_result(app, result);
        }
        return;
    }

    if (app->page == APP_PAGE_ADMIN_FINES) {
        if (app->list_selection < 0 ||
            (size_t)app->list_selection >= app->library.fine_count) {
            set_status(app, L"当前没有罚款记录。");
            return;
        }
        result = library_pay_fine(
            &app->library,
            app->library.fines[app->list_selection].id
        );
        if (result == LIBRARY_RESULT_SUCCESS) {
            set_status(app, L"管理员代收成功，罚款已标记为已缴。");
        } else {
            show_library_result(app, result);
        }
        return;
    }

    if (app->page == APP_PAGE_ADMIN_RESERVATIONS) {
        if (app->list_selection < 0 ||
            (size_t)app->list_selection >= app->library.reservation_count) {
            set_status(app, L"当前没有预约记录。");
            return;
        }
        const Reservation *selected =
            &app->library.reservations[app->list_selection];
        if (selected->status != RESERVATION_STATUS_READY) {
            show_library_result(app, LIBRARY_RESULT_RESERVATION_NOT_READY);
            return;
        }
        int loan_id = 0;
        result = library_borrow_book(
            &app->library, &app->readers, selected->reader_id,
            selected->book_id, &loan_id
        );
        if (result == LIBRARY_RESULT_SUCCESS) {
            wchar_t message[192];
            swprintf(
                message, 192, L"管理员代办预约取书成功，借阅单号：%d。",
                loan_id
            );
            set_status(app, message);
        } else {
            show_library_result(app, result);
        }
    }
}

static void create_selected_book_reservation(App *app) {
    if (app->list_selection < 0 ||
        (size_t)app->list_selection >= app->library.book_count) {
        set_status(app, L"当前没有可选择的图书。");
        return;
    }
    int reservation_id = 0;
    const LibraryResult result = library_create_reservation(
        &app->library, &app->readers, current_reader_id(app),
        app->library.books[app->list_selection].id, &reservation_id
    );
    if (result == LIBRARY_RESULT_SUCCESS) {
        wchar_t message[192];
        swprintf(
            message, 192, L"预约成功，预约单号：%d，已加入等待队列。",
            reservation_id
        );
        set_status(app, message);
    } else {
        show_library_result(app, result);
    }
}

static void close_selected_reservation(
    App *app,
    bool expire_for_demo
) {
    const Reservation *selected = app->page == APP_PAGE_READER_RESERVATIONS
        ? selected_reader_reservation(app)
        : ((app->list_selection >= 0 &&
            (size_t)app->list_selection < app->library.reservation_count)
            ? &app->library.reservations[app->list_selection] : NULL);
    if (selected == NULL) {
        set_status(app, L"当前没有预约记录。");
        return;
    }
    const int reservation_id = selected->id;
    const LibraryResult result = expire_for_demo
        ? library_expire_reservation_for_demo(
            &app->library, reservation_id, time(NULL)
        )
        : library_cancel_reservation(&app->library, reservation_id);
    if (result == LIBRARY_RESULT_SUCCESS) {
        set_status(
            app, expire_for_demo
                ? L"该可借预约已模拟过期，名额已自动顺延。"
                : L"预约已取消，保留副本已自动释放或顺延。"
        );
    } else {
        show_library_result(app, result);
    }
}

static void handle_circulation_event(App *app, const UiEvent *event) {
    if (event->type != UI_EVENT_KEY) {
        return;
    }
    if (!library_refresh_reservations_at(&app->library, time(NULL))) {
        set_status(app, L"预约状态刷新失败，请检查数据文件。");
    }
    const int item_count = circulation_item_count(app);
    switch (event->key) {
        case UI_KEY_TAB:
        case UI_KEY_DOWN:
            if (item_count > 0) {
                app->list_selection = (app->list_selection + 1) % item_count;
            }
            app->status_message[0] = L'\0';
            break;
        case UI_KEY_SHIFT_TAB:
        case UI_KEY_UP:
            if (item_count > 0) {
                app->list_selection =
                    (app->list_selection + item_count - 1) % item_count;
            }
            app->status_message[0] = L'\0';
            break;
        case UI_KEY_HOME:
            app->list_selection = 0;
            break;
        case UI_KEY_END:
            app->list_selection = item_count > 0 ? item_count - 1 : 0;
            break;
        case UI_KEY_ENTER:
            activate_circulation_item(app, false);
            break;
        case UI_KEY_CHARACTER:
            if (app->page == APP_PAGE_BOOK_CATALOG &&
                (event->character == L'r' || event->character == L'R')) {
                create_selected_book_reservation(app);
            } else if ((app->page == APP_PAGE_READER_RESERVATIONS ||
                        app->page == APP_PAGE_ADMIN_RESERVATIONS) &&
                (event->character == L'c' || event->character == L'C')) {
                close_selected_reservation(app, false);
            } else if (app->page == APP_PAGE_ADMIN_RESERVATIONS &&
                (event->character == L'x' || event->character == L'X')) {
                close_selected_reservation(app, true);
            } else if (app->page == APP_PAGE_READER_LOANS &&
                (event->character == L'r' || event->character == L'R')) {
                activate_circulation_item(app, true);
            } else if (app->page == APP_PAGE_ADMIN_LOANS &&
                (event->character == L'b' || event->character == L'B')) {
                admin_borrow_form_init(&app->admin_borrow_form);
                app->page = APP_PAGE_ADMIN_BORROW;
            } else if (app->page == APP_PAGE_ADMIN_LOANS &&
                (event->character == L'o' || event->character == L'O')) {
                if (app->list_selection < 0 ||
                    (size_t)app->list_selection >= app->library.loan_count) {
                    set_status(app, L"当前没有借阅记录。");
                } else {
                    const LibraryResult result =
                        library_mark_loan_overdue_for_demo(
                            &app->library,
                            app->library.loans[app->list_selection].id,
                            3
                        );
                    if (result == LIBRARY_RESULT_SUCCESS) {
                        set_status(
                            app,
                            L"演示：该借阅单已模拟逾期 3 天，可按 Enter 还书生成罚款。"
                        );
                    } else {
                        show_library_result(app, result);
                    }
                }
            }
            break;
        case UI_KEY_ESCAPE:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = (app->page == APP_PAGE_ADMIN_LOANS ||
                         app->page == APP_PAGE_ADMIN_FINES ||
                         app->page == APP_PAGE_ADMIN_RESERVATIONS)
                ? APP_PAGE_ADMIN_HOME : APP_PAGE_READER_HOME;
            break;
        default:
            break;
    }
}

static bool parse_positive_wide(const wchar_t *text, int *value) {
    if (text == NULL || text[0] == L'\0' || value == NULL) {
        return false;
    }
    wchar_t *end = NULL;
    const long parsed = wcstol(text, &end, 10);
    if (end == text || *end != L'\0' || parsed <= 0 || parsed > 2147483647L) {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static void submit_admin_book_form(App *app) {
    int copies = 0;
    if (!parse_positive_wide(app->admin_book_form.copies, &copies)) {
        admin_book_form_set_message(
            &app->admin_book_form, L"副本数必须是大于 0 的整数。", true
        );
        return;
    }
    int book_id = 0;
    const LibraryResult result = library_add_book(
        &app->library,
        app->admin_book_form.isbn,
        app->admin_book_form.title,
        app->admin_book_form.author,
        copies,
        &book_id
    );
    if (result == LIBRARY_RESULT_SUCCESS) {
        wchar_t message[192];
        swprintf(message, 192, L"新增图书成功，图书编号：%d。", book_id);
        app->list_selection = (int)app->library.book_count - 1;
        app->page = APP_PAGE_ADMIN_BOOKS;
        set_status(app, message);
    } else if (result == LIBRARY_RESULT_DUPLICATE_ISBN) {
        admin_book_form_set_message(
            &app->admin_book_form, L"新增失败：ISBN 已经存在。", true
        );
    } else if (result == LIBRARY_RESULT_INVALID_INPUT) {
        admin_book_form_set_message(
            &app->admin_book_form,
            L"ISBN、书名、作者不能为空，副本数范围为 1～999。", true
        );
    } else {
        admin_book_form_set_message(
            &app->admin_book_form, L"新增失败：数据保存失败。", true
        );
    }
}

static void submit_admin_borrow_form(App *app) {
    int book_id = 0;
    if (!parse_positive_wide(app->admin_borrow_form.book_id, &book_id)) {
        admin_borrow_form_set_message(
            &app->admin_borrow_form, L"图书编号必须是大于 0 的整数。", true
        );
        return;
    }
    const ReaderAccount *reader = reader_repository_find_by_number(
        &app->readers, app->admin_borrow_form.reader_no
    );
    if (reader == NULL) {
        admin_borrow_form_set_message(
            &app->admin_borrow_form, L"办理失败：读者账号不存在。", true
        );
        return;
    }
    int loan_id = 0;
    const LibraryResult result = library_borrow_book(
        &app->library, &app->readers, reader->id, book_id, &loan_id
    );
    if (result == LIBRARY_RESULT_SUCCESS) {
        wchar_t message[192];
        swprintf(
            message, 192, L"办理借书成功，借阅单号：%d，借期 30 天。",
            loan_id
        );
        app->list_selection = (int)app->library.loan_count - 1;
        app->page = APP_PAGE_ADMIN_LOANS;
        set_status(app, message);
    } else {
        const wchar_t *message = L"办理失败：数据保存失败。";
        if (result == LIBRARY_RESULT_BOOK_NOT_FOUND) {
            message = L"办理失败：图书编号不存在。";
        } else if (result == LIBRARY_RESULT_BOOK_UNAVAILABLE) {
            message = L"办理失败：该书无可借副本或已停借。";
        } else if (result == LIBRARY_RESULT_READER_BLOCKED) {
            message = L"办理失败：读者已冻结或欠款达到 10 元。";
        } else if (result == LIBRARY_RESULT_LOAN_LIMIT) {
            message = L"办理失败：该读者已达到 5 本借阅上限。";
        } else if (result == LIBRARY_RESULT_ALREADY_BORROWED) {
            message = L"办理失败：该读者已经借阅这本书。";
        }
        admin_borrow_form_set_message(
            &app->admin_borrow_form, message, true
        );
    }
}

static void handle_admin_form_event(App *app, const UiEvent *event) {
    AdminFormAction action = ADMIN_FORM_ACTION_NONE;
    if (app->page == APP_PAGE_ADMIN_ADD_BOOK) {
        action = admin_book_form_handle_event(&app->admin_book_form, event);
        if (action == ADMIN_FORM_ACTION_SUBMIT) {
            submit_admin_book_form(app);
        } else if (action == ADMIN_FORM_ACTION_BACK) {
            app->page = APP_PAGE_ADMIN_BOOKS;
        }
    } else {
        action = admin_borrow_form_handle_event(
            &app->admin_borrow_form, event
        );
        if (action == ADMIN_FORM_ACTION_SUBMIT) {
            submit_admin_borrow_form(app);
        } else if (action == ADMIN_FORM_ACTION_BACK) {
            app->page = APP_PAGE_ADMIN_LOANS;
        }
    }
}

static void move_admin_list_selection(App *app, int direction, int count) {
    if (count > 0) {
        app->list_selection =
            (app->list_selection + direction + count) % count;
    }
    app->status_message[0] = L'\0';
}

static void handle_admin_management_event(App *app, const UiEvent *event) {
    if (event->type != UI_EVENT_KEY) {
        return;
    }
    if (event->key == UI_KEY_ESCAPE) {
        app->list_selection = 0;
        app->status_message[0] = L'\0';
        app->page = APP_PAGE_ADMIN_HOME;
        return;
    }
    const int item_count = app->page == APP_PAGE_ADMIN_BOOKS
        ? (int)app->library.book_count
        : (app->page == APP_PAGE_ADMIN_READERS
            ? (int)app->readers.count : ADMIN_STATS_CARD_COUNT);
    if (app->page == APP_PAGE_ADMIN_STATS && event->key == UI_KEY_ENTER) {
        admin_stats_format_hint(&app->readers, &app->library,
            app->list_selection, app->status_message,
            sizeof(app->status_message) / sizeof(app->status_message[0]));
        return;
    }
    if (event->key == UI_KEY_TAB || event->key == UI_KEY_DOWN) {
        move_admin_list_selection(app, 1, item_count);
        return;
    }
    if (event->key == UI_KEY_SHIFT_TAB || event->key == UI_KEY_UP) {
        move_admin_list_selection(app, -1, item_count);
        return;
    }
    if (event->key == UI_KEY_HOME) {
        app->list_selection = 0;
        return;
    }
    if (event->key == UI_KEY_END) {
        app->list_selection = item_count > 0 ? item_count - 1 : 0;
        return;
    }
    if (event->key != UI_KEY_CHARACTER) {
        return;
    }

    if (app->page == APP_PAGE_ADMIN_BOOKS &&
        (event->character == L'n' || event->character == L'N')) {
        admin_book_form_init(&app->admin_book_form);
        app->page = APP_PAGE_ADMIN_ADD_BOOK;
        return;
    }
    if (app->page == APP_PAGE_ADMIN_BOOKS && item_count > 0 &&
        (event->character == L'+' || event->character == L'=')) {
        const Book *book = &app->library.books[app->list_selection];
        const LibraryResult result = library_add_book_copy(
            &app->library, book->id
        );
        if (result == LIBRARY_RESULT_SUCCESS) {
            set_status(app, L"已为选中图书增加 1 本馆藏副本。");
        } else {
            show_library_result(app, result);
        }
        return;
    }
    if (app->page == APP_PAGE_ADMIN_BOOKS && item_count > 0 &&
        (event->character == L's' || event->character == L'S')) {
        const Book *book = &app->library.books[app->list_selection];
        const LibraryResult result = library_toggle_book_status(
            &app->library, book->id
        );
        if (result == LIBRARY_RESULT_SUCCESS) {
            const Book *updated = &app->library.books[app->list_selection];
            set_status(
                app, updated->status == BOOK_STATUS_AVAILABLE
                    ? L"图书已恢复借阅。" : L"图书已设置为停借。"
            );
        } else {
            show_library_result(app, result);
        }
        return;
    }
    if (app->page == APP_PAGE_ADMIN_READERS && item_count > 0 &&
        (event->character == L'f' || event->character == L'F')) {
        const ReaderAccount *reader = &app->readers.items[app->list_selection];
        if (reader_repository_toggle_frozen(&app->readers, reader->id)) {
            const ReaderAccount *updated =
                &app->readers.items[app->list_selection];
            set_status(
                app, updated->status == READER_STATUS_FROZEN
                    ? L"读者账号已冻结，将无法登录和借书。"
                    : L"读者账号已解冻。"
            );
        } else {
            set_status(app, L"操作失败：读者状态保存失败或账号已注销。");
        }
    }
}

static bool resolve_data_directory(wchar_t path[MAX_PATH]) {
    const DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }
    wchar_t *separator = wcsrchr(path, L'\\');
    if (separator == NULL) {
        return false;
    }
    *separator = L'\0';
    separator = wcsrchr(path, L'\\');
    if (separator == NULL) {
        return false;
    }
    *separator = L'\0';
    const size_t root_length = wcslen(path);
    static const wchar_t suffix[] = L"\\data";
    if (root_length + sizeof(suffix) / sizeof(suffix[0]) > MAX_PATH) {
        return false;
    }
    wcscat(path, suffix);
    return true;
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
    app->home_selection = 0;
    app->running = true;
    login_form_init(&app->login_form);
    register_form_init(&app->register_form);
    admin_book_form_init(&app->admin_book_form);
    admin_borrow_form_init(&app->admin_borrow_form);
    auth_session_clear(&app->session);

    wchar_t data_directory[MAX_PATH];
    if (!resolve_data_directory(data_directory) ||
        !reader_repository_init(&app->readers, data_directory)) {
        console_shutdown(&app->console);
        return false;
    }

    if (!library_repository_init(&app->library, data_directory)) {
        reader_repository_dispose(&app->readers);
        console_shutdown(&app->console);
        return false;
    }

    if (app->console.has_input_mode &&
        !input_init(&app->input, &app->console)) {
        library_repository_dispose(&app->library);
        reader_repository_dispose(&app->readers);
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

    unsigned long long last_render_revision = app_render_revision(app, time(NULL));
    while (app->running) {
        UiEvent event;
        if (!input_read(&app->input, &event)) {
            return EXIT_FAILURE;
        }

        if (!reader_repository_sync(&app->readers) ||
            !library_repository_sync(&app->library)) {
            set_status(
                app,
                L"数据同步失败，请检查 data 目录是否可读写。"
            );
        } else if (event.type == UI_EVENT_KEY && event.key == UI_KEY_F5) {
            set_status(app, L"已从其他终端同步最新数据。");
        }

        UiEvent routed_event = event;
        route_mouse_event(app, &event, &routed_event);

        if (routed_event.type == UI_EVENT_KEY &&
            routed_event.key == UI_KEY_CTRL_Q) {
            app->running = false;
            continue;
        }

        switch (app->page) {
            case APP_PAGE_WELCOME:
                handle_welcome_event(app, &routed_event);
                break;
            case APP_PAGE_LOGIN:
                handle_login_event(app, &routed_event);
                break;
            case APP_PAGE_REGISTER:
                handle_register_event(app, &routed_event);
                break;
            case APP_PAGE_ADMIN_HOME:
            case APP_PAGE_READER_HOME:
                handle_home_event(app, &routed_event);
                break;
            case APP_PAGE_BOOK_CATALOG:
            case APP_PAGE_READER_LOANS:
            case APP_PAGE_READER_RESERVATIONS:
            case APP_PAGE_READER_FINES:
            case APP_PAGE_ADMIN_LOANS:
            case APP_PAGE_ADMIN_RESERVATIONS:
            case APP_PAGE_ADMIN_FINES:
                handle_circulation_event(app, &routed_event);
                break;
            case APP_PAGE_ADMIN_BOOKS:
            case APP_PAGE_ADMIN_READERS:
            case APP_PAGE_ADMIN_STATS:
                handle_admin_management_event(app, &routed_event);
                break;
            case APP_PAGE_ADMIN_ADD_BOOK:
            case APP_PAGE_ADMIN_BORROW:
                handle_admin_form_event(app, &routed_event);
                break;
        }

        const bool window_resized = routed_event.type == UI_EVENT_RESIZE;
        const unsigned long long revision = app_render_revision(app, time(NULL));
        const bool visible_state_changed = last_render_revision != revision;
        if (app->running && (window_resized || visible_state_changed)) {
            draw_current_page(app);
            last_render_revision = revision;
        }
    }

    return EXIT_SUCCESS;
}

void app_shutdown(App *app) {
    if (app == NULL) {
        return;
    }

    auth_session_clear(&app->session);
    library_repository_dispose(&app->library);
    reader_repository_dispose(&app->readers);
    console_shutdown(&app->console);
    app->running = false;
}
