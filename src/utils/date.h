#ifndef LMS_UTILS_DATE_H
#define LMS_UTILS_DATE_H

#include "../types.h"

/* 当前系统日期 */
Date date_today(void);

/* 解析 "YYYY-MM-DD"；失败返回 false */
bool date_parse(const char *s, Date *out);

/* 格式化为 "YYYY-MM-DD" 写入 buf（至少 11 字节） */
void date_format(const Date *d, char *buf, size_t size);

/* 天数差 b - a（可为负） */
long date_diff_days(const Date *a, const Date *b);

/* 日期加减天数 */
Date date_add_days(const Date *d, int days);

/* 当前时间戳 "YYYY-MM-DD HH:MM:SS" 写入 buf（至少 20 字节） */
void date_now_str(char *buf, size_t size);

#endif /* LMS_UTILS_DATE_H */
