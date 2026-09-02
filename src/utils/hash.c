/* 散列工具 - 通用工具层（课程级，非加密用途，PRD FR-105） */
#include "hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FNV_OFFSET 2166136261u
#define FNV_PRIME 16777619u

static const char HEX[] = "0123456789abcdef";

unsigned int hash_fnv1a(const unsigned char *data, size_t len) {
    unsigned int h = FNV_OFFSET;
    size_t i;
    for (i = 0; i < len; i++) {
        h ^= data[i];
        h *= FNV_PRIME;
    }
    return h;
}

void hash_random_salt(char *salt, size_t size) {
    static bool seeded = false;
    int i;
    if (!salt || size < 17) return;
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = true;
    }
    for (i = 0; i < 16; i++) salt[i] = HEX[rand() % 16];
    salt[16] = '\0';
}

void hash_password(const char *password, const char *salt, char *out, size_t size) {
    char buf[256];
    unsigned int h;
    int i;
    if (!out || size < 17) return;
    snprintf(buf, sizeof buf, "%s%s", salt ? salt : "", password ? password : "");
    h = hash_fnv1a((const unsigned char *)buf, strlen(buf));
    for (i = 0; i < 8; i++) out[i] = HEX[(h >> (i * 4)) & 0xF];
    out[8] = '\0';
}

bool hash_password_verify(const char *password, const char *salt, const char *expected_hex) {
    char h[17];
    if (!expected_hex) return false;
    hash_password(password, salt, h, sizeof h);
    return strcmp(h, expected_hex) == 0;
}
