#include "data/library_repository.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "data/csv.h"
#include "utils/text.h"

enum {
    INITIAL_CAPACITY = 16,
    BOOK_FIELD_COUNT = 8,
    LOAN_FIELD_COUNT = 8,
    FINE_FIELD_COUNT = 7,
    RESERVATION_FIELD_COUNT = 6,
    UTF8_VALUE_CAPACITY = 512
};

static bool refresh_reservations_unlocked(
    LibraryRepository *repository,
    time_t now
);

static unsigned long long data_directory_hash(const wchar_t *path) {
    unsigned long long hash = 1469598103934665603ULL;
    for (const wchar_t *cursor = path; *cursor != L'\0'; ++cursor) {
        hash ^= (unsigned long long)towlower(*cursor);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static HANDLE create_transaction_mutex(const wchar_t *data_directory) {
    wchar_t name[96];
    if (swprintf(
            name,
            sizeof(name) / sizeof(name[0]),
            L"Local\\CosmicLibraryData_%016llX",
            data_directory_hash(data_directory)
        ) < 0) {
        return NULL;
    }
    return CreateMutexW(NULL, FALSE, name);
}

static bool lock_repository(LibraryRepository *repository) {
    if (repository == NULL || repository->transaction_mutex == NULL) {
        return false;
    }
    const DWORD result = WaitForSingleObject(
        repository->transaction_mutex, INFINITE
    );
    return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
}

static void unlock_repository(LibraryRepository *repository) {
    if (repository != NULL && repository->transaction_mutex != NULL) {
        ReleaseMutex(repository->transaction_mutex);
    }
}

static bool ensure_array_capacity(
    void **items,
    size_t *capacity,
    size_t count,
    size_t item_size
) {
    if (count < *capacity) {
        return true;
    }
    const size_t new_capacity = *capacity == 0
        ? INITIAL_CAPACITY
        : *capacity * 2;
    void *new_items = realloc(*items, new_capacity * item_size);
    if (new_items == NULL) {
        return false;
    }
    *items = new_items;
    *capacity = new_capacity;
    return true;
}

static bool parse_positive_int(const char *text, int *value) {
    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed <= 0 || parsed > 2147483647L) {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static bool parse_nonnegative_int(const char *text, int *value) {
    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > 2147483647L) {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static bool build_path(
    const LibraryRepository *repository,
    const wchar_t *file_name,
    wchar_t path[MAX_PATH]
) {
    return swprintf(
        path, MAX_PATH, L"%ls\\%ls", repository->data_directory, file_name
    ) >= 0;
}

static bool format_date(time_t value, char output[LIBRARY_DATE_CAPACITY]) {
    struct tm local_time;
    return value != (time_t)-1 && localtime_s(&local_time, &value) == 0 &&
           strftime(output, LIBRARY_DATE_CAPACITY, "%Y-%m-%d", &local_time) > 0;
}

static time_t parse_end_of_date(const char *text) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (text == NULL || sscanf(text, "%d-%d-%d", &year, &month, &day) != 3) {
        return (time_t)-1;
    }
    struct tm value;
    memset(&value, 0, sizeof(value));
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = 23;
    value.tm_min = 59;
    value.tm_sec = 59;
    value.tm_isdst = -1;
    return mktime(&value);
}

static bool add_days_to_date(
    const char *date,
    int days,
    char output[LIBRARY_DATE_CAPACITY]
) {
    const time_t base = parse_end_of_date(date);
    if (base == (time_t)-1) {
        return false;
    }
    struct tm value;
    if (localtime_s(&value, &base) != 0) {
        return false;
    }
    value.tm_mday += days;
    value.tm_hour = 12;
    const time_t result = mktime(&value);
    return format_date(result, output);
}

static int next_book_id(const LibraryRepository *repository) {
    int next = 1;
    for (size_t i = 0; i < repository->book_count; ++i) {
        if (repository->books[i].id >= next) {
            next = repository->books[i].id + 1;
        }
    }
    return next;
}

static int next_loan_id(const LibraryRepository *repository) {
    int next = 1;
    for (size_t i = 0; i < repository->loan_count; ++i) {
        if (repository->loans[i].id >= next) {
            next = repository->loans[i].id + 1;
        }
    }
    return next;
}

static int next_fine_id(const LibraryRepository *repository) {
    int next = 1;
    for (size_t i = 0; i < repository->fine_count; ++i) {
        if (repository->fines[i].id >= next) {
            next = repository->fines[i].id + 1;
        }
    }
    return next;
}

static int next_reservation_id(const LibraryRepository *repository) {
    int next = 1;
    for (size_t i = 0; i < repository->reservation_count; ++i) {
        if (repository->reservations[i].id >= next) {
            next = repository->reservations[i].id + 1;
        }
    }
    return next;
}

static Book *find_book_mutable(LibraryRepository *repository, int id) {
    for (size_t i = 0; i < repository->book_count; ++i) {
        if (repository->books[i].id == id) {
            return &repository->books[i];
        }
    }
    return NULL;
}

static Loan *find_loan_mutable(LibraryRepository *repository, int id) {
    for (size_t i = 0; i < repository->loan_count; ++i) {
        if (repository->loans[i].id == id) {
            return &repository->loans[i];
        }
    }
    return NULL;
}

static Fine *find_fine_mutable(LibraryRepository *repository, int id) {
    for (size_t i = 0; i < repository->fine_count; ++i) {
        if (repository->fines[i].id == id) {
            return &repository->fines[i];
        }
    }
    return NULL;
}

static Reservation *find_reservation_mutable(
    LibraryRepository *repository,
    int id
) {
    for (size_t i = 0; i < repository->reservation_count; ++i) {
        if (repository->reservations[i].id == id) {
            return &repository->reservations[i];
        }
    }
    return NULL;
}

static bool reservation_is_open(const Reservation *reservation) {
    return reservation->status == RESERVATION_STATUS_WAITING ||
           reservation->status == RESERVATION_STATUS_READY;
}

static bool activate_next_waiting(
    LibraryRepository *repository,
    int book_id,
    time_t now,
    size_t *activated_index
) {
    Book *book = find_book_mutable(repository, book_id);
    if (activated_index != NULL) {
        *activated_index = (size_t)-1;
    }
    if (book == NULL || book->available_copies <= 0) {
        return book != NULL;
    }
    for (size_t i = 0; i < repository->reservation_count; ++i) {
        Reservation *reservation = &repository->reservations[i];
        if (reservation->book_id != book_id ||
            reservation->status != RESERVATION_STATUS_WAITING) {
            continue;
        }
        char today[LIBRARY_DATE_CAPACITY];
        if (!format_date(now, today) || !add_days_to_date(
                today,
                LIBRARY_RESERVATION_HOLD_DAYS,
                reservation->expire_date
            )) {
            return false;
        }
        reservation->status = RESERVATION_STATUS_READY;
        --book->available_copies;
        if (activated_index != NULL) {
            *activated_index = i;
        }
        return true;
    }
    return true;
}

static bool reader_exists_and_normal(
    const ReaderRepository *readers,
    int reader_id,
    bool *normal
) {
    if (readers == NULL || normal == NULL) {
        return false;
    }
    for (size_t i = 0; i < readers->count; ++i) {
        if (readers->items[i].id == reader_id) {
            *normal = readers->items[i].status == READER_STATUS_NORMAL;
            return true;
        }
    }
    return false;
}

static bool write_field(FILE *file, const char *value, bool last) {
    return csv_write_field(file, value) &&
           (last ? fputs("\r\n", file) >= 0 : fputc(',', file) != EOF);
}

static bool finish_atomic_file(
    FILE *file,
    bool success,
    const wchar_t *temporary_path,
    const wchar_t *path
) {
    if (fflush(file) != 0 || fclose(file) != 0) {
        success = false;
    }
    if (!success || !MoveFileExW(
            temporary_path,
            path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        DeleteFileW(temporary_path);
        return false;
    }
    return true;
}

static bool open_atomic_file(
    const wchar_t *path,
    wchar_t temporary_path[MAX_PATH],
    FILE **file
) {
    if (swprintf(temporary_path, MAX_PATH, L"%ls.tmp", path) < 0) {
        return false;
    }
    *file = _wfopen(temporary_path, L"wb");
    if (*file == NULL) {
        return false;
    }
    static const unsigned char BOM[] = {0xEF, 0xBB, 0xBF};
    if (fwrite(BOM, 1, sizeof(BOM), *file) != sizeof(BOM)) {
        fclose(*file);
        DeleteFileW(temporary_path);
        return false;
    }
    return true;
}

static bool save_books(const LibraryRepository *repository) {
    wchar_t path[MAX_PATH];
    wchar_t temporary_path[MAX_PATH];
    FILE *file = NULL;
    if (!build_path(repository, L"books.csv", path) ||
        !open_atomic_file(path, temporary_path, &file)) {
        return false;
    }
    bool success = fputs(
        "id,isbn,title,author,total_copies,available_copies,status,created_at\r\n",
        file
    ) >= 0;
    for (size_t i = 0; success && i < repository->book_count; ++i) {
        const Book *book = &repository->books[i];
        char id[24];
        char total[24];
        char available[24];
        char isbn[UTF8_VALUE_CAPACITY];
        char title[UTF8_VALUE_CAPACITY];
        char author[UTF8_VALUE_CAPACITY];
        snprintf(id, sizeof(id), "%d", book->id);
        snprintf(total, sizeof(total), "%d", book->total_copies);
        snprintf(available, sizeof(available), "%d", book->available_copies);
        success = text_wide_to_utf8(book->isbn, isbn, sizeof(isbn)) &&
            text_wide_to_utf8(book->title, title, sizeof(title)) &&
            text_wide_to_utf8(book->author, author, sizeof(author)) &&
            write_field(file, id, false) &&
            write_field(file, isbn, false) &&
            write_field(file, title, false) &&
            write_field(file, author, false) &&
            write_field(file, total, false) &&
            write_field(file, available, false) &&
            write_field(file, book->status == BOOK_STATUS_AVAILABLE
                ? "available" : "suspended", false) &&
            write_field(file, book->created_at, true);
    }
    return finish_atomic_file(file, success, temporary_path, path);
}

static bool save_loans(const LibraryRepository *repository) {
    wchar_t path[MAX_PATH];
    wchar_t temporary_path[MAX_PATH];
    FILE *file = NULL;
    if (!build_path(repository, L"loans.csv", path) ||
        !open_atomic_file(path, temporary_path, &file)) {
        return false;
    }
    bool success = fputs(
        "id,book_id,reader_id,borrow_date,due_date,return_date,status,renew_count\r\n",
        file
    ) >= 0;
    for (size_t i = 0; success && i < repository->loan_count; ++i) {
        const Loan *loan = &repository->loans[i];
        char id[24];
        char book_id[24];
        char reader_id[24];
        char renew_count[24];
        snprintf(id, sizeof(id), "%d", loan->id);
        snprintf(book_id, sizeof(book_id), "%d", loan->book_id);
        snprintf(reader_id, sizeof(reader_id), "%d", loan->reader_id);
        snprintf(renew_count, sizeof(renew_count), "%d", loan->renew_count);
        success = write_field(file, id, false) &&
            write_field(file, book_id, false) &&
            write_field(file, reader_id, false) &&
            write_field(file, loan->borrow_date, false) &&
            write_field(file, loan->due_date, false) &&
            write_field(file, loan->return_date, false) &&
            write_field(file, loan->status == LOAN_STATUS_ACTIVE
                ? "active" : "returned", false) &&
            write_field(file, renew_count, true);
    }
    return finish_atomic_file(file, success, temporary_path, path);
}

static bool save_fines(const LibraryRepository *repository) {
    wchar_t path[MAX_PATH];
    wchar_t temporary_path[MAX_PATH];
    FILE *file = NULL;
    if (!build_path(repository, L"fines.csv", path) ||
        !open_atomic_file(path, temporary_path, &file)) {
        return false;
    }
    bool success = fputs(
        "id,reader_id,loan_id,amount_cents,created_at,status,paid_at\r\n",
        file
    ) >= 0;
    for (size_t i = 0; success && i < repository->fine_count; ++i) {
        const Fine *fine = &repository->fines[i];
        char id[24];
        char reader_id[24];
        char loan_id[24];
        char amount[24];
        snprintf(id, sizeof(id), "%d", fine->id);
        snprintf(reader_id, sizeof(reader_id), "%d", fine->reader_id);
        snprintf(loan_id, sizeof(loan_id), "%d", fine->loan_id);
        snprintf(amount, sizeof(amount), "%d", fine->amount_cents);
        success = write_field(file, id, false) &&
            write_field(file, reader_id, false) &&
            write_field(file, loan_id, false) &&
            write_field(file, amount, false) &&
            write_field(file, fine->created_at, false) &&
            write_field(file, fine->status == FINE_STATUS_UNPAID
                ? "unpaid" : "paid", false) &&
            write_field(file, fine->paid_at, true);
    }
    return finish_atomic_file(file, success, temporary_path, path);
}

static const char *reservation_status_text(ReservationStatus status) {
    switch (status) {
        case RESERVATION_STATUS_WAITING: return "waiting";
        case RESERVATION_STATUS_READY: return "ready";
        case RESERVATION_STATUS_FULFILLED: return "fulfilled";
        case RESERVATION_STATUS_EXPIRED: return "expired";
        case RESERVATION_STATUS_CANCELLED: return "cancelled";
    }
    return "cancelled";
}

static bool save_reservations(const LibraryRepository *repository) {
    wchar_t path[MAX_PATH];
    wchar_t temporary_path[MAX_PATH];
    FILE *file = NULL;
    if (!build_path(repository, L"reservations.csv", path) ||
        !open_atomic_file(path, temporary_path, &file)) {
        return false;
    }
    bool success = fputs(
        "id,book_id,reader_id,reserve_date,expire_date,status\r\n", file
    ) >= 0;
    for (size_t i = 0; success && i < repository->reservation_count; ++i) {
        const Reservation *reservation = &repository->reservations[i];
        char id[24];
        char book_id[24];
        char reader_id[24];
        snprintf(id, sizeof(id), "%d", reservation->id);
        snprintf(book_id, sizeof(book_id), "%d", reservation->book_id);
        snprintf(reader_id, sizeof(reader_id), "%d", reservation->reader_id);
        success = write_field(file, id, false) &&
            write_field(file, book_id, false) &&
            write_field(file, reader_id, false) &&
            write_field(file, reservation->reserve_date, false) &&
            write_field(file, reservation->expire_date, false) &&
            write_field(file, reservation_status_text(
                reservation->status
            ), true);
    }
    return finish_atomic_file(file, success, temporary_path, path);
}

static bool save_all(const LibraryRepository *repository) {
    return save_books(repository) && save_loans(repository) &&
           save_fines(repository) && save_reservations(repository);
}

static bool load_books(LibraryRepository *repository) {
    wchar_t path[MAX_PATH];
    if (!build_path(repository, L"books.csv", path)) {
        return false;
    }
    FILE *file = _wfopen(path, L"rb");
    if (file == NULL) {
        return errno == ENOENT;
    }
    char line[4096];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return true;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char fields[BOOK_FIELD_COUNT][CSV_FIELD_CAPACITY];
        size_t count = 0;
        Book book;
        memset(&book, 0, sizeof(book));
        if (!csv_parse_line(line, fields, BOOK_FIELD_COUNT, &count) ||
            count != BOOK_FIELD_COUNT ||
            !parse_positive_int(fields[0], &book.id) ||
            !text_utf8_to_wide(fields[1], book.isbn, BOOK_ISBN_CAPACITY) ||
            !text_utf8_to_wide(fields[2], book.title, BOOK_TITLE_CAPACITY) ||
            !text_utf8_to_wide(fields[3], book.author, BOOK_AUTHOR_CAPACITY) ||
            !parse_nonnegative_int(fields[4], &book.total_copies) ||
            !parse_nonnegative_int(fields[5], &book.available_copies) ||
            book.available_copies > book.total_copies ||
            (strcmp(fields[6], "available") != 0 &&
             strcmp(fields[6], "suspended") != 0) ||
            !ensure_array_capacity((void **)&repository->books,
                &repository->book_capacity, repository->book_count,
                sizeof(*repository->books))) {
            fclose(file);
            return false;
        }
        book.status = strcmp(fields[6], "available") == 0
            ? BOOK_STATUS_AVAILABLE : BOOK_STATUS_SUSPENDED;
        strncpy(book.created_at, fields[7], LIBRARY_DATE_CAPACITY - 1);
        repository->books[repository->book_count++] = book;
    }
    const bool success = !ferror(file);
    fclose(file);
    return success;
}

static bool load_loans(LibraryRepository *repository) {
    wchar_t path[MAX_PATH];
    if (!build_path(repository, L"loans.csv", path)) {
        return false;
    }
    FILE *file = _wfopen(path, L"rb");
    if (file == NULL) {
        return errno == ENOENT;
    }
    char line[4096];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return true;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char fields[LOAN_FIELD_COUNT][CSV_FIELD_CAPACITY];
        size_t count = 0;
        Loan loan;
        memset(&loan, 0, sizeof(loan));
        if (!csv_parse_line(line, fields, LOAN_FIELD_COUNT, &count) ||
            count != LOAN_FIELD_COUNT ||
            !parse_positive_int(fields[0], &loan.id) ||
            !parse_positive_int(fields[1], &loan.book_id) ||
            !parse_positive_int(fields[2], &loan.reader_id) ||
            (strcmp(fields[6], "active") != 0 &&
             strcmp(fields[6], "returned") != 0) ||
            !parse_nonnegative_int(fields[7], &loan.renew_count) ||
            !ensure_array_capacity((void **)&repository->loans,
                &repository->loan_capacity, repository->loan_count,
                sizeof(*repository->loans))) {
            fclose(file);
            return false;
        }
        strncpy(loan.borrow_date, fields[3], LIBRARY_DATE_CAPACITY - 1);
        strncpy(loan.due_date, fields[4], LIBRARY_DATE_CAPACITY - 1);
        strncpy(loan.return_date, fields[5], LIBRARY_DATE_CAPACITY - 1);
        loan.status = strcmp(fields[6], "active") == 0
            ? LOAN_STATUS_ACTIVE : LOAN_STATUS_RETURNED;
        repository->loans[repository->loan_count++] = loan;
    }
    const bool success = !ferror(file);
    fclose(file);
    return success;
}

static bool load_fines(LibraryRepository *repository) {
    wchar_t path[MAX_PATH];
    if (!build_path(repository, L"fines.csv", path)) {
        return false;
    }
    FILE *file = _wfopen(path, L"rb");
    if (file == NULL) {
        return errno == ENOENT;
    }
    char line[4096];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return true;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char fields[FINE_FIELD_COUNT][CSV_FIELD_CAPACITY];
        size_t count = 0;
        Fine fine;
        memset(&fine, 0, sizeof(fine));
        if (!csv_parse_line(line, fields, FINE_FIELD_COUNT, &count) ||
            count != FINE_FIELD_COUNT ||
            !parse_positive_int(fields[0], &fine.id) ||
            !parse_positive_int(fields[1], &fine.reader_id) ||
            !parse_positive_int(fields[2], &fine.loan_id) ||
            !parse_nonnegative_int(fields[3], &fine.amount_cents) ||
            (strcmp(fields[5], "unpaid") != 0 &&
             strcmp(fields[5], "paid") != 0) ||
            !ensure_array_capacity((void **)&repository->fines,
                &repository->fine_capacity, repository->fine_count,
                sizeof(*repository->fines))) {
            fclose(file);
            return false;
        }
        strncpy(fine.created_at, fields[4], LIBRARY_DATE_CAPACITY - 1);
        fine.status = strcmp(fields[5], "unpaid") == 0
            ? FINE_STATUS_UNPAID : FINE_STATUS_PAID;
        strncpy(fine.paid_at, fields[6], LIBRARY_DATE_CAPACITY - 1);
        repository->fines[repository->fine_count++] = fine;
    }
    const bool success = !ferror(file);
    fclose(file);
    return success;
}

static bool parse_reservation_status(
    const char *text,
    ReservationStatus *status
) {
    if (strcmp(text, "waiting") == 0) {
        *status = RESERVATION_STATUS_WAITING;
    } else if (strcmp(text, "ready") == 0) {
        *status = RESERVATION_STATUS_READY;
    } else if (strcmp(text, "fulfilled") == 0) {
        *status = RESERVATION_STATUS_FULFILLED;
    } else if (strcmp(text, "expired") == 0) {
        *status = RESERVATION_STATUS_EXPIRED;
    } else if (strcmp(text, "cancelled") == 0) {
        *status = RESERVATION_STATUS_CANCELLED;
    } else {
        return false;
    }
    return true;
}

static bool load_reservations(LibraryRepository *repository) {
    wchar_t path[MAX_PATH];
    if (!build_path(repository, L"reservations.csv", path)) {
        return false;
    }
    FILE *file = _wfopen(path, L"rb");
    if (file == NULL) {
        return errno == ENOENT;
    }
    char line[4096];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return true;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char fields[RESERVATION_FIELD_COUNT][CSV_FIELD_CAPACITY];
        size_t count = 0;
        Reservation reservation;
        memset(&reservation, 0, sizeof(reservation));
        if (!csv_parse_line(
                line, fields, RESERVATION_FIELD_COUNT, &count
            ) || count != RESERVATION_FIELD_COUNT ||
            !parse_positive_int(fields[0], &reservation.id) ||
            !parse_positive_int(fields[1], &reservation.book_id) ||
            !parse_positive_int(fields[2], &reservation.reader_id) ||
            !parse_reservation_status(fields[5], &reservation.status) ||
            !ensure_array_capacity(
                (void **)&repository->reservations,
                &repository->reservation_capacity,
                repository->reservation_count,
                sizeof(*repository->reservations)
            )) {
            fclose(file);
            return false;
        }
        strncpy(
            reservation.reserve_date,
            fields[3],
            LIBRARY_DATE_CAPACITY - 1
        );
        strncpy(
            reservation.expire_date,
            fields[4],
            LIBRARY_DATE_CAPACITY - 1
        );
        repository->reservations[repository->reservation_count++] =
            reservation;
    }
    const bool success = !ferror(file);
    fclose(file);
    return success;
}

