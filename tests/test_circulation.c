#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "core/auth.h"
#include "data/library_repository.h"
#include "data/reader_repository.h"

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

static time_t local_date(int year, int month, int day) {
    struct tm value;
    memset(&value, 0, sizeof(value));
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = 12;
    value.tm_isdst = -1;
    return mktime(&value);
}

static void clean_test_data(void) {
    DeleteFileW(L"tests\\tmp_circulation\\readers.csv.tmp");
    DeleteFileW(L"tests\\tmp_circulation\\readers.csv");
    DeleteFileW(L"tests\\tmp_circulation\\books.csv.tmp");
    DeleteFileW(L"tests\\tmp_circulation\\books.csv");
    DeleteFileW(L"tests\\tmp_circulation\\loans.csv.tmp");
    DeleteFileW(L"tests\\tmp_circulation\\loans.csv");
    DeleteFileW(L"tests\\tmp_circulation\\fines.csv.tmp");
    DeleteFileW(L"tests\\tmp_circulation\\fines.csv");
    RemoveDirectoryW(L"tests\\tmp_circulation");
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    clean_test_data();

    ReaderRepository readers;
    LibraryRepository library;
    check(
        reader_repository_init(&readers, L"tests\\tmp_circulation") &&
            library_repository_init(&library, L"tests\\tmp_circulation"),
        "initialize circulation repositories"
    );
    check(library.book_count == 5, "seed five demonstration books");

    const ReaderRegistrationRequest request = {
        L"loan-test", L"借阅测试读者", L"reader123", L"reader123",
        L"13800138000"
    };
    check(
        auth_register_reader(&readers, &request) == REGISTER_RESULT_SUCCESS,
        "register circulation test reader"
    );
    const ReaderAccount *reader = reader_repository_find_by_number(
        &readers, L"loan-test"
    );
    check(reader != NULL, "find registered reader id");
    const int reader_id = reader != NULL ? reader->id : 0;

    int loan_id = 0;
    check(
        library_borrow_book_at(
            &library, &readers, reader_id, 1,
            local_date(2026, 1, 1), &loan_id
        ) == LIBRARY_RESULT_SUCCESS,
        "borrow an available book"
    );
    check(
        loan_id > 0 && library.books[0].available_copies == 2 &&
            strcmp(library.loans[0].due_date, "2026-01-31") == 0,
        "borrowing reduces availability and sets 30-day due date"
    );
    check(
        library_borrow_book_at(
            &library, &readers, reader_id, 1,
            local_date(2026, 1, 2), NULL
        ) == LIBRARY_RESULT_ALREADY_BORROWED,
        "reject duplicate active loan for same book"
    );
    check(
        library_renew_loan_at(
            &library, loan_id, local_date(2026, 1, 10)
        ) == LIBRARY_RESULT_SUCCESS &&
            strcmp(library.loans[0].due_date, "2026-02-15") == 0,
        "renew once and extend due date by 15 days"
    );
    check(
        library_renew_loan_at(
            &library, loan_id, local_date(2026, 1, 11)
        ) == LIBRARY_RESULT_RENEW_LIMIT,
        "reject second renewal"
    );
    int fine_cents = -1;
    check(
        library_return_loan_at(
            &library, loan_id, local_date(2026, 2, 1), &fine_cents
        ) == LIBRARY_RESULT_SUCCESS && fine_cents == 0 &&
            library.books[0].available_copies == 3,
        "on-time return restores availability without fine"
    );

    int overdue_loan_id = 0;
    check(
        library_borrow_book_at(
            &library, &readers, reader_id, 2,
            local_date(2026, 1, 1), &overdue_loan_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_return_loan_at(
            &library, overdue_loan_id, local_date(2026, 2, 5), &fine_cents
        ) == LIBRARY_RESULT_SUCCESS && fine_cents == 250,
        "overdue return creates 0.50 yuan per day fine"
    );
    check(
        library.fine_count == 1 &&
            library_reader_unpaid_fine_cents(&library, reader_id) == 250,
        "track unpaid fine total"
    );
    check(
        library_pay_fine_at(
            &library, library.fines[0].id, local_date(2026, 2, 5)
        ) == LIBRARY_RESULT_SUCCESS &&
            library_reader_unpaid_fine_cents(&library, reader_id) == 0,
        "pay fine and record paid status"
    );

    int blocked_loan_id = 0;
    check(
        library_borrow_book_at(
            &library, &readers, reader_id, 3,
            local_date(2026, 1, 1), &blocked_loan_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_return_loan_at(
            &library, blocked_loan_id, local_date(2026, 2, 21), &fine_cents
        ) == LIBRARY_RESULT_SUCCESS && fine_cents >= 1000 &&
        library_borrow_book_at(
            &library, &readers, reader_id, 4,
            local_date(2026, 2, 21), NULL
        ) == LIBRARY_RESULT_READER_BLOCKED,
        "block new borrowing when unpaid fines reach threshold"
    );

    int demo_loan_id = 0;
    check(
        library_pay_fine_at(
            &library, library.fines[1].id, local_date(2026, 2, 21)
        ) == LIBRARY_RESULT_SUCCESS &&
        library_borrow_book(
            &library, &readers, reader_id, 4, &demo_loan_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_mark_loan_overdue_for_demo(
            &library, demo_loan_id, 3
        ) == LIBRARY_RESULT_SUCCESS &&
        library_loan_is_overdue(
            library_find_loan(&library, demo_loan_id), time(NULL)
        ),
        "admin demo helper marks an active loan overdue"
    );

    library_repository_dispose(&library);
    check(
        library_repository_init(&library, L"tests\\tmp_circulation") &&
            library.loan_count == 4 && library.fine_count == 2 &&
            library.fines[0].status == FINE_STATUS_PAID &&
            library.fines[1].status == FINE_STATUS_PAID,
        "reload books loans and fines after restart"
    );

    library_repository_dispose(&library);
    reader_repository_dispose(&readers);
    clean_test_data();
    printf("\nCirculation test result: %d passed, %d failed.\n",
           passed_count, failed_count);
    return failed_count == 0 ? 0 : 1;
}
