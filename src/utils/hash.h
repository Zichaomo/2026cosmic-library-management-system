#ifndef LMS_UTILS_HASH_H
#define LMS_UTILS_HASH_H

#include "../types.h"

/* FNV-1a 32 位散列（课程级，非加密用途，见 PRD FR-105） */
unsigned int hash_fnv1a(const unsigned char *data, size_t len);

/* 生成随机盐（十六进制写入 salt，size >= 17） */
void hash_random_salt(char *salt, size_t size);

/* 密码散列 = fnv1a(salt + password)，十六进制写入 out（size >= 17） */
void hash_password(const char *password, const char *salt, char *out, size_t size);

/* 校验密码：比对散列十六进制是否一致 */
bool hash_password_verify(const char *password, const char *salt, const char *expected_hex);

#endif /* LMS_UTILS_HASH_H */