static bool add_seed_book(
    LibraryRepository *repository,
    const wchar_t *isbn,
    const wchar_t *title,
    const wchar_t *author,
    int copies,
    const char *created_at
) {
    if (!ensure_array_capacity((void **)&repository->books,
            &repository->book_capacity, repository->book_count,
            sizeof(*repository->books))) {
        return false;
    }
    Book book;
    memset(&book, 0, sizeof(book));
    book.id = next_book_id(repository);
    book.total_copies = copies;
    book.available_copies = copies;
    book.status = BOOK_STATUS_AVAILABLE;
    wcsncpy(book.isbn, isbn, BOOK_ISBN_CAPACITY - 1);
    wcsncpy(book.title, title, BOOK_TITLE_CAPACITY - 1);
    wcsncpy(book.author, author, BOOK_AUTHOR_CAPACITY - 1);
    strncpy(book.created_at, created_at, LIBRARY_DATE_CAPACITY - 1);
    repository->books[repository->book_count++] = book;
    return true;
}

static bool seed_books(LibraryRepository *repository) {
    char today[LIBRARY_DATE_CAPACITY];
    if (!format_date(time(NULL), today)) {
        return false;
    }
    static const struct {
        const wchar_t *code;
        const wchar_t *title;
        const wchar_t *author;
        int copies;
    } catalog[] = {
        {L"9787111128069", L"C程序设计语言", L"Brian W. Kernighan", 3},
        {L"9787115428028", L"数据结构（C语言版）", L"严蔚敏", 2},
        {L"9787111544937", L"深入理解计算机系统", L"Randal E. Bryant", 2},
        {L"9787020002207", L"红楼梦", L"曹雪芹", 4},
        {L"9787544253994", L"百年孤独", L"加西亚·马尔克斯", 2},
        {L"BUCT-CS-004", L"C Primer Plus", L"Stephen Prata", 3},
        {L"BUCT-CS-005", L"算法导论", L"Thomas H. Cormen", 2},
        {L"BUCT-CS-006", L"操作系统概念", L"Abraham Silberschatz", 2},
        {L"BUCT-CS-007", L"计算机网络", L"谢希仁", 3},
        {L"BUCT-EE-001", L"STM32嵌入式系统开发", L"实训资料组", 4},
        {L"BUCT-AU-001", L"自动控制原理", L"胡寿松", 3},
        {L"BUCT-AU-002", L"现代控制理论", L"刘豹", 2},
        {L"BUCT-EE-002", L"信号与系统", L"奥本海姆", 2},
        {L"BUCT-EE-003", L"数字电子技术基础", L"阎石", 3},
        {L"BUCT-LIT-001", L"三体", L"刘慈欣", 4},
        {L"BUCT-LIT-002", L"活着", L"余华", 4},
        {L"BUCT-LIT-003", L"平凡的世界", L"路遥", 3},
        {L"BUCT-LIT-004", L"呐喊", L"鲁迅", 3},
        {L"BUCT-LIT-005", L"小王子", L"圣埃克苏佩里", 3},
        {L"BUCT-SCI-001", L"时间简史", L"斯蒂芬·霍金", 2}
    };

    for (size_t index = 0;
         index < sizeof(catalog) / sizeof(catalog[0]); ++index) {
        if (!add_seed_book(
                repository, catalog[index].code, catalog[index].title,
                catalog[index].author, catalog[index].copies, today
            )) {
            return false;
        }
    }
    return true;
}

