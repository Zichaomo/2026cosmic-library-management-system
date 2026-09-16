#include "ui/admin_screen.h"

#include <string.h>
#include <time.h>
#include <wchar.h>

#include "ui/screen.h"
#include "ui/widget.h"

enum { MAX_VISIBLE_ROWS = 15 };

static void set_text(
    wchar_t *target,
    size_t capacity,
    const wchar_t *message
) {
    wcsncpy(target, message, capacity - 1);
    target[capacity - 1] = L'\0';
}

void admin_book_form_init(AdminBookForm *form) {
    if (form != NULL) {
        memset(form, 0, sizeof(*form));
    }
}

void admin_borrow_form_init(AdminBorrowForm *form) {
    if (form != NULL) {
        memset(form, 0, sizeof(*form));
    }
}

void admin_book_form_set_message(
    AdminBookForm *form,
    const wchar_t *message,
    bool is_error
) {
    if (form != NULL && message != NULL) {
        set_text(form->message, 128, message);
        form->message_is_error = is_error;
    }
}

void admin_borrow_form_set_message(
    AdminBorrowForm *form,
    const wchar_t *message,
    bool is_error
) {
    if (form != NULL && message != NULL) {
        set_text(form->message, 128, message);
        form->message_is_error = is_error;
    }
}

static wchar_t *book_field(
    AdminBookForm *form,
    int field,
    size_t *capacity
) {
    switch (field) {
        case 0: *capacity = BOOK_ISBN_CAPACITY; return form->isbn;
        case 1: *capacity = BOOK_TITLE_CAPACITY; return form->title;
        case 2: *capacity = BOOK_AUTHOR_CAPACITY; return form->author;
        case 3: *capacity = ADMIN_COPIES_CAPACITY; return form->copies;
        default: *capacity = 0; return NULL;
    }
}

static wchar_t *borrow_field(
    AdminBorrowForm *form,
    int field,
    size_t *capacity
) {
    if (field == 0) {
        *capacity = READER_NUMBER_CAPACITY;
        return form->reader_no;
    }
    if (field == 1) {
        *capacity = sizeof(form->book_id) / sizeof(form->book_id[0]);
        return form->book_id;
    }
    *capacity = 0;
    return NULL;
}

static AdminFormAction handle_form_event(
    const UiEvent *event,
    int *active_field,
    int field_count,
    wchar_t *(*get_field)(void *, int, size_t *),
    void *form,
    size_t *lengths,
    wchar_t *message
) {
    if (event == NULL || event->type != UI_EVENT_KEY) {
        return ADMIN_FORM_ACTION_NONE;
    }
    if (event->key == UI_KEY_ENTER) {
        return ADMIN_FORM_ACTION_SUBMIT;
    }
    if (event->key == UI_KEY_ESCAPE) {
        return ADMIN_FORM_ACTION_BACK;
    }
    if (event->key == UI_KEY_TAB || event->key == UI_KEY_DOWN) {
        *active_field = (*active_field + 1) % field_count;
        message[0] = L'\0';
        return ADMIN_FORM_ACTION_NONE;
    }
    if (event->key == UI_KEY_SHIFT_TAB || event->key == UI_KEY_UP) {
        *active_field = (*active_field + field_count - 1) % field_count;
        message[0] = L'\0';
        return ADMIN_FORM_ACTION_NONE;
    }
    size_t capacity = 0;
    wchar_t *buffer = get_field(form, *active_field, &capacity);
    size_t *length = &lengths[*active_field];
    if (event->key == UI_KEY_BACKSPACE && buffer != NULL && *length > 0) {
        buffer[--(*length)] = L'\0';
        message[0] = L'\0';
    } else if (event->key == UI_KEY_CHARACTER && buffer != NULL &&
               *length + 1 < capacity) {
        buffer[(*length)++] = event->character;
        buffer[*length] = L'\0';
        message[0] = L'\0';
    }
    return ADMIN_FORM_ACTION_NONE;
}

static wchar_t *book_field_adapter(void *form, int field, size_t *capacity) {
    return book_field((AdminBookForm *)form, field, capacity);
}

static wchar_t *borrow_field_adapter(void *form, int field, size_t *capacity) {
    return borrow_field((AdminBorrowForm *)form, field, capacity);
}

AdminFormAction admin_book_form_handle_event(
    AdminBookForm *form,
    const UiEvent *event
) {
    if (form == NULL) {
        return ADMIN_FORM_ACTION_NONE;
    }
    return handle_form_event(
        event, &form->active_field, ADMIN_BOOK_FORM_FIELD_COUNT,
        book_field_adapter, form, form->lengths, form->message
    );
}

