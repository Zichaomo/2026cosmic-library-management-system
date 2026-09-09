#include "ui/circulation_screen.h"

#include <stdio.h>
#include <time.h>
#include <wchar.h>

#include "ui/screen.h"

enum { LIST_START_OFFSET = 4, MAX_VISIBLE_ROWS = 15 };

static const wchar_t *reader_number_by_id(
    const ReaderRepository *readers,
    int reader_id
) {
    if (readers != NULL) {
        for (size_t i = 0; i < readers->count; ++i) {
            if (readers->items[i].id == reader_id) {
                return readers->items[i].reader_no;
            }
        }
    }
    return L"未知";
}

static void draw_message(
    const Console *console,
    const ScreenLayout *layout,
    const wchar_t *message
) {
    if (message != NULL && message[0] != L'\0') {
        const short y =
            (short)(layout->content_y + layout->content_height - 2);
        console_fill_rect(
            console, 2, y, (short)(layout->width - 4), 1, L' ',
            CONSOLE_COLOR_YELLOW, CONSOLE_COLOR_BLACK
        );
        console_write_at(
            console,
            3,
            y,
            message,
            CONSOLE_COLOR_YELLOW,
            CONSOLE_COLOR_BLACK
        );
    }
}

static void draw_row(
    const Console *console,
    const ScreenLayout *layout,
    short y,
    const wchar_t *text,
    bool selected,
    bool warning
) {
    const ConsoleColor foreground = selected
        ? CONSOLE_COLOR_BLACK
        : (warning ? CONSOLE_COLOR_RED : CONSOLE_COLOR_WHITE);
    const ConsoleColor background = selected
        ? CONSOLE_COLOR_CYAN
        : CONSOLE_COLOR_BLACK;
    console_fill_rect(
        console, 2, y, (short)(layout->width - 4), 1, L' ',
        foreground, background
    );
    console_write_at(console, 3, y, text, foreground, background);
}

static const Book *loan_book(
    const LibraryRepository *library,
    const Loan *loan
) {
    return library_find_book(library, loan->book_id);
}

static const wchar_t *reservation_status_name(ReservationStatus status) {
    switch (status) {
        case RESERVATION_STATUS_WAITING: return L"排队中";
        case RESERVATION_STATUS_READY: return L"可借";
        case RESERVATION_STATUS_FULFILLED: return L"已取书";
        case RESERVATION_STATUS_EXPIRED: return L"已过期";
        case RESERVATION_STATUS_CANCELLED: return L"已取消";
    }
    return L"未知";
}

static int reservation_queue_position(
    const LibraryRepository *library,
    size_t reservation_index
) {
    const Reservation *target = &library->reservations[reservation_index];
    if (target->status != RESERVATION_STATUS_WAITING) {
        return 0;
    }
    int position = 1;
    for (size_t i = 0; i < reservation_index; ++i) {
        if (library->reservations[i].book_id == target->book_id &&
            library->reservations[i].status == RESERVATION_STATUS_WAITING) {
            ++position;
        }
    }
    return position;
}

void circulation_screen_draw_books(
    const Console *console,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 图书检索 / 自助借书 ",
        L"↑↓选择  Enter 借书  R 无副本时预约  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"编号  书名 / 作者                         馆藏状态",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    const size_t start = selection >= MAX_VISIBLE_ROWS
        ? (size_t)(selection - MAX_VISIBLE_ROWS + 1) : 0;
    int visible = 0;
    for (size_t i = start; i < library->book_count &&
         visible < MAX_VISIBLE_ROWS; ++i, ++visible) {
        const Book *book = &library->books[i];
        wchar_t row[160];
        swprintf(
            row, 160, L"[%d] %.16ls / %.12ls  |  可借 %d/%d%ls",
            book->id, book->title, book->author,
            book->available_copies, book->total_copies,
            book->status == BOOK_STATUS_AVAILABLE ? L"" : L"（停借）"
        );
        draw_row(
            console, &layout,
            (short)(layout.content_y + LIST_START_OFFSET + visible),
            row, (int)i == selection,
            book->available_copies <= 0 ||
                book->status != BOOK_STATUS_AVAILABLE
        );
    }
    draw_message(console, &layout, message);
}

