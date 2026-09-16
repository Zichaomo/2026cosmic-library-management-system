#include "utils/hash.h"

#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils/text.h"

enum { SALT_BYTE_COUNT = 16, PASSWORD_UTF8_CAPACITY = 256 };

static uint64_t fnv1a_update(
    uint64_t hash,
    const unsigned char *data,
    size_t length
) {
    static const uint64_t FNV_PRIME = UINT64_C(1099511628211);
    for (size_t index = 0; index < length; ++index) {
        hash ^= data[index];
        hash *= FNV_PRIME;
    }
    return hash;
}

static bool hex_to_bytes(
    const char *hex,
    unsigned char bytes[SALT_BYTE_COUNT]
) {
    if (hex == NULL || strlen(hex) != SALT_BYTE_COUNT * 2) {
        return false;
    }

    for (size_t index = 0; index < SALT_BYTE_COUNT; ++index) {
        unsigned int value = 0;
        if (sscanf(hex + index * 2, "%2x", &value) != 1) {
            return false;
        }
        bytes[index] = (unsigned char)value;
    }
    return true;
}

static bool compute_hash(
    const wchar_t *password,
    const unsigned char salt[SALT_BYTE_COUNT],
    char hash_hex[PASSWORD_HASH_HEX_CAPACITY]
) {
    char password_utf8[PASSWORD_UTF8_CAPACITY];
    if (!text_wide_to_utf8(
            password, password_utf8, sizeof(password_utf8)
        )) {
        return false;
    }

    uint64_t hash = UINT64_C(14695981039346656037);
    hash = fnv1a_update(hash, salt, SALT_BYTE_COUNT);
    hash = fnv1a_update(
        hash,
        (const unsigned char *)password_utf8,
        strlen(password_utf8)
    );
    return snprintf(
        hash_hex,
        PASSWORD_HASH_HEX_CAPACITY,
        "%016llx",
        (unsigned long long)hash
    ) == PASSWORD_HASH_HEX_CAPACITY - 1;
}

bool password_hash_create(
    const wchar_t *password,
    char salt_hex[PASSWORD_SALT_HEX_CAPACITY],
    char hash_hex[PASSWORD_HASH_HEX_CAPACITY]
) {
    unsigned char salt[SALT_BYTE_COUNT];
    if (password == NULL || salt_hex == NULL || hash_hex == NULL ||
        BCryptGenRandom(
            NULL,
            salt,
            SALT_BYTE_COUNT,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG
        ) != 0) {
        return false;
    }

    for (size_t index = 0; index < SALT_BYTE_COUNT; ++index) {
        snprintf(salt_hex + index * 2, 3, "%02x", salt[index]);
    }
    salt_hex[PASSWORD_SALT_HEX_CAPACITY - 1] = '\0';
    return compute_hash(password, salt, hash_hex);
}

bool password_hash_verify(
    const wchar_t *password,
    const char *salt_hex,
    const char *expected_hash_hex
) {
    unsigned char salt[SALT_BYTE_COUNT];
    char actual_hash[PASSWORD_HASH_HEX_CAPACITY];

    return password != NULL && expected_hash_hex != NULL &&
           hex_to_bytes(salt_hex, salt) &&
           compute_hash(password, salt, actual_hash) &&
           strcmp(actual_hash, expected_hash_hex) == 0;
}