AdminFormAction admin_borrow_form_handle_event(
    AdminBorrowForm *form,
    const UiEvent *event
) {
    if (form == NULL) {
        return ADMIN_FORM_ACTION_NONE;
    }
    return handle_form_event(
        event, &form->active_field, ADMIN_BORROW_FORM_FIELD_COUNT,
        borrow_field_adapter, form, form->lengths, form->message
    );
}

static void draw_form(
    const Console *console,
    const wchar_t *page_name,
    const wchar_t *const labels[],
    const wchar_t *const values[],
    int field_count,
    int active_field,
    const wchar_t *message,
    bool message_is_error
) {
    ScreenLayout screen;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", page_name,
        L"Tab/方向键切换  Enter 提交  Esc 返回"
    );
    if (!screen_get_layout(console, &screen)) {
        return;
    }
    const short panel_width = screen.width >= 76 ? 70 : (short)(screen.width - 4);
    const short panel_height = (short)(field_count * 3 + 4);
    UiRect panel = {
        (short)((screen.width - panel_width) / 2),
        (short)(screen.content_y + 2),
        panel_width,
        panel_height
    };
    widget_draw_box(console, &panel, CONSOLE_COLOR_CYAN);
    for (int i = 0; i < field_count; ++i) {
        const short y = (short)(panel.y + 1 + i * 3);
        UiRect input = {
            (short)(panel.x + 17), y,
            (short)(panel.width - 21), 3
        };
        console_write_at(
            console, (short)(panel.x + 3), (short)(y + 1), labels[i],
            CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK
        );
        widget_draw_input(console, &input, values[i], false, i == active_field);
    }
    if (message != NULL && message[0] != L'\0') {
        console_write_at(
            console, (short)(panel.x + 3),
            (short)(panel.y + panel.height - 2), message,
            message_is_error ? CONSOLE_COLOR_RED : CONSOLE_COLOR_GREEN,
            CONSOLE_COLOR_BLACK
        );
    }
}

void admin_screen_draw_book_form(
    const Console *console,
    const AdminBookForm *form
) {
    static const wchar_t *const labels[] = {
        L"ISBN：", L"书名：", L"作者：", L"副本数："
    };
    const wchar_t *const values[] = {
        form->isbn, form->title, form->author, form->copies
    };
    draw_form(
        console, L" 新增图书 ", labels, values,
        ADMIN_BOOK_FORM_FIELD_COUNT, form->active_field,
        form->message, form->message_is_error
    );
}

void admin_screen_draw_borrow_form(
    const Console *console,
    const AdminBorrowForm *form
) {
    static const wchar_t *const labels[] = {L"读者账号：", L"图书编号："};
    const wchar_t *const values[] = {form->reader_no, form->book_id};
    draw_form(
        console, L" 管理员办理借书 ", labels, values,
        ADMIN_BORROW_FORM_FIELD_COUNT, form->active_field,
        form->message, form->message_is_error
    );
}

static void draw_list_message(
    const Console *console,
    const ScreenLayout *layout,
    const wchar_t *message
) {
    if (message != NULL && message[0] != L'\0') {
        const short y = (short)(layout->content_y + layout->content_height - 2);
        console_fill_rect(
            console, 2, y, (short)(layout->width - 4), 1, L' ',
            CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_BLACK
        );
        console_write_at(
            console, 3, y, message,
            CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_BLACK
        );
    }
}

static void draw_list_row(
    const Console *console,
    const ScreenLayout *layout,
    short y,
    const wchar_t *text,
    bool selected,
    bool warning
) {
    const ConsoleColor foreground = selected ? CONSOLE_COLOR_BLACK :
        (warning ? CONSOLE_COLOR_RED : CONSOLE_COLOR_WHITE);
    const ConsoleColor background = selected
        ? CONSOLE_COLOR_CYAN : CONSOLE_COLOR_BLACK;
    console_fill_rect(
        console, 2, y, (short)(layout->width - 4), 1, L' ',
        foreground, background
    );
    console_write_at(console, 3, y, text, foreground, background);
}

void admin_screen_draw_books(
    const Console *console,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 图书管理 ",
        L"↑↓选择  N 新增  + 增加副本  S 停借/恢复  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"编号 / ISBN  书名 / 作者  馆藏状态",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    const size_t start = selection >= MAX_VISIBLE_ROWS
        ? (size_t)(selection - MAX_VISIBLE_ROWS + 1) : 0;
    int visible = 0;
    for (size_t i = start; i < library->book_count &&
         visible < MAX_VISIBLE_ROWS; ++i, ++visible) {
        const Book *book = &library->books[i];
        wchar_t row[180];
        swprintf(
            row, 180, L"[%d] %.18ls  %.14ls / %.10ls  |  %d/%d %ls",
            book->id, book->isbn, book->title, book->author,
            book->available_copies, book->total_copies,
            book->status == BOOK_STATUS_AVAILABLE ? L"在架" : L"停借"
        );
        draw_list_row(
            console, &layout, (short)(layout.content_y + 4 + visible),
            row, (int)i == selection,
            book->status == BOOK_STATUS_SUSPENDED
        );
    }
    draw_list_message(console, &layout, message);
}