void circulation_screen_draw_reader_loans(
    const Console *console,
    const LibraryRepository *library,
    int reader_id,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 我的借阅 ",
        L"↑↓/Tab 选择  Enter 还书  R 续借  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"借阅单  图书                         借出日期    应还日期    状态",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    int item_index = 0;
    int visible = 0;
    for (size_t i = 0; i < library->loan_count &&
         visible < MAX_VISIBLE_ROWS; ++i) {
        const Loan *loan = &library->loans[i];
        if (loan->reader_id != reader_id) {
            continue;
        }
        const Book *book = loan_book(library, loan);
        const bool overdue = library_loan_is_overdue(loan, time(NULL));
        wchar_t row[180];
        const wchar_t *state = loan->status == LOAN_STATUS_RETURNED
            ? L"已还" : (overdue ? L"逾期" : (loan->renew_count ? L"已续借" : L"借出"));
        swprintf(
            row, 180, L"单号%d  %.14ls  |  %hs 至 %hs  |  %ls",
            loan->id, book != NULL ? book->title : L"未知图书",
            loan->borrow_date, loan->due_date, state
        );
        draw_row(
            console, &layout,
            (short)(layout.content_y + LIST_START_OFFSET + visible),
            row, item_index == selection, overdue
        );
        ++item_index;
        ++visible;
    }
    if (item_index == 0) {
        console_write_at(
            console, 3, (short)(layout.content_y + LIST_START_OFFSET),
            L"暂无借阅记录。请先到“图书检索”中选择图书借阅。",
            CONSOLE_COLOR_DARK_GRAY, CONSOLE_COLOR_BLACK
        );
    }
    draw_message(console, &layout, message);
}

void circulation_screen_draw_reader_fines(
    const Console *console,
    const LibraryRepository *library,
    int reader_id,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    wchar_t status[128];
    swprintf(
        status, 128, L"欠款合计：%.2f 元  |  ↑↓/Tab 选择  Enter 模拟缴费  Esc 返回",
        library_reader_unpaid_fine_cents(library, reader_id) / 100.0
    );
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 我的罚款 ", status
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"罚款单  借阅单  金额        生成日期    状态 / 缴费日期",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    int item_index = 0;
    int visible = 0;
    for (size_t i = 0; i < library->fine_count &&
         visible < MAX_VISIBLE_ROWS; ++i) {
        const Fine *fine = &library->fines[i];
        if (fine->reader_id != reader_id) {
            continue;
        }
        wchar_t row[180];
        swprintf(
            row, 180, L"罚款%d  借阅%d  |  %.2f 元  %hs  |  %ls%hs",
            fine->id, fine->loan_id, fine->amount_cents / 100.0,
            fine->created_at,
            fine->status == FINE_STATUS_UNPAID ? L"未缴" : L"已缴 ",
            fine->status == FINE_STATUS_PAID ? fine->paid_at : ""
        );
        draw_row(
            console, &layout,
            (short)(layout.content_y + LIST_START_OFFSET + visible),
            row, item_index == selection, fine->status == FINE_STATUS_UNPAID
        );
        ++item_index;
        ++visible;
    }
    if (item_index == 0) {
        console_write_at(
            console, 3, (short)(layout.content_y + LIST_START_OFFSET),
            L"暂无罚款记录。按时还书不会产生罚款。",
            CONSOLE_COLOR_DARK_GRAY, CONSOLE_COLOR_BLACK
        );
    }
    draw_message(console, &layout, message);
}

void circulation_screen_draw_reader_reservations(
    const Console *console,
    const LibraryRepository *library,
    int reader_id,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 我的预约 ",
        L"↑↓选择  Enter 领取可借图书  C 取消预约  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"预约单  图书  预约日期 / 保留期限  状态",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    int item_index = 0;
    int visible = 0;
    for (size_t i = 0; i < library->reservation_count &&
         visible < MAX_VISIBLE_ROWS; ++i) {
        const Reservation *reservation = &library->reservations[i];
        if (reservation->reader_id != reader_id) {
            continue;
        }
        const Book *book = library_find_book(library, reservation->book_id);
        const int position = reservation_queue_position(library, i);
        wchar_t row[180];
        if (reservation->status == RESERVATION_STATUS_WAITING) {
            swprintf(
                row, 180, L"预约%d  %.16ls  |  %hs  |  排队中（第%d位）",
                reservation->id, book != NULL ? book->title : L"未知图书",
                reservation->reserve_date, position
            );
        } else if (reservation->status == RESERVATION_STATUS_READY) {
            swprintf(
                row, 180, L"预约%d  %.16ls  |  保留至 %hs  |  可借",
                reservation->id, book != NULL ? book->title : L"未知图书",
                reservation->expire_date
            );
        } else {
            swprintf(
                row, 180, L"预约%d  %.16ls  |  %hs  |  %ls",
                reservation->id, book != NULL ? book->title : L"未知图书",
                reservation->reserve_date,
                reservation_status_name(reservation->status)
            );
        }
        draw_row(
            console, &layout,
            (short)(layout.content_y + LIST_START_OFFSET + visible),
            row, item_index == selection,
            reservation->status == RESERVATION_STATUS_EXPIRED
        );
        ++item_index;
        ++visible;
    }
    if (item_index == 0) {
        console_write_at(
            console, 3, (short)(layout.content_y + LIST_START_OFFSET),
            L"暂无预约记录。图书无可借副本时，可在图书检索页按 R 预约。",
            CONSOLE_COLOR_DARK_GRAY, CONSOLE_COLOR_BLACK
        );
    }
    draw_message(console, &layout, message);
}

