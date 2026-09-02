/* 字符串工具 - 通用工具层 */
#include "string.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

char *str_trim(char *s) {
    char *end;
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

char *str_lower(char *s) {
    char *p;
    if (!s) return s;
    for (p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    return s;
}

/* 不区分大小写比较前 n 个字符 */
static bool ci_eq(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (a[i] == '\0' || b[i] == '\0') return a[i] == b[i];
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
}

bool str_contains_ci(const char *s, const char *sub) {
    const char *p;
    size_t n;
    if (!s || !sub) return false;
    n = strlen(sub);
    if (n == 0) return true;
    for (p = s; *p; p++) {
        if (ci_eq(p, sub, n)) return true;
    }
    return false;
}

int str_format(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    int n;
    if (!buf || size == 0 || !fmt) return 0;
    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

bool str_copy(char *dst, size_t size, const char *src) {
    size_t len;
    if (!dst || size == 0) return false;
    if (!src) src = "";
    len = strlen(src);
    if (len >= size) {
        memcpy(dst, src, size - 1);
        dst[size - 1] = '\0';
        return false; /* 截断 */
    }
    memcpy(dst, src, len + 1);
    return true;
}

bool str_empty(const char *s) {
    if (!s) return true;
    while (*s && isspace((unsigned char)*s)) s++;
    return *s == '\0';
}
