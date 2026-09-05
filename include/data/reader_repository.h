#ifndef COSMIC_LIBRARY_DATA_READER_REPOSITORY_H
#define COSMIC_LIBRARY_DATA_READER_REPOSITORY_H

#include <stdbool.h>
#include <stddef.h>
#include <windows.h>

#include "utils/hash.h"

enum {
    READER_NUMBER_CAPACITY = 32,
    READER_NAME_CAPACITY = 64,
    READER_CONTACT_CAPACITY = 64,
    READER_EMAIL_CAPACITY = 64,
    READER_CREATED_AT_CAPACITY = 20
};

typedef enum {
    READER_STATUS_NORMAL = 0,
    READER_STATUS_FROZEN,
    READER_STATUS_CANCELLED
} ReaderStatus;

typedef struct {
    int id;
    wchar_t reader_no[READER_NUMBER_CAPACITY];
    wchar_t name[READER_NAME_CAPACITY];
    char password_hash[PASSWORD_HASH_HEX_CAPACITY];
    char salt[PASSWORD_SALT_HEX_CAPACITY];
    wchar_t contact[READER_CONTACT_CAPACITY];
    wchar_t email[READER_EMAIL_CAPACITY];
    ReaderStatus status;
    char created_at[READER_CREATED_AT_CAPACITY];
} ReaderAccount;

typedef struct {
    ReaderAccount *items;
    size_t count;
    size_t capacity;
    wchar_t file_path[MAX_PATH];
} ReaderRepository;

bool reader_repository_init(
    ReaderRepository *repository,
    const wchar_t *data_directory
);
void reader_repository_dispose(ReaderRepository *repository);
const ReaderAccount *reader_repository_find_by_number(
    const ReaderRepository *repository,
    const wchar_t *reader_no
);
int reader_repository_next_id(const ReaderRepository *repository);
bool reader_repository_add(
    ReaderRepository *repository,
    const ReaderAccount *account
);
bool reader_repository_save(const ReaderRepository *repository);

#endif

