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
    DeleteFileW(L"tests\\tmp_circulation\\reservations.csv.tmp");
    DeleteFileW(L"tests\\tmp_circulation\\reservations.csv");
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
    check(library.book_count == 20, "seed rich demonstration catalog");
    int added_book_id = 0;
    check(
        library_add_book(
            &library, L"9787300000001", L"管理员新增图书",
            L"测试作者", 2, &added_book_id
        ) == LIBRARY_RESULT_SUCCESS && library.book_count == 21,
        "admin adds a new book"
    );
    check(
        library_add_book(
            &library, L"9787300000001", L"重复 ISBN",
            L"测试作者", 1, NULL
        ) == LIBRARY_RESULT_DUPLICATE_ISBN,
        "reject duplicate ISBN"
    );
    check(
        library_add_book_copy(&library, added_book_id) ==
            LIBRARY_RESULT_SUCCESS &&
        library_find_book(&library, added_book_id)->total_copies == 3 &&
        library_find_book(&library, added_book_id)->available_copies == 3,
        "admin adds one inventory copy"
    );
    check(
        library_toggle_book_status(&library, added_book_id) ==
            LIBRARY_RESULT_SUCCESS &&
        library_find_book(&library, added_book_id)->status ==
            BOOK_STATUS_SUSPENDED,
        "admin suspends a book"
    );

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
    check(
        library_borrow_book(
            &library, &readers, reader_id, added_book_id, NULL
        ) == LIBRARY_RESULT_BOOK_UNAVAILABLE &&
        library_toggle_book_status(&library, added_book_id) ==
            LIBRARY_RESULT_SUCCESS,
        "suspended book cannot be borrowed and can be restored"
    );
    check(
        reader_repository_toggle_frozen(&readers, reader_id) &&
        library_borrow_book(
            &library, &readers, reader_id, 1, NULL
        ) == LIBRARY_RESULT_READER_BLOCKED &&
        reader_repository_toggle_frozen(&readers, reader_id),
        "admin freeze blocks borrowing and unfreeze restores access"
    );

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

    int queue_book_id = 0;
    check(
        library_add_book(
            &library, L"9787300000002", L"预约测试图书",
            L"测试作者", 1, &queue_book_id
        ) == LIBRARY_RESULT_SUCCESS,
        "create one-copy book for reservation queue"
    );
    const ReaderRegistrationRequest reader_two_request = {
        L"reserve-2", L"预约读者二", L"reader123", L"reader123", L"10002"
    };
    const ReaderRegistrationRequest reader_three_request = {
        L"reserve-3", L"预约读者三", L"reader123", L"reader123", L"10003"
    };
    const ReaderRegistrationRequest reader_four_request = {
        L"reserve-4", L"预约读者四", L"reader123", L"reader123", L"10004"
    };
    check(
        auth_register_reader(&readers, &reader_two_request) ==
            REGISTER_RESULT_SUCCESS &&
        auth_register_reader(&readers, &reader_three_request) ==
            REGISTER_RESULT_SUCCESS &&
        auth_register_reader(&readers, &reader_four_request) ==
            REGISTER_RESULT_SUCCESS,
        "register readers for reservation queue"
    );
    const ReaderAccount *reader_two = reader_repository_find_by_number(
        &readers, L"reserve-2"
    );
    const ReaderAccount *reader_three = reader_repository_find_by_number(
        &readers, L"reserve-3"
    );
    const ReaderAccount *reader_four = reader_repository_find_by_number(
        &readers, L"reserve-4"
    );
    const int reader_two_id = reader_two != NULL ? reader_two->id : 0;
    const int reader_three_id = reader_three != NULL ? reader_three->id : 0;
    const int reader_four_id = reader_four != NULL ? reader_four->id : 0;
    int queue_loan_id = 0;
    check(
        library_borrow_book_at(
            &library, &readers, reader_id, queue_book_id,
            local_date(2026, 3, 1), &queue_loan_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_find_book(&library, queue_book_id)->available_copies == 0,
        "borrow final copy before reservations"
    );
    int reservation_two_id = 0;
    int reservation_three_id = 0;
    int reservation_four_id = 0;
    check(
        library_create_reservation_at(
            &library, &readers, reader_two_id, queue_book_id,
            local_date(2026, 3, 2), &reservation_two_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_create_reservation_at(
            &library, &readers, reader_three_id, queue_book_id,
            local_date(2026, 3, 2), &reservation_three_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_create_reservation_at(
            &library, &readers, reader_four_id, queue_book_id,
            local_date(2026, 3, 2), &reservation_four_id
        ) == LIBRARY_RESULT_SUCCESS,
        "readers join reservation queue in order"
    );
    check(
        library_create_reservation_at(
            &library, &readers, reader_two_id, queue_book_id,
            local_date(2026, 3, 2), NULL
        ) == LIBRARY_RESULT_RESERVATION_DUPLICATE,
        "reject duplicate open reservation"
    );
    check(
        library_cancel_reservation_at(
            &library, reservation_four_id, local_date(2026, 3, 2)
        ) == LIBRARY_RESULT_SUCCESS &&
        library_find_reservation(&library, reservation_four_id)->status ==
            RESERVATION_STATUS_CANCELLED,
        "reader cancels waiting reservation"
    );
    check(
        library_renew_loan_at(
            &library, queue_loan_id, local_date(2026, 3, 2)
        ) == LIBRARY_RESULT_RESERVATION_PENDING,
        "reservation queue prevents renewal"
    );
    check(
        library_return_loan_at(
            &library, queue_loan_id, local_date(2026, 3, 3), NULL
        ) == LIBRARY_RESULT_SUCCESS &&
        library_find_reservation(&library, reservation_two_id)->status ==
            RESERVATION_STATUS_READY &&
        strcmp(
            library_find_reservation(&library, reservation_two_id)->expire_date,
            "2026-03-05"
        ) == 0 &&
        library_find_book(&library, queue_book_id)->available_copies == 0,
        "return activates first reservation and holds copy for two days"
    );
    int reserved_loan_id = 0;
    check(
        library_borrow_book_at(
            &library, &readers, reader_two_id, queue_book_id,
            local_date(2026, 3, 3), &reserved_loan_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_find_reservation(&library, reservation_two_id)->status ==
            RESERVATION_STATUS_FULFILLED,
        "ready reader picks up reserved copy"
    );
    check(
        library_return_loan_at(
            &library, reserved_loan_id, local_date(2026, 3, 4), NULL
        ) == LIBRARY_RESULT_SUCCESS &&
        library_find_reservation(&library, reservation_three_id)->status ==
            RESERVATION_STATUS_READY &&
        library_refresh_reservations_at(
            &library, local_date(2026, 3, 7)
        ) &&
        library_find_reservation(&library, reservation_three_id)->status ==
            RESERVATION_STATUS_EXPIRED &&
        library_find_book(&library, queue_book_id)->available_copies == 1,
        "expired hold advances queue and releases copy"
    );
    check(
        library_create_reservation_at(
            &library, &readers, reader_four_id, queue_book_id,
            local_date(2026, 3, 7), NULL
        ) == LIBRARY_RESULT_BOOK_STILL_AVAILABLE,
        "reject reservation while a copy is available"
    );

    library_repository_dispose(&library);
    check(
        library_repository_init(&library, L"tests\\tmp_circulation") &&
            library.book_count == 22 && library.loan_count == 6 &&
            library.fine_count == 2 && library.reservation_count == 3 &&
            library.fines[0].status == FINE_STATUS_PAID &&
            library.fines[1].status == FINE_STATUS_PAID &&
            library.reservations[0].status == RESERVATION_STATUS_FULFILLED &&
            library.reservations[1].status == RESERVATION_STATUS_EXPIRED,
        "reload books loans fines and reservations after restart"
    );

    LibraryRepository second_library;
    check(
        library_repository_init(
            &second_library, L"tests\\tmp_circulation"
        ),
        "open a second terminal repository"
    );
    int shared_book_id = 0;
    check(
        library_add_book(
            &library, L"9787300000999", L"多终端同步测试",
            L"系统测试", 1, &shared_book_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_repository_sync(&second_library) &&
        library_find_book(&second_library, shared_book_id) != NULL,
        "second terminal sees newly added book"
    );
    const size_t shared_loan_count = library.loan_count;
    int shared_loan_id = 0;
    check(
        library_borrow_book_at(
            &library, &readers, reader_id, shared_book_id,
            local_date(2026, 4, 1), &shared_loan_id
        ) == LIBRARY_RESULT_SUCCESS &&
        library_borrow_book_at(
            &second_library, &readers, reader_two_id, shared_book_id,
            local_date(2026, 4, 1), NULL
        ) == LIBRARY_RESULT_BOOK_UNAVAILABLE &&
        library_repository_sync(&library) &&
        library.loan_count == shared_loan_count + 1 &&
        library_find_book(&library, shared_book_id)->available_copies == 0,
        "stale terminal cannot lend the final copy twice"
    );
    check(
        library_return_loan_at(
            &second_library, shared_loan_id, local_date(2026, 4, 2), NULL
        ) == LIBRARY_RESULT_SUCCESS &&
        library_repository_sync(&library) &&
        library_find_book(&library, shared_book_id)->available_copies == 1,
        "return from second terminal appears in first terminal"
    );

    ReaderRepository second_readers;
    check(
        reader_repository_init(
            &second_readers, L"tests\\tmp_circulation"
        ),
        "open a second terminal reader repository"
    );
    const ReaderRegistrationRequest shared_reader_one = {
        L"shared-reader-1", L"同步读者一", L"reader123", L"reader123", L"20001"
    };
    const ReaderRegistrationRequest shared_reader_two = {
        L"shared-reader-2", L"同步读者二", L"reader123", L"reader123", L"20002"
    };
    check(
        auth_register_reader(&readers, &shared_reader_one) ==
            REGISTER_RESULT_SUCCESS &&
        auth_register_reader(&second_readers, &shared_reader_two) ==
            REGISTER_RESULT_SUCCESS &&
        reader_repository_sync(&readers) &&
        reader_repository_find_by_number(
            &readers, L"shared-reader-1"
        ) != NULL &&
        reader_repository_find_by_number(
            &readers, L"shared-reader-2"
        ) != NULL &&
        reader_repository_find_by_number(
            &readers, L"shared-reader-1"
        )->id != reader_repository_find_by_number(
            &readers, L"shared-reader-2"
        )->id,
        "registrations from two terminals do not overwrite each other"
    );

    reader_repository_dispose(&second_readers);
    library_repository_dispose(&second_library);
    library_repository_dispose(&library);
    reader_repository_dispose(&readers);
    clean_test_data();
    printf("\nCirculation test result: %d passed, %d failed.\n",
           passed_count, failed_count);
    return failed_count == 0 ? 0 : 1;
}