void admin_screen_draw_readers(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 读者管理 ",
        L"↑↓/Tab 选择  F 冻结/解冻  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"读者账号 / 姓名  联系方式  当前借阅 / 欠款 / 状态",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    const size_t start = selection >= MAX_VISIBLE_ROWS
        ? (size_t)(selection - MAX_VISIBLE_ROWS + 1) : 0;
    int visible = 0;
    for (size_t i = start; i < readers->count &&
         visible < MAX_VISIBLE_ROWS; ++i, ++visible) {
        const ReaderAccount *reader = &readers->items[i];
        wchar_t row[180];
        const wchar_t *status = reader->status == READER_STATUS_NORMAL
            ? L"正常" : (reader->status == READER_STATUS_FROZEN
                ? L"冻结" : L"注销");
        swprintf(
            row, 180, L"%.14ls / %.10ls  %.13ls  |  %d 本 / %.2f 元 / %ls",
            reader->reader_no, reader->name, reader->contact,
            library_reader_active_loan_count(library, reader->id),
            library_reader_unpaid_fine_cents(library, reader->id) / 100.0,
            status
        );
        draw_list_row(
            console, &layout, (short)(layout.content_y + 4 + visible),
            row, (int)i == selection, reader->status != READER_STATUS_NORMAL
        );
    }
    if (readers->count == 0) {
        console_write_at(
            console, 3, (short)(layout.content_y + 4), L"暂无注册读者。",
            CONSOLE_COLOR_DARK_GRAY, CONSOLE_COLOR_BLACK
        );
    }
    draw_list_message(console, &layout, message);
}

void admin_screen_draw_stats(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 统计报表 ",
        L"F5 刷新  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    int total_copies = 0;
    int available = 0;
    int active_loans = 0;
    int overdue = 0;
    int unpaid_cents = 0;
    int paid_cents = 0;
    int frozen = 0;
    int waiting_reservations = 0;
    int ready_reservations = 0;
    for (size_t i = 0; i < library->book_count; ++i) {
        total_copies += library->books[i].total_copies;
        available += library->books[i].available_copies;
    }
    for (size_t i = 0; i < library->loan_count; ++i) {
        if (library->loans[i].status == LOAN_STATUS_ACTIVE) {
            ++active_loans;
            if (library_loan_is_overdue(&library->loans[i], time(NULL))) {
                ++overdue;
            }
        }
    }
    for (size_t i = 0; i < library->fine_count; ++i) {
        if (library->fines[i].status == FINE_STATUS_UNPAID) {
            unpaid_cents += library->fines[i].amount_cents;
        } else {
            paid_cents += library->fines[i].amount_cents;
        }
    }
    for (size_t i = 0; i < readers->count; ++i) {
        if (readers->items[i].status == READER_STATUS_FROZEN) {
            ++frozen;
        }
    }
    for (size_t i = 0; i < library->reservation_count; ++i) {
        if (library->reservations[i].status == RESERVATION_STATUS_WAITING) {
            ++waiting_reservations;
        } else if (library->reservations[i].status ==
                   RESERVATION_STATUS_READY) {
            ++ready_reservations;
        }
    }
    wchar_t lines[8][96];
    swprintf(lines[0], 96, L"图书种类：%zu 种", library->book_count);
    swprintf(lines[1], 96, L"馆藏副本：%d 本，可借：%d 本，借出：%d 本",
             total_copies, available, total_copies - available);
    swprintf(lines[2], 96, L"注册读者：%zu 人，冻结：%d 人", readers->count, frozen);
    swprintf(lines[3], 96, L"当前借阅：%d 条，逾期未还：%d 条", active_loans, overdue);
    swprintf(lines[4], 96, L"历史借阅：%zu 条", library->loan_count);
    swprintf(lines[5], 96, L"预约队列：%d 人排队，%d 人可借",
             waiting_reservations, ready_reservations);
    swprintf(lines[6], 96, L"未缴罚款：%.2f 元", unpaid_cents / 100.0);
    swprintf(lines[7], 96, L"累计罚款收入：%.2f 元", paid_cents / 100.0);
    for (int i = 0; i < 8; ++i) {
        console_write_at(
            console, 8, (short)(layout.content_y + 3 + i * 2), lines[i],
            i == 6 && unpaid_cents > 0 ? CONSOLE_COLOR_RED :
                CONSOLE_COLOR_WHITE,
            CONSOLE_COLOR_BLACK
        );
    }
}
