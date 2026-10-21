#include "ui/admin_screen.h"

#include <string.h>
#include <time.h>
#include <wchar.h>

#include "ui/screen.h"
#include "ui/widget.h"
#include "ui/text_edit.h"

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
    size_t *carets,
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
    if (text_edit_handle(buffer, capacity, length,
            &carets[*active_field], event)) {
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
        book_field_adapter, form, form->lengths, form->carets, form->message
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
        borrow_field_adapter, form, form->lengths, form->carets, form->message
    );
}

static void draw_form(
    const Console *console,
    const wchar_t *page_name,
    const wchar_t *const labels[],
    const wchar_t *const values[],
    int field_count,
    int active_field,
    const size_t *carets,
    const wchar_t *message,
    bool message_is_error
) {
    ScreenLayout screen;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", page_name,
        L"左键定位光标  Tab 切换  左右/Home/End 编辑  Enter 提交  Esc 返回"
    );
    if (!screen_get_layout(console, &screen)) {
        return;
    }
    const short panel_width = screen.width >= 76 ? 70 : (short)(screen.width - 4);
    const short panel_height = (short)(field_count * 3 + 8);
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
        widget_draw_input_at_caret(console, &input, values[i], false,
            i == active_field, carets[i]);
    }
    const short action_y = (short)(panel.y + 2 + field_count * 3);
    const UiRect submit = {
        (short)(panel.x + 14), action_y, 14, 3
    };
    const UiRect back = {
        (short)(panel.x + panel.width - 28), action_y, 14, 3
    };
    widget_draw_button(console, &submit, L"确认提交", false);
    widget_draw_button(console, &back, L"返回", false);
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
        form->carets,
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
        form->carets,
        form->message, form->message_is_error
    );
}

void admin_book_form_place_caret(AdminBookForm *form, const UiRect *rect, short x) {
    size_t capacity = 0;
    wchar_t *value = book_field(form, form->active_field, &capacity);
    form->carets[form->active_field] = widget_input_caret_from_click(
        rect, value, false, form->carets[form->active_field], x);
}

