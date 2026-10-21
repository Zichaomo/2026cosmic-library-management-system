#ifndef COSMIC_FOUNDATION_H
#define COSMIC_FOUNDATION_H

#include <stdio.h>
#include "base_types.h"

/* Strings must be NUL-terminated. Capacities count bytes including NUL.
   Failure leaves output unchanged. Copy/clip need non-overlapping storage;
   append also supports appending the destination (or its suffix) to itself. */
int text_copy(char *out, int capacity, const char *in);
int text_append(char *out, int capacity, const char *in);
int text_clip(char *out, int capacity, const char *in, int columns);
int cl_int_parse(const char *text, int *value);
int cl_int_format(char *out, int capacity, int value);
int positive_number(const char *text, int *value);

/* The decoder returns bytes consumed, 0 at NUL, -1 for malformed UTF-8.
   Width supports the prototype's ordinary Chinese/ASCII scope, not graphemes.
   Unsupported/control codepoints have width -1. Offsets count bytes. */
int utf8_decode(const char *text, unsigned *codepoint);
int utf8_columns(unsigned codepoint);
int utf8_width(const char *text);
int utf8_boundary(const char *text, int offset);
int utf8_previous(const char *text, int offset, int *previous);
int utf8_next(const char *text, int offset, int *next);

int cl_hex_encode(char *out, int capacity, const u8 *bytes, int count);
int cl_hex_decode(u8 *out, int capacity, const char *hex);
int cl_u64_hex(char *out, int capacity, u64 value);

/* Gregorian civil dates: years 1..9999; serial day 0 is 0001-01-01.
   Date differences/addition use calendar days, independent of the CRT zone. */
typedef struct { int year, month, day; } ClDate;
typedef struct { ClDate date; int hour, minute, second; } ClDateTime;
int cl_date_valid(ClDate date);
int cl_date_parse(const char *text, ClDate *out);
int cl_date_format(char *out, int capacity, ClDate date);
int cl_date_serial(ClDate date, int *serial);
int cl_date_from_serial(int serial, ClDate *out);
int cl_date_add(ClDate date, int days, ClDate *out);
int cl_date_difference(ClDate later, ClDate earlier, int *days);
int cl_datetime_parse(const char *text, ClDateTime *out);
int cl_datetime_format(char *out, int capacity, ClDateTime value);
/* POSIX epoch seconds interpreted in fixed Beijing UTC+8, without DST.
   from_unix accepts -1 as an instant; now treats time() == -1 as failure. */
int cl_datetime_from_unix(long long seconds, ClDateTime *out);
int cl_datetime_to_unix(ClDateTime value, long long *seconds);
int cl_datetime_now(ClDateTime *out);

#endif
