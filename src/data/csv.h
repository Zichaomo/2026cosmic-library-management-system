#ifndef LMS_DATA_CSV_H
#define LMS_DATA_CSV_H

#include "../types.h"

/* ============ CSV 解析（RFC 4180） ============ */

/* 一行解析结果：字段数组 */
typedef struct {
    char **fields;
    size_t count;
    size_t cap;
} CsvRow;

/* 整表解析结果（含表头行 rows[0]） */
typedef struct {
    CsvRow *rows;
    size_t count;
    size_t cap;
} CsvTable;

/* 解析 CSV 文本（支持引号转义、字段内含逗号/引号/换行）。
   成功返回 RES_OK，调用者须用 csv_table_free 释放。 */
Result csv_parse(const char *text, CsvTable *out);

/* 释放解析结果 */
void csv_table_free(CsvTable *t);

/* 将一行字段序列化为 CSV 行（自动按 RFC 4180 转义），写入 buf。
   返回 RES_OK；buf 不足返回 RES_ERR_INVALID。 */
Result csv_row_write(const char *const *fields, size_t count, char *buf, size_t size);

/* 将整表（含表头）序列化到 malloc 字符串（调用者 free，含结尾 \0）。 */
Result csv_table_write(const CsvTable *t, char **out);

#endif /* LMS_DATA_CSV_H */
