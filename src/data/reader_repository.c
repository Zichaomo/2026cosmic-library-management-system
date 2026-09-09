#include "data/reader_repository.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "data/csv.h"
#include "utils/text.h"

enum {
    INITIAL_READER_CAPACITY = 16,
    READER_CSV_FIELD_COUNT = 9,
    UTF8_BUFFER_CAPACITY = 512
};

static const wchar_t *status_to_text(ReaderStatus status) {
    switch (status) {
        case READER_STATUS_NORMAL:
            return L"正常";
        case READER_STATUS_FROZEN:
            return L"冻结";
        case READER_STATUS_CANCELLED:
            return L"注销";
    }
    return L"注销";
}

static bool text_to_status(const char *text, ReaderStatus *status) {
    wchar_t wide_status[8];
    if (!text_utf8_to_wide(
            text, wide_status, sizeof(wide_status) / sizeof(wide_status[0])
        )) {
        return false;
    }
    if (wcscmp(wide_status, L"正常") == 0) {
        *status = READER_STATUS_NORMAL;
    } else if (wcscmp(wide_status, L"冻结") == 0) {
        *status = READER_STATUS_FROZEN;
    } else if (wcscmp(wide_status, L"注销") == 0) {
        *status = READER_STATUS_CANCELLED;
    } else {
        return false;
    }
    return true;
}

static bool ensure_capacity(ReaderRepository *repository) {
    if (repository->count < repository->capacity) {
        return true;
    }

    const size_t new_capacity = repository->capacity == 0
        ? INITIAL_READER_CAPACITY
        : repository->capacity * 2;
    ReaderAccount *new_items = realloc(
        repository->items, new_capacity * sizeof(*new_items)
    );
    if (new_items == NULL) {
        return false;
    }
    repository->items = new_items;
    repository->capacity = new_capacity;
    return true;
}

static bool parse_account(
    char fields[][CSV_FIELD_CAPACITY],
    ReaderAccount *account
) {
    char *end = NULL;
    const long id = strtol(fields[0], &end, 10);
    if (end == fields[0] || *end != '\0' || id <= 0 ||
        strlen(fields[3]) != PASSWORD_HASH_HEX_CAPACITY - 1 ||
        strlen(fields[4]) != PASSWORD_SALT_HEX_CAPACITY - 1) {
        return false;
    }

    memset(account, 0, sizeof(*account));
    account->id = (int)id;
    if (!text_utf8_to_wide(
            fields[1], account->reader_no, READER_NUMBER_CAPACITY
        ) ||
        !text_utf8_to_wide(
            fields[2], account->name, READER_NAME_CAPACITY
        ) ||
        !text_utf8_to_wide(
            fields[5], account->contact, READER_CONTACT_CAPACITY
        ) ||
        !text_utf8_to_wide(
            fields[6], account->email, READER_EMAIL_CAPACITY
        ) ||
        !text_to_status(fields[7], &account->status)) {
        return false;
    }

    memcpy(
        account->password_hash,
        fields[3],
        PASSWORD_HASH_HEX_CAPACITY
    );
    memcpy(account->salt, fields[4], PASSWORD_SALT_HEX_CAPACITY);
    strncpy(
        account->created_at,
        fields[8],
        READER_CREATED_AT_CAPACITY - 1
    );
    account->created_at[READER_CREATED_AT_CAPACITY - 1] = '\0';
    return true;
}

static bool load_accounts(ReaderRepository *repository) {
    FILE *file = _wfopen(repository->file_path, L"rb");
    if (file == NULL) {
        return errno == ENOENT;
    }

    char line[4096];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return true;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char fields[READER_CSV_FIELD_COUNT][CSV_FIELD_CAPACITY];
        size_t field_count = 0;
        ReaderAccount account;
        if (!csv_parse_line(
                line, fields, READER_CSV_FIELD_COUNT, &field_count
            ) || field_count != READER_CSV_FIELD_COUNT ||
            !parse_account(fields, &account) ||
            !ensure_capacity(repository)) {
            fclose(file);
            return false;
        }
        repository->items[repository->count++] = account;
    }

    const bool success = !ferror(file);
    fclose(file);
    return success;
}

static bool write_field(FILE *file, const char *field, bool last) {
    return csv_write_field(file, field) &&
           (last ? fputs("\r\n", file) >= 0 : fputc(',', file) != EOF);
}