void circulation_screen_draw_admin_loans(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 借阅管理 ",
        L"↑↓选择  B 办理借书  Enter 还书  O 模拟逾期  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"借阅单  读者账号        图书                     应还日期    状态",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    const size_t start = selection >= MAX_VISIBLE_ROWS
        ? (size_t)(selection - MAX_VISIBLE_ROWS + 1) : 0;
    int visible = 0;
    for (size_t i = start; i < library->loan_count &&
         visible < MAX_VISIBLE_ROWS; ++i, ++visible) {
        const Loan *loan = &library->loans[i];
        const Book *book = loan_book(library, loan);
        const bool overdue = library_loan_is_overdue(loan, time(NULL));
        wchar_t row[180];
        swprintf(
            row, 180, L"单号%d  %.12ls  %.12ls  |  应还 %hs  |  %ls",
            loan->id, reader_number_by_id(readers, loan->reader_id),
            book != NULL ? book->title : L"未知图书", loan->due_date,
            loan->status == LOAN_STATUS_RETURNED ? L"已还" :
                (overdue ? L"逾期" : L"借出")
        );
        draw_row(
            console, &layout,
            (short)(layout.content_y + LIST_START_OFFSET + visible),
            row, (int)i == selection, overdue
        );
    }
    if (library->loan_count == 0) {
        console_write_at(
            console, 3, (short)(layout.content_y + LIST_START_OFFSET),
            L"暂无借阅记录。",
            CONSOLE_COLOR_DARK_GRAY, CONSOLE_COLOR_BLACK
        );
    }
    draw_message(console, &layout, message);
}

void circulation_screen_draw_admin_fines(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 罚款管理 ",
        L"↑↓/Tab 选择  Enter 管理员代收  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"罚款单  读者账号        借阅单  金额        日期        状态",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    const size_t start = selection >= MAX_VISIBLE_ROWS
        ? (size_t)(selection - MAX_VISIBLE_ROWS + 1) : 0;
    int visible = 0;
    for (size_t i = start; i < library->fine_count &&
         visible < MAX_VISIBLE_ROWS; ++i, ++visible) {
        const Fine *fine = &library->fines[i];
        wchar_t row[180];
        swprintf(
            row, 180, L"罚款%d  %.12ls  借阅%d  |  %.2f 元  %hs  |  %ls",
            fine->id, reader_number_by_id(readers, fine->reader_id),
            fine->loan_id, fine->amount_cents / 100.0,
            fine->created_at,
            fine->status == FINE_STATUS_UNPAID ? L"未缴" : L"已缴"
        );
        draw_row(
            console, &layout,
            (short)(layout.content_y + LIST_START_OFFSET + visible),
            row, (int)i == selection, fine->status == FINE_STATUS_UNPAID
        );
    }
    if (library->fine_count == 0) {
        console_write_at(
            console, 3, (short)(layout.content_y + LIST_START_OFFSET),
            L"暂无罚款记录。逾期图书归还后会自动生成罚款单。",
            CONSOLE_COLOR_DARK_GRAY, CONSOLE_COLOR_BLACK
        );
    }
    draw_message(console, &layout, message);
}

void circulation_screen_draw_admin_reservations(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
) {
    ScreenLayout layout;
    screen_draw_shell(
        console, L"宇宙图书馆管理系统", L" 预约管理 ",
        L"↑↓选择  Enter 代办领取  C 取消  X 模拟过期  Esc 返回"
    );
    if (!screen_get_layout(console, &layout)) {
        return;
    }
    console_write_at(
        console, 3, (short)(layout.content_y + 2),
        L"预约单  读者 / 图书  日期 / 期限  状态",
        CONSOLE_COLOR_LIGHT_GRAY, CONSOLE_COLOR_BLACK
    );
    const size_t start = selection >= MAX_VISIBLE_ROWS
        ? (size_t)(selection - MAX_VISIBLE_ROWS + 1) : 0;
    int visible = 0;
    for (size_t i = start; i < library->reservation_count &&
         visible < MAX_VISIBLE_ROWS; ++i, ++visible) {
        const Reservation *reservation = &library->reservations[i];
        const Book *book = library_find_book(library, reservation->book_id);
        wchar_t row[180];
        swprintf(
            row, 180, L"预约%d  %.12ls / %.14ls  |  %hs %hs  |  %ls",
            reservation->id,
            reader_number_by_id(readers, reservation->reader_id),
            book != NULL ? book->title : L"未知图书",
            reservation->reserve_date,
            reservation->status == RESERVATION_STATUS_READY
                ? reservation->expire_date : "",
            reservation_status_name(reservation->status)
        );
        draw_row(
            console, &layout,
            (short)(layout.content_y + LIST_START_OFFSET + visible),
            row, (int)i == selection,
            reservation->status == RESERVATION_STATUS_EXPIRED
        );
    }
    if (library->reservation_count == 0) {
        console_write_at(
            console, 3, (short)(layout.content_y + LIST_START_OFFSET),
            L"暂无预约记录。",
            CONSOLE_COLOR_DARK_GRAY, CONSOLE_COLOR_BLACK
        );
    }
    draw_message(console, &layout, message);
}
