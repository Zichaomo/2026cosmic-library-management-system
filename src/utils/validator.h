#ifndef LMS_UTILS_VALIDATOR_H
#define LMS_UTILS_VALIDATOR_H

#include "../types.h"

/* 非空且长度在 [min, max] 内（不含结尾 \0） */
bool valid_length(const char *s, int min, int max);

/* 是否全部为数字 */
bool valid_digits(const char *s);

/* 邮箱简单校验：含 @ 且 @ 前后非空 */
bool valid_email(const char *s);

/* 手机号：11 位数字且以 1 开头 */
bool valid_phone(const char *s);

/* 非负数字字符串（可含小数点），解析到 *out（可为 NULL） */
bool valid_number_str(const char *s, double *out);

/* 合法日期 "YYYY-MM-DD"（含大小月/闰年检查） */
bool valid_date_str(const char *s);

#endif /* LMS_UTILS_VALIDATOR_H */