static bool write_account(FILE *file, const ReaderAccount *account) {
    char id[32];
    char reader_no[UTF8_BUFFER_CAPACITY];
    char name[UTF8_BUFFER_CAPACITY];
    char contact[UTF8_BUFFER_CAPACITY];
    char email[UTF8_BUFFER_CAPACITY];
    char status[UTF8_BUFFER_CAPACITY];

    snprintf(id, sizeof(id), "%d", account->id);
    if (!text_wide_to_utf8(
            account->reader_no, reader_no, sizeof(reader_no)
        ) ||
        !text_wide_to_utf8(account->name, name, sizeof(name)) ||
        !text_wide_to_utf8(
            account->contact, contact, sizeof(contact)
        ) ||
        !text_wide_to_utf8(account->email, email, sizeof(email)) ||
        !text_wide_to_utf8(
            status_to_text(account->status), status, sizeof(status)
        )) {
        return false;
    }

    return write_field(file, id, false) &&
           write_field(file, reader_no, false) &&
           write_field(file, name, false) &&
           write_field(file, account->password_hash, false) &&
           write_field(file, account->salt, false) &&
           write_field(file, contact, false) &&
           write_field(file, email, false) &&
           write_field(file, status, false) &&
           write_field(file, account->created_at, true);
}

bool reader_repository_init(
    ReaderRepository *repository,
    const wchar_t *data_directory
) {
    if (repository == NULL || data_directory == NULL ||
        data_directory[0] == L'\0') {
        return false;
    }

    memset(repository, 0, sizeof(*repository));
    if (!CreateDirectoryW(data_directory, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    if (swprintf(
            repository->file_path,
            MAX_PATH,
            L"%ls\\readers.csv",
            data_directory
        ) < 0) {
        return false;
    }
    return load_accounts(repository);
}

void reader_repository_dispose(ReaderRepository *repository) {
    if (repository == NULL) {
        return;
    }
    free(repository->items);
    memset(repository, 0, sizeof(*repository));
}

const ReaderAccount *reader_repository_find_by_number(
    const ReaderRepository *repository,
    const wchar_t *reader_no
) {
    if (repository == NULL || reader_no == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < repository->count; ++index) {
        if (wcscmp(repository->items[index].reader_no, reader_no) == 0) {
            return &repository->items[index];
        }
    }
    return NULL;
}

const ReaderAccount *reader_repository_find_by_id(
    const ReaderRepository *repository,
    int reader_id
) {
    if (repository == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < repository->count; ++index) {
        if (repository->items[index].id == reader_id) {
            return &repository->items[index];
        }
    }
    return NULL;
}

int reader_repository_next_id(const ReaderRepository *repository) {
    int next_id = 1;
    if (repository == NULL) {
        return next_id;
    }
    for (size_t index = 0; index < repository->count; ++index) {
        if (repository->items[index].id >= next_id) {
            next_id = repository->items[index].id + 1;
        }
    }
    return next_id;
}

bool reader_repository_add(
    ReaderRepository *repository,
    const ReaderAccount *account
) {
    if (repository == NULL || account == NULL ||
        reader_repository_find_by_number(
            repository, account->reader_no
        ) != NULL || !ensure_capacity(repository)) {
        return false;
    }

    repository->items[repository->count++] = *account;
    if (!reader_repository_save(repository)) {
        --repository->count;
        return false;
    }
    return true;
}

bool reader_repository_save(const ReaderRepository *repository) {
    if (repository == NULL) {
        return false;
    }

    wchar_t temporary_path[MAX_PATH];
    if (swprintf(
            temporary_path,
            MAX_PATH,
            L"%ls.tmp",
            repository->file_path
        ) < 0) {
        return false;
    }

    FILE *file = _wfopen(temporary_path, L"wb");
    if (file == NULL) {
        return false;
    }

    static const unsigned char UTF8_BOM[] = {0xEF, 0xBB, 0xBF};
    bool success = fwrite(UTF8_BOM, 1, sizeof(UTF8_BOM), file) ==
                   sizeof(UTF8_BOM) &&
                   fputs(
                       "id,reader_no,name,password_hash,salt,phone,email,"
                       "status,created_at\r\n",
                       file
                   ) >= 0;
    for (size_t index = 0; success && index < repository->count; ++index) {
        success = write_account(file, &repository->items[index]);
    }
    if (fflush(file) != 0 || fclose(file) != 0) {
        success = false;
    }

    if (!success || !MoveFileExW(
            temporary_path,
            repository->file_path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        DeleteFileW(temporary_path);
        return false;
    }
    return true;
}

bool reader_repository_toggle_frozen(
    ReaderRepository *repository,
    int reader_id
) {
    if (repository == NULL) {
        return false;
    }
    for (size_t index = 0; index < repository->count; ++index) {
        ReaderAccount *reader = &repository->items[index];
        if (reader->id != reader_id ||
            reader->status == READER_STATUS_CANCELLED) {
            continue;
        }
        const ReaderStatus old_status = reader->status;
        reader->status = old_status == READER_STATUS_FROZEN
            ? READER_STATUS_NORMAL : READER_STATUS_FROZEN;
        if (!reader_repository_save(repository)) {
            reader->status = old_status;
            return false;
        }
        return true;
    }
    return false;
}
