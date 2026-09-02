/* 输入校验工具 - 通用工具层 */
#include "validator.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "date.h"

bool valid_length(const char *s, int min, int max) {
    size_t n;
    if (!s) return false;
    n = strlen(s);
    return n >= (size_t)min && n <= (size_t)max;
}

bool valid_digits(const char *s) {
    const char *p;
    if (!s || !*s) return false;
    for (p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return false;
    }
    return true;
}

bool valid_email(const char *s) {
    const char *at;
    if (!s) return false;
    at = strchr(s, '@');
    if (!at || at == s || at[1] == '\0') return false;
    return strchr(at + 1, '@') == NULL;
}

bool valid_phone(const char *s) {
    if (!s) return false;
    return strlen(s) == 11 && s[0] == '1' && valid_digits(s);
}

bool valid_number_str(const char *s, double *out) {
    char *end = NULL;
    double v;
    if (!s || !*s) return false;
    v = strtod(s, &end);
    if (!end || end == s || *end != '\0' || v < 0) return false;
    if (out) *out = v;
    return true;
}

bool valid_date_str(const char *s) {
    static const int md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    Date d;
    int max;
    if (!date_parse(s, &d)) return false;
    max = md[d.month - 1];
    if (d.month == 2 &&
        ((d.year % 4 == 0 && d.year % 100 != 0) || d.year % 400 == 0)) {
        max = 29;
    }
    return d.day <= max;
}