static void free_loaded_data(LibraryRepository *repository) {
    free(repository->books);
    free(repository->loans);
    free(repository->fines);
    free(repository->reservations);
    repository->books = NULL;
    repository->loans = NULL;
    repository->fines = NULL;
    repository->reservations = NULL;
    repository->book_count = 0;
    repository->loan_count = 0;
    repository->fine_count = 0;
    repository->reservation_count = 0;
    repository->book_capacity = 0;
    repository->loan_capacity = 0;
    repository->fine_capacity = 0;
    repository->reservation_capacity = 0;
}

static bool reload_from_disk_unlocked(LibraryRepository *repository) {
    LibraryRepository loaded;
    memset(&loaded, 0, sizeof(loaded));
    wcsncpy(loaded.data_directory, repository->data_directory, MAX_PATH - 1);
    if (!load_books(&loaded) || !load_loans(&loaded) ||
        !load_fines(&loaded) || !load_reservations(&loaded)) {
        free_loaded_data(&loaded);
        return false;
    }

    free_loaded_data(repository);
    repository->books = loaded.books;
    repository->book_count = loaded.book_count;
    repository->book_capacity = loaded.book_capacity;
    repository->loans = loaded.loans;
    repository->loan_count = loaded.loan_count;
    repository->loan_capacity = loaded.loan_capacity;
    repository->fines = loaded.fines;
    repository->fine_count = loaded.fine_count;
    repository->fine_capacity = loaded.fine_capacity;
    repository->reservations = loaded.reservations;
    repository->reservation_count = loaded.reservation_count;
    repository->reservation_capacity = loaded.reservation_capacity;
    return true;
}

