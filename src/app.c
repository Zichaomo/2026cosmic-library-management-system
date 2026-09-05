#include "app.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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
            : L"方向键或 Tab 选择  |  Enter 确认  |  Ctrl+Q 退出"
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
        case APP_PAGE_READER_FINES:
            circulation_screen_draw_reader_fines(
                &app->console, &app->library, current_reader_id(app),
                app->list_selection, app->status_message
            );
            break;
        case APP_PAGE_ADMIN_LOANS:
            circulation_screen_draw_admin_loans(
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
                    L"帮助：选择“登录”进入账号登录界面，选择“退出”关闭系统。"
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
            set_status(app, L"已选择图书管理，下一阶段接入图书数据。");
            break;
        case ADMIN_MENU_READERS:
            set_status(app, L"已选择读者管理，下一阶段接入读者数据。");
            break;
        case ADMIN_MENU_LOANS:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_ADMIN_LOANS;
            break;
        case ADMIN_MENU_RESERVATIONS:
            set_status(app, L"已选择预约管理，功能尚未实现。");
            break;
        case ADMIN_MENU_FINES:
            app->list_selection = 0;
            app->status_message[0] = L'\0';
            app->page = APP_PAGE_ADMIN_FINES;
            break;
        case ADMIN_MENU_STATS:
            set_status(app, L"已选择统计报表，功能尚未实现。");
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
            set_status(app, L"已选择我的预约，功能尚未实现。");
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
        default:
            return 0;
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
    }
}

static void handle_circulation_event(App *app, const UiEvent *event) {
    if (event->type != UI_EVENT_KEY) {
        return;
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
            if (app->page == APP_PAGE_READER_LOANS &&
                (event->character == L'r' || event->character == L'R')) {
                activate_circulation_item(app, true);
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
                         app->page == APP_PAGE_ADMIN_FINES)
                ? APP_PAGE_ADMIN_HOME : APP_PAGE_READER_HOME;
            break;
        default:
            break;
    }
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
    auth_session_clear(&app->session);

    if (!reader_repository_init(&app->readers, L"data")) {
        console_shutdown(&app->console);
        return false;
    }

    if (!library_repository_init(&app->library, L"data")) {
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

    while (app->running) {
        UiEvent event;
        if (!input_read(&app->input, &event)) {
            return EXIT_FAILURE;
        }

        if (event.type == UI_EVENT_KEY && event.key == UI_KEY_CTRL_Q) {
            app->running = false;
            continue;
        }

        switch (app->page) {
            case APP_PAGE_WELCOME:
                handle_welcome_event(app, &event);
                break;
            case APP_PAGE_LOGIN:
                handle_login_event(app, &event);
                break;
            case APP_PAGE_REGISTER:
                handle_register_event(app, &event);
                break;
            case APP_PAGE_ADMIN_HOME:
            case APP_PAGE_READER_HOME:
                handle_home_event(app, &event);
                break;
            case APP_PAGE_BOOK_CATALOG:
            case APP_PAGE_READER_LOANS:
            case APP_PAGE_READER_FINES:
            case APP_PAGE_ADMIN_LOANS:
            case APP_PAGE_ADMIN_FINES:
                handle_circulation_event(app, &event);
                break;
        }

        if (app->running) {
            draw_current_page(app);
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