void admin_borrow_form_place_caret(AdminBorrowForm *form, const UiRect *rect, short x) {
    size_t capacity = 0;
    wchar_t *value = borrow_field(form, form->active_field, &capacity);
    form->carets[form->active_field] = widget_input_caret_from_click(
        rect, value, false, form->carets[form->active_field], x);
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
        L"左键/滚轮选择  N 新增  + 副本  S 停借/恢复  右键返回"
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
        const wchar_t *availability = book->status == BOOK_STATUS_SUSPENDED
            ? L"停借"
            : (book->available_copies <= 0 ? L"已借完" : L"在架");
        wchar_t row[180];
        swprintf(
            row, 180, L"[%d] %.18ls  %.14ls / %.10ls  |  %d/%d %ls",
            book->id, book->isbn, book->title, book->author,
            book->available_copies, book->total_copies,
            availability
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
        L"左键/滚轮选择  F 冻结/解冻  右键返回"
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

typedef struct {
    int titles;
    int total_copies;
    int available;
    int on_loan;
    int readers;
    int frozen;
    int active_loans;
    int overdue;
    int history_loans;
    int waiting;
    int ready;
    int unpaid_cents;
    int paid_cents;
} AdminStatsSnapshot;

static void collect_admin_stats(
    const ReaderRepository *readers,
    const LibraryRepository *library,
    AdminStatsSnapshot *stats
) {
    memset(stats, 0, sizeof(*stats));
    if (library == NULL || readers == NULL) {
        return;
    }
    stats->titles = (int)library->book_count;
    stats->readers = (int)readers->count;
    stats->history_loans = (int)library->loan_count;
    for (size_t i = 0; i < library->book_count; ++i) {
        stats->total_copies += library->books[i].total_copies;
        stats->available += library->books[i].available_copies;
    }
    stats->on_loan = stats->total_copies - stats->available;
    for (size_t i = 0; i < library->loan_count; ++i) {
        if (library->loans[i].status == LOAN_STATUS_ACTIVE) {
            ++stats->active_loans;
            if (library_loan_is_overdue(&library->loans[i], time(NULL))) {
                ++stats->overdue;
            }
        }
    }
    for (size_t i = 0; i < library->fine_count; ++i) {
        if (library->fines[i].status == FINE_STATUS_UNPAID) {
            stats->unpaid_cents += library->fines[i].amount_cents;
        } else {
            stats->paid_cents += library->fines[i].amount_cents;
        }
    }
    for (size_t i = 0; i < readers->count; ++i) {
        if (readers->items[i].status == READER_STATUS_FROZEN) {
            ++stats->frozen;
        }
    }
    for (size_t i = 0; i < library->reservation_count; ++i) {
        if (library->reservations[i].status == RESERVATION_STATUS_WAITING) {
            ++stats->waiting;
        } else if (library->reservations[i].status ==
                   RESERVATION_STATUS_READY) {
            ++stats->ready;
        }
    }
}

static bool layout_stats_cards(
    const ScreenLayout *layout,
    UiRect cards[ADMIN_STATS_CARD_COUNT]
) {
    enum { COLUMNS = 4, ROWS = 2, GAP = 1 };
    if (layout == NULL || cards == NULL) {
        return false;
    }
    const short inner_width = (short)(layout->width - 6);
    const short inner_height = (short)(layout->content_height - 5);
    if (inner_width < 56 || inner_height < 12) {
        return false;
    }
    short card_width = (short)((inner_width - GAP * (COLUMNS - 1)) / COLUMNS);
    short card_height = (short)((inner_height - GAP * (ROWS - 1)) / ROWS);
    if (card_width < 13 || card_height < 5) {
        return false;
    }
    if (card_height > 8) {
        card_height = 8;
    }
    const short start_x = 3;
    const short start_y = (short)(layout->content_y + 2);
    for (int index = 0; index < ADMIN_STATS_CARD_COUNT; ++index) {
        const int column = index % COLUMNS;
        const int row = index / COLUMNS;
        cards[index] = (UiRect){
            (short)(start_x + column * (card_width + GAP)),
            (short)(start_y + row * (card_height + GAP)),
            card_width,
            card_height
        };
    }
    return true;
}

static void draw_stats_card(
    const Console *console,
    const UiRect *card,
    const wchar_t *label,
    const wchar_t *value,
    const wchar_t *detail,
    bool selected,
    bool warning
) {
    const ConsoleColor border = selected
        ? CONSOLE_COLOR_YELLOW
        : (warning ? CONSOLE_COLOR_RED : CONSOLE_COLOR_DARK_CYAN);
    const ConsoleColor background = selected
        ? CONSOLE_COLOR_DARK_BLUE
        : CONSOLE_COLOR_BLACK;
    const ConsoleColor value_color = warning
        ? CONSOLE_COLOR_RED
        : (selected ? CONSOLE_COLOR_YELLOW : CONSOLE_COLOR_WHITE);
    console_fill_rect(
        console, card->x, card->y, card->width, card->height, L' ',
        CONSOLE_COLOR_WHITE, background
    );
    widget_draw_box(console, card, border);
    console_write_at(
        console, (short)(card->x + 2), (short)(card->y + 1), label,
        selected ? CONSOLE_COLOR_CYAN : CONSOLE_COLOR_LIGHT_GRAY,
        background
    );
    console_write_at(
        console, (short)(card->x + 2), (short)(card->y + 3), value,
        value_color, background
    );
    if (detail != NULL && detail[0] != L'\0' && card->height >= 6) {
        console_write_at(
            console, (short)(card->x + 2), (short)(card->y + card->height - 2),
            detail, CONSOLE_COLOR_DARK_GRAY, background
        );
    }
}

static void fill_stats_card_copy(
    const AdminStatsSnapshot *stats,
    int index,
    wchar_t *label,
    wchar_t *value,
    wchar_t *detail,
    bool *warning
) {
    *warning = false;
    switch (index) {
        case 0:
            wcscpy(label, L"图书种类");
            swprintf(value, 32, L"%d 种", stats->titles);
            wcscpy(detail, stats->titles == 0 ? L"馆藏为空" : L"书目总量");
            *warning = stats->titles == 0;
            break;
        case 1:
            wcscpy(label, L"馆藏副本");
            swprintf(value, 32, L"%d 本", stats->total_copies);
            swprintf(detail, 40, L"可借 %d · 借出 %d",
                     stats->available, stats->on_loan);
            *warning = stats->available == 0 && stats->total_copies > 0;
            break;
        case 2:
            wcscpy(label, L"当前借阅");
            swprintf(value, 32, L"%d 条", stats->active_loans);
            swprintf(detail, 40, L"历史 %d 条", stats->history_loans);
            break;
        case 3:
            wcscpy(label, L"逾期未还");
            swprintf(value, 32, L"%d 条", stats->overdue);
            wcscpy(detail, stats->overdue > 0 ? L"需要催还" : L"暂无逾期");
            *warning = stats->overdue > 0;
            break;
        case 4:
            wcscpy(label, L"注册读者");
            swprintf(value, 32, L"%d 人", stats->readers);
            swprintf(detail, 40, L"冻结 %d 人", stats->frozen);
            *warning = stats->frozen > 0;
            break;
        case 5:
            wcscpy(label, L"预约队列");
            swprintf(value, 32, L"%d 人", stats->waiting + stats->ready);
            swprintf(detail, 40, L"排队 %d · 可取 %d",
                     stats->waiting, stats->ready);
            break;
        case 6:
            wcscpy(label, L"未缴罚款");
            swprintf(value, 32, L"%.2f 元", stats->unpaid_cents / 100.0);
            wcscpy(detail, stats->unpaid_cents > 0 ? L"待催缴" : L"已结清");
            *warning = stats->unpaid_cents > 0;
            break;
        case 7:
            wcscpy(label, L"罚款收入");
            swprintf(value, 32, L"%.2f 元", stats->paid_cents / 100.0);
            wcscpy(detail, L"已入账合计");
            break;
        default:
            label[0] = L'\0';
            value[0] = L'\0';
            detail[0] = L'\0';
            break;
    }
}

int admin_stats_hit_test(const Console *console, short x, short y) {
    ScreenLayout layout;
    UiRect cards[ADMIN_STATS_CARD_COUNT];
    if (!screen_get_layout(console, &layout) ||
        !layout_stats_cards(&layout, cards)) {
        return -1;
    }
    for (int index = 0; index < ADMIN_STATS_CARD_COUNT; ++index) {
        if (widget_contains_point(&cards[index], x, y)) {
            return index;
        }
    }
    return -1;
}

void admin_stats_format_hint(
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    wchar_t *buffer,
    size_t capacity
) {
    AdminStatsSnapshot stats;
    if (buffer == NULL || capacity == 0) {
        return;
    }
    collect_admin_stats(readers, library, &stats);
    if (selection < 0) {
        selection = 0;
    }
    selection %= ADMIN_STATS_CARD_COUNT;
    switch (selection) {
        case 0:
            swprintf(
                buffer, capacity,
                stats.titles == 0
                    ? L"还没有书目。打开“图书管理”按 N 新增第一本。"
                    : L"馆藏 %d 种。打开“图书管理”可新增或停借。"
                , stats.titles
            );
            break;
        case 1:
            swprintf(
                buffer, capacity,
                L"共 %d 本副本，可借 %d 本，已借出 %d 本。",
                stats.total_copies, stats.available, stats.on_loan
            );
            break;
        case 2:
            swprintf(
                buffer, capacity,
                L"未还 %d 条，历史借阅 %d 条。打开“借阅管理”办理还书。",
                stats.active_loans, stats.history_loans
            );
            break;
        case 3:
            swprintf(
                buffer, capacity,
                stats.overdue > 0
                    ? L"有 %d 条逾期。借阅管理中 Enter 还书结算；O 仅模拟逾期。"
                    : L"当前没有逾期借阅。",
                stats.overdue
            );
            break;
        case 4:
            swprintf(
                buffer, capacity,
                stats.frozen > 0
                    ? L"注册 %d 人，其中 %d 人已冻结。打开“读者管理”处理。"
                    : L"注册读者 %d 人，账号状态正常。",
                stats.readers, stats.frozen
            );
            break;
        case 5:
            swprintf(
                buffer, capacity,
                L"排队 %d 人，可取书 %d 人。打开“预约管理”办理领取。",
                stats.waiting, stats.ready
            );
            break;
        case 6:
            swprintf(
                buffer, capacity,
                stats.unpaid_cents > 0
                    ? L"待缴 %.2f 元。打开“罚款管理”可代收。"
                    : L"没有待缴罚款。",
                stats.unpaid_cents / 100.0
            );
            break;
        default:
            swprintf(
                buffer, capacity,
                L"已入账罚款 %.2f 元。",
                stats.paid_cents / 100.0
            );
            break;
    }
}

void admin_screen_draw_stats(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    UiRect cards[ADMIN_STATS_CARD_COUNT];
    AdminStatsSnapshot stats;
    const wchar_t *status = (message != NULL && message[0] != L'\0')
        ? message
        : L"左键选择指标  Tab/方向键切换  Enter 说明  F5 刷新  右键返回";
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 统计报表 ", status
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    collect_admin_stats(readers, library, &stats);
    if (selection < 0) {
        selection = 0;
    }
    selection %= ADMIN_STATS_CARD_COUNT;

    if (!layout_stats_cards(&layout, cards)) {
        console_write_at(
            console, 4, (short)(layout.content_y + 3),
            L"窗口过窄，无法展示指标卡片。请将终端调整到至少 100 × 30。",
            CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_BLACK
        );
        return;
    }

    for (int index = 0; index < ADMIN_STATS_CARD_COUNT; ++index) {
        wchar_t label[16];
        wchar_t value[32];
        wchar_t detail[40];
        bool warning = false;
        fill_stats_card_copy(
            &stats, index, label, value, detail, &warning
        );
        draw_stats_card(
            console, &cards[index], label, value, detail,
            index == selection, warning
        );
    }

    if (stats.titles == 0 && stats.readers == 0) {
        console_write_at(
            console, 3,
            (short)(layout.content_y + layout.content_height - 2),
            L"空馆状态：还没有书目和读者。从工作台进入图书管理 / 等待注册即可。",
            CONSOLE_COLOR_DARK_GRAY, CONSOLE_COLOR_BLACK
        );
    }
}