static bool begin_transaction(LibraryRepository *repository) {
    if (!lock_repository(repository)) {
        return false;
    }
    if (!reload_from_disk_unlocked(repository)) {
        unlock_repository(repository);
        return false;
    }
    return true;
}

bool library_repository_init(
    LibraryRepository *repository,
    const wchar_t *data_directory
) {
    if (repository == NULL || data_directory == NULL ||
        data_directory[0] == L'\0') {
        return false;
    }
    memset(repository, 0, sizeof(*repository));
    wchar_t absolute_directory[MAX_PATH];
    const DWORD path_length = GetFullPathNameW(
        data_directory, MAX_PATH, absolute_directory, NULL
    );
    if (path_length == 0 || path_length >= MAX_PATH ||
        (!CreateDirectoryW(absolute_directory, NULL) &&
         GetLastError() != ERROR_ALREADY_EXISTS)) {
        return false;
    }
    wcsncpy(repository->data_directory, absolute_directory, MAX_PATH - 1);
    repository->transaction_mutex = create_transaction_mutex(
        repository->data_directory
    );
    if (repository->transaction_mutex == NULL ||
        !lock_repository(repository)) {
        library_repository_dispose(repository);
        return false;
    }
    if (!load_books(repository) || !load_loans(repository) ||
        !load_fines(repository) || !load_reservations(repository)) {
        unlock_repository(repository);
        library_repository_dispose(repository);
        return false;
    }
    if (repository->book_count == 0) {
        if (!seed_books(repository) || !save_all(repository)) {
            unlock_repository(repository);
            library_repository_dispose(repository);
            return false;
        }
    } else if (!save_loans(repository) || !save_fines(repository) ||
               !save_reservations(repository)) {
        unlock_repository(repository);
        library_repository_dispose(repository);
        return false;
    }
    if (!refresh_reservations_unlocked(repository, time(NULL))) {
        unlock_repository(repository);
        library_repository_dispose(repository);
        return false;
    }
    unlock_repository(repository);
    return true;
}

