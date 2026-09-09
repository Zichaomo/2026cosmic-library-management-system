#ifndef COSMIC_LIBRARY_DATA_LIBRARY_REPOSITORY_H
#define COSMIC_LIBRARY_DATA_LIBRARY_REPOSITORY_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <windows.h>

#include "data/reader_repository.h"

enum {
    BOOK_ISBN_CAPACITY = 24,
    BOOK_TITLE_CAPACITY = 80,
    BOOK_AUTHOR_CAPACITY = 48,
    LIBRARY_DATE_CAPACITY = 20,
    LIBRARY_LOAN_DAYS = 30,
    LIBRARY_RENEW_DAYS = 15,
    LIBRARY_LOAN_LIMIT = 5,
    LIBRARY_FINE_PER_DAY_CENTS = 50,
    LIBRARY_FINE_BLOCK_CENTS = 1000,
    LIBRARY_RESERVATION_HOLD_DAYS = 2
};

typedef enum {
    BOOK_STATUS_AVAILABLE = 0,
    BOOK_STATUS_SUSPENDED
} BookStatus;

typedef struct {
    int id;
    wchar_t isbn[BOOK_ISBN_CAPACITY];
    wchar_t title[BOOK_TITLE_CAPACITY];
    wchar_t author[BOOK_AUTHOR_CAPACITY];
    int total_copies;
    int available_copies;
    BookStatus status;
    char created_at[LIBRARY_DATE_CAPACITY];
} Book;

typedef enum {
    LOAN_STATUS_ACTIVE = 0,
    LOAN_STATUS_RETURNED
} LoanStatus;

typedef struct {
    int id;
    int book_id;
    int reader_id;
    char borrow_date[LIBRARY_DATE_CAPACITY];
    char due_date[LIBRARY_DATE_CAPACITY];
    char return_date[LIBRARY_DATE_CAPACITY];
    LoanStatus status;
    int renew_count;
} Loan;

typedef enum {
    FINE_STATUS_UNPAID = 0,
    FINE_STATUS_PAID
} FineStatus;

typedef struct {
    int id;
    int reader_id;
    int loan_id;
    int amount_cents;
    char created_at[LIBRARY_DATE_CAPACITY];
    FineStatus status;
    char paid_at[LIBRARY_DATE_CAPACITY];
} Fine;

typedef enum {
    RESERVATION_STATUS_WAITING = 0,
    RESERVATION_STATUS_READY,
    RESERVATION_STATUS_FULFILLED,
    RESERVATION_STATUS_EXPIRED,
    RESERVATION_STATUS_CANCELLED
} ReservationStatus;

typedef struct {
    int id;
    int book_id;
    int reader_id;
    char reserve_date[LIBRARY_DATE_CAPACITY];
    char expire_date[LIBRARY_DATE_CAPACITY];
    ReservationStatus status;
} Reservation;

typedef struct {
    Book *books;
    size_t book_count;
    size_t book_capacity;
    Loan *loans;
    size_t loan_count;
    size_t loan_capacity;
    Fine *fines;
    size_t fine_count;
    size_t fine_capacity;
    Reservation *reservations;
    size_t reservation_count;
    size_t reservation_capacity;
    wchar_t data_directory[MAX_PATH];
    HANDLE transaction_mutex;
} LibraryRepository;

typedef enum {
    LIBRARY_RESULT_SUCCESS = 0,
    LIBRARY_RESULT_BOOK_NOT_FOUND,
    LIBRARY_RESULT_BOOK_UNAVAILABLE,
    LIBRARY_RESULT_READER_NOT_FOUND,
    LIBRARY_RESULT_READER_BLOCKED,
    LIBRARY_RESULT_LOAN_LIMIT,
    LIBRARY_RESULT_ALREADY_BORROWED,
    LIBRARY_RESULT_LOAN_NOT_FOUND,
    LIBRARY_RESULT_LOAN_RETURNED,
    LIBRARY_RESULT_RENEW_LIMIT,
    LIBRARY_RESULT_LOAN_OVERDUE,
    LIBRARY_RESULT_FINE_NOT_FOUND,
    LIBRARY_RESULT_FINE_PAID,
    LIBRARY_RESULT_INVALID_INPUT,
    LIBRARY_RESULT_DUPLICATE_ISBN,
    LIBRARY_RESULT_BOOK_STILL_AVAILABLE,
    LIBRARY_RESULT_RESERVATION_DUPLICATE,
    LIBRARY_RESULT_RESERVATION_NOT_FOUND,
    LIBRARY_RESULT_RESERVATION_NOT_READY,
    LIBRARY_RESULT_RESERVATION_CLOSED,
    LIBRARY_RESULT_RESERVATION_PENDING,
    LIBRARY_RESULT_STORAGE_ERROR
} LibraryResult;

bool library_repository_init(
    LibraryRepository *repository,
    const wchar_t *data_directory
);
void library_repository_dispose(LibraryRepository *repository);
bool library_repository_sync(LibraryRepository *repository);
const Book *library_find_book(const LibraryRepository *repository, int id);
const Loan *library_find_loan(const LibraryRepository *repository, int id);
const Fine *library_find_fine(const LibraryRepository *repository, int id);
const Reservation *library_find_reservation(
    const LibraryRepository *repository,
    int id
);
int library_reader_active_loan_count(
    const LibraryRepository *repository,
    int reader_id
);
int library_reader_unpaid_fine_cents(
    const LibraryRepository *repository,
    int reader_id
);
bool library_loan_is_overdue(const Loan *loan, time_t now);
int library_loan_overdue_days(const Loan *loan, time_t now);

LibraryResult library_borrow_book_at(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    time_t now,
    int *loan_id
);
LibraryResult library_return_loan_at(
    LibraryRepository *repository,
    int loan_id,
    time_t now,
    int *fine_cents
);
LibraryResult library_renew_loan_at(
    LibraryRepository *repository,
    int loan_id,
    time_t now
);
LibraryResult library_pay_fine_at(
    LibraryRepository *repository,
    int fine_id,
    time_t now
);

LibraryResult library_borrow_book(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    int *loan_id
);
LibraryResult library_return_loan(
    LibraryRepository *repository,
    int loan_id,
    int *fine_cents
);
LibraryResult library_renew_loan(
    LibraryRepository *repository,
    int loan_id
);
LibraryResult library_pay_fine(
    LibraryRepository *repository,
    int fine_id
);
LibraryResult library_mark_loan_overdue_for_demo(
    LibraryRepository *repository,
    int loan_id,
    int overdue_days
);
LibraryResult library_add_book(
    LibraryRepository *repository,
    const wchar_t *isbn,
    const wchar_t *title,
    const wchar_t *author,
    int copies,
    int *book_id
);
LibraryResult library_add_book_copy(
    LibraryRepository *repository,
    int book_id
);
LibraryResult library_toggle_book_status(
    LibraryRepository *repository,
    int book_id
);
LibraryResult library_create_reservation_at(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    time_t now,
    int *reservation_id
);
LibraryResult library_cancel_reservation_at(
    LibraryRepository *repository,
    int reservation_id,
    time_t now
);
LibraryResult library_expire_reservation_for_demo(
    LibraryRepository *repository,
    int reservation_id,
    time_t now
);
LibraryResult library_create_reservation(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    int *reservation_id
);
LibraryResult library_cancel_reservation(
    LibraryRepository *repository,
    int reservation_id
);
bool library_refresh_reservations_at(
    LibraryRepository *repository,
    time_t now
);

#endif
