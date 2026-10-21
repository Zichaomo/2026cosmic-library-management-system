#ifndef COSMIC_LIBRARY_UTILS_HASH_H
#define COSMIC_LIBRARY_UTILS_HASH_H

#include <stdbool.h>
#include <wchar.h>

enum {
    PASSWORD_SALT_HEX_CAPACITY = 33,
    PASSWORD_HASH_HEX_CAPACITY = 17
};

bool password_hash_create(
    const wchar_t *password,
    char salt_hex[PASSWORD_SALT_HEX_CAPACITY],
    char hash_hex[PASSWORD_HASH_HEX_CAPACITY]
);
bool password_hash_verify(
    const wchar_t *password,
    const char *salt_hex,
    const char *expected_hash_hex
);

#endif

