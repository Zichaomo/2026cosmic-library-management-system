/* 日期时间工具 - 通用工具层 */
#include "date.h"

#include <stdio.h>
#include <time.h>

/* ---- 儒略日数算法（Howard Hinnant）---- */
static long date_to_days(const Date *d) {
    int y = d->year;
    int m = d->month;
    int dd = d->day;
    long era;
    unsigned yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)dd - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

static Date days_to_date(long z) {
    long era;
    unsigned doe, yoe, doy, mp;
    long y;
    unsigned m, dd;
    Date out;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = (long)yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    dd = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    out.year = (int)(y + (m <= 2));
    out.month = (int)m;
    out.day = (int)dd;
    return out;
}

Date date_today(void) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    Date d = {1970, 1, 1};
    if (lt) {
        d.year = lt->tm_year + 1900;
        d.month = lt->tm_mon + 1;
        d.day = lt->tm_mday;
    }
    return d;
}

bool date_parse(const char *s, Date *out) {
    int y, m, d;
    if (!s || !out) return false;
    if (sscanf(s, "%d-%d-%d", &y, &m, &d) != 3) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    out->year = y;
    out->month = m;
    out->day = d;
    return true;
}

void date_format(const Date *d, char *buf, size_t size) {
    if (!d || !buf || size < 11) return;
    snprintf(buf, size, "%04d-%02d-%02d", d->year, d->month, d->day);
}

long date_diff_days(const Date *a, const Date *b) {
    if (!a || !b) return 0;
    return date_to_days(b) - date_to_days(a);
}

Date date_add_days(const Date *d, int days) {
    Date zero = {1970, 1, 1};
    if (!d) return zero;
    return days_to_date(date_to_days(d) + days);
}

void date_now_str(char *buf, size_t size) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (!buf || size < 20) return;
    if (lt) {
        snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d",
                 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                 lt->tm_hour, lt->tm_min, lt->tm_sec);
    } else {
        snprintf(buf, size, "1970-01-01 00:00:00");
    }
}
