/* CSV 解析/写出（RFC 4180）- 数据层 */
#include "csv.h"

#include <stdlib.h>
#include <string.h>

/* ---- 行内字段集合 ---- */
static void row_free(CsvRow *r) {
    size_t i;
    if (!r) return;
    for (i = 0; i < r->count; i++) free(r->fields[i]);
    free(r->fields);
    r->fields = NULL;
    r->count = r->cap = 0;
}

static Result row_push_field(CsvRow *r, const char *start, size_t len) {
    char *s;
    if (r->count == r->cap) {
        size_t nc = r->cap ? r->cap * 2 : 8;
        char **nf = realloc(r->fields, nc * sizeof(char *));
        if (!nf) return RES_ERR_NO_MEM;
        r->fields = nf;
        r->cap = nc;
    }
    s = malloc(len + 1);
    if (!s) return RES_ERR_NO_MEM;
    memcpy(s, start, len);
    s[len] = '\0';
    r->fields[r->count++] = s;
    return RES_OK;
}

/* ---- 拼接缓冲（当前字段） ---- */
static int buf_append(char **buf, size_t *len, size_t *cap, char c) {
    if (*len + 1 >= *cap) {
        size_t nc = *cap ? *cap * 2 : 64;
        char *nb = realloc(*buf, nc);
        if (!nb) return -1;
        *buf = nb;
        *cap = nc;
    }
    (*buf)[(*len)++] = c;
    return 0;
}

Result csv_parse(const char *text, CsvTable *out) {
    CsvRow cur = {0};
    char *buf = NULL;
    size_t blen = 0, bcap = 0;
    bool in_quotes = false;
    bool at_row_start = true; /* 当前行尚无任何内容（用于跳过空行） */
    const char *p;
    Result rc = RES_OK;

    if (!text || !out) return RES_ERR_INVALID;
    memset(out, 0, sizeof *out);

    for (p = text;;) {
        char c = *p;
        if (in_quotes) {
            if (c == '"') {
                if (p[1] == '"') { /* 转义引号 "" -> " */
                    if (buf_append(&buf, &blen, &bcap, '"') != 0) { rc = RES_ERR_NO_MEM; break; }
                    p += 2;
                    continue;
                }
                in_quotes = false;
                p++;
                continue;
            }
            if (c == '\0') break; /* 未闭合引号，容错结束 */
            if (buf_append(&buf, &blen, &bcap, c) != 0) { rc = RES_ERR_NO_MEM; break; }
            p++;
            continue;
        }
        if (c == '"' && blen == 0) { /* 字段以引号开始 */
            in_quotes = true;
            at_row_start = false;
            p++;
            continue;
        }
        if (c == ',' || c == '\n' || c == '\0') {
            if ((c == '\n' || c == '\0') && at_row_start && blen == 0) {
                /* 空行（含文件末尾换行）→ 跳过 */
                p++;
                if (c == '\0') break;
                continue;
            }
            if (buf_append(&buf, &blen, &bcap, '\0') != 0) { rc = RES_ERR_NO_MEM; break; }
            if (row_push_field(&cur, buf, blen - 1) != RES_OK) { rc = RES_ERR_NO_MEM; break; }
            blen = 0;
            at_row_start = false;
            if (c == '\n' || c == '\0') {
                if (cur.count > 0) { /* 提交整行 */
                    if (out->count == out->cap) {
                        size_t nc = out->cap ? out->cap * 2 : 16;
                        CsvRow *nr = realloc(out->rows, nc * sizeof(CsvRow));
                        if (!nr) { rc = RES_ERR_NO_MEM; break; }
                        out->rows = nr;
                        out->cap = nc;
                    }
                    out->rows[out->count++] = cur;
                    cur = (CsvRow){0};
                    at_row_start = true;
                }
                if (c == '\0') break;
            }
            p++;
            continue;
        }
        if (c == '\r') { p++; continue; } /* 忽略 CR */
        if (buf_append(&buf, &blen, &bcap, c) != 0) { rc = RES_ERR_NO_MEM; break; }
        at_row_start = false;
        p++;
    }

    free(buf);
    row_free(&cur);
    if (rc != RES_OK) csv_table_free(out);
    return rc;
}

void csv_table_free(CsvTable *t) {
    size_t i;
    if (!t) return;
    for (i = 0; i < t->count; i++) row_free(&t->rows[i]);
    free(t->rows);
    t->rows = NULL;
    t->count = t->cap = 0;
}

Result csv_row_write(const char *const *fields, size_t count, char *buf, size_t size) {
    size_t i, pos = 0;
    if (!buf || size == 0) return RES_ERR_INVALID;
    for (i = 0; i < count; i++) {
        const char *f = fields[i] ? fields[i] : "";
        bool need_quote = strchr(f, ',') || strchr(f, '"') || strchr(f, '\n') || strchr(f, '\r');
        const char *q;
        if (i > 0) {
            if (pos + 1 >= size) return RES_ERR_INVALID;
            buf[pos++] = ',';
        }
        if (!need_quote) {
            size_t fl = strlen(f);
            if (pos + fl >= size) return RES_ERR_INVALID;
            memcpy(buf + pos, f, fl);
            pos += fl;
            continue;
        }
        if (pos + 1 >= size) return RES_ERR_INVALID;
        buf[pos++] = '"';
        for (q = f; *q; q++) {
            if (*q == '"') {
                if (pos + 2 >= size) return RES_ERR_INVALID;
                buf[pos++] = '"';
                buf[pos++] = '"';
            } else {
                if (pos + 1 >= size) return RES_ERR_INVALID;
                buf[pos++] = *q;
            }
        }
        if (pos + 1 >= size) return RES_ERR_INVALID;
        buf[pos++] = '"';
    }
    buf[pos] = '\0';
    return RES_OK;
}

Result csv_table_write(const CsvTable *t, char **out) {
    static const size_t LINE_CAP = 65536; /* 单行上限 */
    char *buf = NULL, *line = NULL;
    size_t len = 0, cap = 0;
    size_t i;
    Result rc = RES_OK;

    if (!t || !out) return RES_ERR_INVALID;
    *out = NULL;
    line = malloc(LINE_CAP);
    if (!line) return RES_ERR_NO_MEM;

    for (i = 0; i < t->count; i++) {
        size_t ll;
        if (csv_row_write((const char *const *)t->rows[i].fields,
                          t->rows[i].count, line, LINE_CAP) != RES_OK) {
            rc = RES_ERR_INVALID;
            goto done;
        }
        ll = strlen(line);
        if (len + ll + 2 > cap) {
            size_t nc = cap ? cap * 2 : 4096;
            char *nb;
            while (nc < len + ll + 2) nc *= 2;
            nb = realloc(buf, nc);
            if (!nb) { rc = RES_ERR_NO_MEM; goto done; }
            buf = nb;
            cap = nc;
        }
        memcpy(buf + len, line, ll);
        len += ll;
        buf[len++] = '\n';
    }
    if (len + 1 > cap) {
        char *nb = realloc(buf, len + 1);
        if (!nb) { rc = RES_ERR_NO_MEM; goto done; }
        buf = nb;
        cap = len + 1;
    }
    if (buf) buf[len] = '\0';
    rc = RES_OK;

done:
    free(line);
    if (rc != RES_OK) {
        free(buf);
        return rc;
    }
    if (!buf) { /* 空表也返回一个空串 */
        buf = malloc(1);
        if (!buf) return RES_ERR_NO_MEM;
        buf[0] = '\0';
    }
    *out = buf;
    return RES_OK;
}