void library_repository_dispose(LibraryRepository *repository) {
    if (repository == NULL) {
        return;
    }
    free_loaded_data(repository);
    if (repository->transaction_mutex != NULL) {
        CloseHandle(repository->transaction_mutex);
    }
    memset(repository, 0, sizeof(*repository));
}

bool library_repository_sync(LibraryRepository *repository) {
    if (!lock_repository(repository)) {
        return false;
    }
    const bool success = reload_from_disk_unlocked(repository) &&
        refresh_reservations_unlocked(repository, time(NULL));
    unlock_repository(repository);
    return success;
}

const Book *library_find_book(const LibraryRepository *repository, int id) {
    if (repository == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < repository->book_count; ++i) {
        if (repository->books[i].id == id) {
            return &repository->books[i];
        }
    }
    return NULL;
}

const Loan *library_find_loan(const LibraryRepository *repository, int id) {
    if (repository == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < repository->loan_count; ++i) {
        if (repository->loans[i].id == id) {
            return &repository->loans[i];
        }
    }
    return NULL;
}

const Fine *library_find_fine(const LibraryRepository *repository, int id) {
    if (repository == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < repository->fine_count; ++i) {
        if (repository->fines[i].id == id) {
            return &repository->fines[i];
        }
    }
    return NULL;
}

const Reservation *library_find_reservation(
    const LibraryRepository *repository,
    int id
) {
    if (repository == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < repository->reservation_count; ++i) {
        if (repository->reservations[i].id == id) {
            return &repository->reservations[i];
        }
    }
    return NULL;
}

int library_reader_active_loan_count(
    const LibraryRepository *repository,
    int reader_id
) {
    int count = 0;
    if (repository == NULL) {
        return 0;
    }
    for (size_t i = 0; i < repository->loan_count; ++i) {
        if (repository->loans[i].reader_id == reader_id &&
            repository->loans[i].status == LOAN_STATUS_ACTIVE) {
            ++count;
        }
    }
    return count;
}

int library_reader_unpaid_fine_cents(
    const LibraryRepository *repository,
    int reader_id
) {
    int total = 0;
    if (repository == NULL) {
        return 0;
    }
    for (size_t i = 0; i < repository->fine_count; ++i) {
        if (repository->fines[i].reader_id == reader_id &&
            repository->fines[i].status == FINE_STATUS_UNPAID) {
            total += repository->fines[i].amount_cents;
        }
    }
    return total;
}

bool library_loan_is_overdue(const Loan *loan, time_t now) {
    if (loan == NULL || loan->status != LOAN_STATUS_ACTIVE) {
        return false;
    }
    const time_t due = parse_end_of_date(loan->due_date);
    return due != (time_t)-1 && difftime(now, due) > 0.0;
}

int library_loan_overdue_days(const Loan *loan, time_t now) {
    if (!library_loan_is_overdue(loan, now)) {
        return 0;
    }
    const time_t due = parse_end_of_date(loan->due_date);
    const double seconds = difftime(now, due);
    return (int)(seconds / 86400.0) + 1;
}

