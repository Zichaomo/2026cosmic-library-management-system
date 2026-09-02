#ifndef LMS_UTILS_STRING_H
#define LMS_UTILS_STRING_H

#include "../types.h"

/* 去除首尾空白字符（原地修改，返回 s） */
char *str_trim(char *s);

/* 转小写（原地修改，返回 s） */
char *str_lower(char *s);

/* 判断 s 是否包含子串 sub（不区分大小写） */
bool str_contains_ci(const char *s, const char *sub);

/* snprintf 封装，返回实际应写入长度（不含结尾 \0） */
int str_format(char *buf, size_t size, const char *fmt, ...);

/* 复制到定长字段并保证 \0 结尾；src 过长时截断并返回 false */
bool str_copy(char *dst, size_t size, const char *src);

/* 是否为空串（NULL 或全空白） */
bool str_empty(const char *s);

#endif /* LMS_UTILS_STRING_H */