static LibraryResult borrow_book_at_unlocked(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    time_t now,
    int *loan_id
) {
    if (repository == NULL) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    if (!refresh_reservations_unlocked(repository, now)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    bool reader_normal = false;
    if (!reader_exists_and_normal(readers, reader_id, &reader_normal)) {
        return LIBRARY_RESULT_READER_NOT_FOUND;
    }
    if (!reader_normal || library_reader_unpaid_fine_cents(
            repository, reader_id
        ) >= LIBRARY_FINE_BLOCK_CENTS) {
        return LIBRARY_RESULT_READER_BLOCKED;
    }
    Book *book = find_book_mutable(repository, book_id);
    if (book == NULL) {
        return LIBRARY_RESULT_BOOK_NOT_FOUND;
    }
    Reservation *ready_reservation = NULL;
    for (size_t i = 0; i < repository->reservation_count; ++i) {
        Reservation *reservation = &repository->reservations[i];
        if (reservation->reader_id == reader_id &&
            reservation->book_id == book_id &&
            reservation->status == RESERVATION_STATUS_READY) {
            ready_reservation = reservation;
            break;
        }
    }
    if (book->status != BOOK_STATUS_AVAILABLE ||
        (book->available_copies <= 0 && ready_reservation == NULL)) {
        return LIBRARY_RESULT_BOOK_UNAVAILABLE;
    }
    if (library_reader_active_loan_count(repository, reader_id) >=
        LIBRARY_LOAN_LIMIT) {
        return LIBRARY_RESULT_LOAN_LIMIT;
    }
    for (size_t i = 0; i < repository->loan_count; ++i) {
        if (repository->loans[i].reader_id == reader_id &&
            repository->loans[i].book_id == book_id &&
            repository->loans[i].status == LOAN_STATUS_ACTIVE) {
            return LIBRARY_RESULT_ALREADY_BORROWED;
        }
    }
    if (!ensure_array_capacity((void **)&repository->loans,
            &repository->loan_capacity, repository->loan_count,
            sizeof(*repository->loans))) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Loan loan;
    memset(&loan, 0, sizeof(loan));
    loan.id = next_loan_id(repository);
    loan.book_id = book_id;
    loan.reader_id = reader_id;
    loan.status = LOAN_STATUS_ACTIVE;
    if (!format_date(now, loan.borrow_date) ||
        !add_days_to_date(loan.borrow_date, LIBRARY_LOAN_DAYS, loan.due_date)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    repository->loans[repository->loan_count++] = loan;
    if (ready_reservation != NULL) {
        ready_reservation->status = RESERVATION_STATUS_FULFILLED;
    } else {
        --book->available_copies;
    }
    if (!save_all(repository)) {
        if (ready_reservation != NULL) {
            ready_reservation->status = RESERVATION_STATUS_READY;
        } else {
            ++book->available_copies;
        }
        --repository->loan_count;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    if (loan_id != NULL) {
        *loan_id = loan.id;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_borrow_book_at(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    time_t now,
    int *loan_id
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = borrow_book_at_unlocked(
        repository, readers, reader_id, book_id, now, loan_id
    );
    unlock_repository(repository);
    return result;
}

static LibraryResult return_loan_at_unlocked(
    LibraryRepository *repository,
    int loan_id,
    time_t now,
    int *fine_cents
) {
    if (repository == NULL) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Loan *loan = find_loan_mutable(repository, loan_id);
    if (loan == NULL) {
        return LIBRARY_RESULT_LOAN_NOT_FOUND;
    }
    if (loan->status == LOAN_STATUS_RETURNED) {
        return LIBRARY_RESULT_LOAN_RETURNED;
    }
    Book *book = find_book_mutable(repository, loan->book_id);
    if (book == NULL) {
        return LIBRARY_RESULT_BOOK_NOT_FOUND;
    }
    const int overdue_days = library_loan_overdue_days(loan, now);
    if (overdue_days > 0 && !ensure_array_capacity(
            (void **)&repository->fines,
            &repository->fine_capacity,
            repository->fine_count,
            sizeof(*repository->fines))) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const Loan old_loan = *loan;
    const int old_available = book->available_copies;
    loan->status = LOAN_STATUS_RETURNED;
    if (!format_date(now, loan->return_date)) {
        *loan = old_loan;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    if (book->available_copies < book->total_copies) {
        ++book->available_copies;
    }
    size_t activated_reservation = (size_t)-1;
    if (!activate_next_waiting(
            repository, book->id, now, &activated_reservation
        )) {
        *loan = old_loan;
        book->available_copies = old_available;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    if (overdue_days > 0) {
        Fine fine;
        memset(&fine, 0, sizeof(fine));
        fine.id = next_fine_id(repository);
        fine.reader_id = loan->reader_id;
        fine.loan_id = loan->id;
        fine.amount_cents = overdue_days * LIBRARY_FINE_PER_DAY_CENTS;
        fine.status = FINE_STATUS_UNPAID;
        if (!format_date(now, fine.created_at)) {
            *loan = old_loan;
            book->available_copies = old_available;
            return LIBRARY_RESULT_STORAGE_ERROR;
        }
        repository->fines[repository->fine_count++] = fine;
    }
    if (!save_all(repository)) {
        if (overdue_days > 0) {
            --repository->fine_count;
        }
        if (activated_reservation != (size_t)-1) {
            Reservation *reservation =
                &repository->reservations[activated_reservation];
            reservation->status = RESERVATION_STATUS_WAITING;
            reservation->expire_date[0] = '\0';
        }
        *loan = old_loan;
        book->available_copies = old_available;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    if (fine_cents != NULL) {
        *fine_cents = overdue_days * LIBRARY_FINE_PER_DAY_CENTS;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_return_loan_at(
    LibraryRepository *repository,
    int loan_id,
    time_t now,
    int *fine_cents
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = return_loan_at_unlocked(
        repository, loan_id, now, fine_cents
    );
    unlock_repository(repository);
    return result;
}

static LibraryResult renew_loan_at_unlocked(
    LibraryRepository *repository,
    int loan_id,
    time_t now
) {
    if (repository == NULL) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Loan *loan = find_loan_mutable(repository, loan_id);
    if (loan == NULL) {
        return LIBRARY_RESULT_LOAN_NOT_FOUND;
    }
    if (loan->status == LOAN_STATUS_RETURNED) {
        return LIBRARY_RESULT_LOAN_RETURNED;
    }
    if (library_loan_is_overdue(loan, now)) {
        return LIBRARY_RESULT_LOAN_OVERDUE;
    }
    if (loan->renew_count >= 1) {
        return LIBRARY_RESULT_RENEW_LIMIT;
    }
    for (size_t i = 0; i < repository->reservation_count; ++i) {
        if (repository->reservations[i].book_id == loan->book_id &&
            reservation_is_open(&repository->reservations[i])) {
            return LIBRARY_RESULT_RESERVATION_PENDING;
        }
    }
    const Loan old_loan = *loan;
    if (!add_days_to_date(
            loan->due_date, LIBRARY_RENEW_DAYS, loan->due_date
        )) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    ++loan->renew_count;
    if (!save_loans(repository)) {
        *loan = old_loan;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_renew_loan_at(
    LibraryRepository *repository,
    int loan_id,
    time_t now
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = renew_loan_at_unlocked(
        repository, loan_id, now
    );
    unlock_repository(repository);
    return result;
}

static LibraryResult pay_fine_at_unlocked(
    LibraryRepository *repository,
    int fine_id,
    time_t now
) {
    if (repository == NULL) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Fine *fine = find_fine_mutable(repository, fine_id);
    if (fine == NULL) {
        return LIBRARY_RESULT_FINE_NOT_FOUND;
    }
    if (fine->status == FINE_STATUS_PAID) {
        return LIBRARY_RESULT_FINE_PAID;
    }
    const Fine old_fine = *fine;
    fine->status = FINE_STATUS_PAID;
    if (!format_date(now, fine->paid_at) || !save_fines(repository)) {
        *fine = old_fine;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_pay_fine_at(
    LibraryRepository *repository,
    int fine_id,
    time_t now
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = pay_fine_at_unlocked(
        repository, fine_id, now
    );
    unlock_repository(repository);
    return result;
}

LibraryResult library_borrow_book(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    int *loan_id
) {
    return library_borrow_book_at(
        repository, readers, reader_id, book_id, time(NULL), loan_id
    );
}

LibraryResult library_return_loan(
    LibraryRepository *repository,
    int loan_id,
    int *fine_cents
) {
    return library_return_loan_at(repository, loan_id, time(NULL), fine_cents);
}

LibraryResult library_renew_loan(
    LibraryRepository *repository,
    int loan_id
) {
    return library_renew_loan_at(repository, loan_id, time(NULL));
}

LibraryResult library_pay_fine(
    LibraryRepository *repository,
    int fine_id
) {
    return library_pay_fine_at(repository, fine_id, time(NULL));
}

static LibraryResult mark_loan_overdue_for_demo_unlocked(
    LibraryRepository *repository,
    int loan_id,
    int overdue_days
) {
    if (repository == NULL || overdue_days <= 0) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Loan *loan = find_loan_mutable(repository, loan_id);
    if (loan == NULL) {
        return LIBRARY_RESULT_LOAN_NOT_FOUND;
    }
    if (loan->status == LOAN_STATUS_RETURNED) {
        return LIBRARY_RESULT_LOAN_RETURNED;
    }
    const Loan old_loan = *loan;
    const time_t simulated_due = time(NULL) - (time_t)overdue_days * 86400;
    if (!format_date(simulated_due, loan->due_date) ||
        !save_loans(repository)) {
        *loan = old_loan;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_mark_loan_overdue_for_demo(
    LibraryRepository *repository,
    int loan_id,
    int overdue_days
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = mark_loan_overdue_for_demo_unlocked(
        repository, loan_id, overdue_days
    );
    unlock_repository(repository);
    return result;
}

static LibraryResult add_book_unlocked(
    LibraryRepository *repository,
    const wchar_t *isbn,
    const wchar_t *title,
    const wchar_t *author,
    int copies,
    int *book_id
) {
    if (repository == NULL || isbn == NULL || title == NULL ||
        author == NULL || isbn[0] == L'\0' || title[0] == L'\0' ||
        author[0] == L'\0' || copies <= 0 || copies > 999 ||
        wcslen(isbn) >= BOOK_ISBN_CAPACITY ||
        wcslen(title) >= BOOK_TITLE_CAPACITY ||
        wcslen(author) >= BOOK_AUTHOR_CAPACITY) {
        return LIBRARY_RESULT_INVALID_INPUT;
    }
    for (size_t i = 0; i < repository->book_count; ++i) {
        if (wcscmp(repository->books[i].isbn, isbn) == 0) {
            return LIBRARY_RESULT_DUPLICATE_ISBN;
        }
    }
    if (!ensure_array_capacity(
            (void **)&repository->books,
            &repository->book_capacity,
            repository->book_count,
            sizeof(*repository->books)
        )) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Book book;
    memset(&book, 0, sizeof(book));
    book.id = next_book_id(repository);
    book.total_copies = copies;
    book.available_copies = copies;
    book.status = BOOK_STATUS_AVAILABLE;
    wcsncpy(book.isbn, isbn, BOOK_ISBN_CAPACITY - 1);
    wcsncpy(book.title, title, BOOK_TITLE_CAPACITY - 1);
    wcsncpy(book.author, author, BOOK_AUTHOR_CAPACITY - 1);
    if (!format_date(time(NULL), book.created_at)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    repository->books[repository->book_count++] = book;
    if (!save_books(repository)) {
        --repository->book_count;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    if (book_id != NULL) {
        *book_id = book.id;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_add_book(
    LibraryRepository *repository,
    const wchar_t *isbn,
    const wchar_t *title,
    const wchar_t *author,
    int copies,
    int *book_id
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = add_book_unlocked(
        repository, isbn, title, author, copies, book_id
    );
    unlock_repository(repository);
    return result;
}

static LibraryResult add_book_copy_unlocked(
    LibraryRepository *repository,
    int book_id
) {
    if (repository == NULL) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Book *book = find_book_mutable(repository, book_id);
    if (book == NULL) {
        return LIBRARY_RESULT_BOOK_NOT_FOUND;
    }
    const Book old_book = *book;
    ++book->total_copies;
    ++book->available_copies;
    size_t activated_reservation = (size_t)-1;
    if (!activate_next_waiting(
            repository, book_id, time(NULL), &activated_reservation
        ) || !save_all(repository)) {
        if (activated_reservation != (size_t)-1) {
            Reservation *reservation =
                &repository->reservations[activated_reservation];
            reservation->status = RESERVATION_STATUS_WAITING;
            reservation->expire_date[0] = '\0';
        }
        *book = old_book;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_add_book_copy(
    LibraryRepository *repository,
    int book_id
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = add_book_copy_unlocked(repository, book_id);
    unlock_repository(repository);
    return result;
}

static LibraryResult toggle_book_status_unlocked(
    LibraryRepository *repository,
    int book_id
) {
    if (repository == NULL) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Book *book = find_book_mutable(repository, book_id);
    if (book == NULL) {
        return LIBRARY_RESULT_BOOK_NOT_FOUND;
    }
    const BookStatus old_status = book->status;
    book->status = old_status == BOOK_STATUS_AVAILABLE
        ? BOOK_STATUS_SUSPENDED : BOOK_STATUS_AVAILABLE;
    if (!save_books(repository)) {
        book->status = old_status;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_toggle_book_status(
    LibraryRepository *repository,
    int book_id
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = toggle_book_status_unlocked(
        repository, book_id
    );
    unlock_repository(repository);
    return result;
}

static bool refresh_reservations_unlocked(
    LibraryRepository *repository,
    time_t now
) {
    if (repository == NULL) {
        return false;
    }
    bool changed = false;
    for (size_t i = 0; i < repository->reservation_count; ++i) {
        Reservation *reservation = &repository->reservations[i];
        if (reservation->status != RESERVATION_STATUS_READY) {
            continue;
        }
        const time_t expires = parse_end_of_date(reservation->expire_date);
        if (expires == (time_t)-1 || difftime(now, expires) <= 0.0) {
            continue;
        }
        Book *book = find_book_mutable(repository, reservation->book_id);
        if (book == NULL) {
            return false;
        }
        reservation->status = RESERVATION_STATUS_EXPIRED;
        if (book->available_copies < book->total_copies) {
            ++book->available_copies;
        }
        if (!activate_next_waiting(repository, book->id, now, NULL)) {
            return false;
        }
        changed = true;
    }
    return !changed || save_all(repository);
}

bool library_refresh_reservations_at(
    LibraryRepository *repository,
    time_t now
) {
    if (!begin_transaction(repository)) {
        return false;
    }
    const bool success = refresh_reservations_unlocked(repository, now);
    unlock_repository(repository);
    return success;
}

static LibraryResult create_reservation_at_unlocked(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    time_t now,
    int *reservation_id
) {
    if (repository == NULL ||
        !refresh_reservations_unlocked(repository, now)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    bool reader_normal = false;
    if (!reader_exists_and_normal(readers, reader_id, &reader_normal)) {
        return LIBRARY_RESULT_READER_NOT_FOUND;
    }
    if (!reader_normal) {
        return LIBRARY_RESULT_READER_BLOCKED;
    }
    const Book *book = library_find_book(repository, book_id);
    if (book == NULL) {
        return LIBRARY_RESULT_BOOK_NOT_FOUND;
    }
    if (book->status != BOOK_STATUS_AVAILABLE) {
        return LIBRARY_RESULT_BOOK_UNAVAILABLE;
    }
    if (book->available_copies > 0) {
        return LIBRARY_RESULT_BOOK_STILL_AVAILABLE;
    }
    for (size_t i = 0; i < repository->loan_count; ++i) {
        if (repository->loans[i].reader_id == reader_id &&
            repository->loans[i].book_id == book_id &&
            repository->loans[i].status == LOAN_STATUS_ACTIVE) {
            return LIBRARY_RESULT_ALREADY_BORROWED;
        }
    }
    for (size_t i = 0; i < repository->reservation_count; ++i) {
        const Reservation *reservation = &repository->reservations[i];
        if (reservation->reader_id == reader_id &&
            reservation->book_id == book_id &&
            reservation_is_open(reservation)) {
            return LIBRARY_RESULT_RESERVATION_DUPLICATE;
        }
    }
    if (!ensure_array_capacity(
            (void **)&repository->reservations,
            &repository->reservation_capacity,
            repository->reservation_count,
            sizeof(*repository->reservations)
        )) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Reservation reservation;
    memset(&reservation, 0, sizeof(reservation));
    reservation.id = next_reservation_id(repository);
    reservation.book_id = book_id;
    reservation.reader_id = reader_id;
    reservation.status = RESERVATION_STATUS_WAITING;
    if (!format_date(now, reservation.reserve_date)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    repository->reservations[repository->reservation_count++] = reservation;
    if (!save_reservations(repository)) {
        --repository->reservation_count;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    if (reservation_id != NULL) {
        *reservation_id = reservation.id;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_create_reservation_at(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    time_t now,
    int *reservation_id
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = create_reservation_at_unlocked(
        repository, readers, reader_id, book_id, now, reservation_id
    );
    unlock_repository(repository);
    return result;
}

static LibraryResult close_reservation(
    LibraryRepository *repository,
    int reservation_id,
    ReservationStatus closed_status,
    time_t now
) {
    if (repository == NULL ||
        !refresh_reservations_unlocked(repository, now)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    Reservation *reservation = find_reservation_mutable(
        repository, reservation_id
    );
    if (reservation == NULL) {
        return LIBRARY_RESULT_RESERVATION_NOT_FOUND;
    }
    if (!reservation_is_open(reservation)) {
        return LIBRARY_RESULT_RESERVATION_CLOSED;
    }
    if (closed_status == RESERVATION_STATUS_EXPIRED &&
        reservation->status != RESERVATION_STATUS_READY) {
        return LIBRARY_RESULT_RESERVATION_NOT_READY;
    }
    Book *book = find_book_mutable(repository, reservation->book_id);
    if (book == NULL) {
        return LIBRARY_RESULT_BOOK_NOT_FOUND;
    }
    const Reservation old_reservation = *reservation;
    const int old_available = book->available_copies;
    const bool released_copy = reservation->status == RESERVATION_STATUS_READY;
    reservation->status = closed_status;
    if (released_copy && book->available_copies < book->total_copies) {
        ++book->available_copies;
    }
    size_t activated_index = (size_t)-1;
    if (!activate_next_waiting(
            repository, book->id, now, &activated_index
        ) || !save_all(repository)) {
        if (activated_index != (size_t)-1) {
            Reservation *activated = &repository->reservations[activated_index];
            activated->status = RESERVATION_STATUS_WAITING;
            activated->expire_date[0] = '\0';
        }
        *reservation = old_reservation;
        book->available_copies = old_available;
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    return LIBRARY_RESULT_SUCCESS;
}

LibraryResult library_cancel_reservation_at(
    LibraryRepository *repository,
    int reservation_id,
    time_t now
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = close_reservation(
        repository,
        reservation_id,
        RESERVATION_STATUS_CANCELLED,
        now
    );
    unlock_repository(repository);
    return result;
}

LibraryResult library_expire_reservation_for_demo(
    LibraryRepository *repository,
    int reservation_id,
    time_t now
) {
    if (!begin_transaction(repository)) {
        return LIBRARY_RESULT_STORAGE_ERROR;
    }
    const LibraryResult result = close_reservation(
        repository,
        reservation_id,
        RESERVATION_STATUS_EXPIRED,
        now
    );
    unlock_repository(repository);
    return result;
}

LibraryResult library_create_reservation(
    LibraryRepository *repository,
    const ReaderRepository *readers,
    int reader_id,
    int book_id,
    int *reservation_id
) {
    return library_create_reservation_at(
        repository, readers, reader_id, book_id,
        time(NULL), reservation_id
    );
}

LibraryResult library_cancel_reservation(
    LibraryRepository *repository,
    int reservation_id
) {
    return library_cancel_reservation_at(
        repository, reservation_id, time(NULL)
    );
}
