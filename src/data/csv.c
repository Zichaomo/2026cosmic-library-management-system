#include "data/csv.h"

#include <string.h>

bool csv_write_field(FILE *file, const char *value) {
    if (file == NULL || value == NULL) {
        return false;
    }

    const bool needs_quotes = strpbrk(value, ",\"\r\n") != NULL;
    if (!needs_quotes) {
        return fputs(value, file) >= 0;
    }

    if (fputc('"', file) == EOF) {
        return false;
    }
    for (const char *cursor = value; *cursor != '\0'; ++cursor) {
        if (*cursor == '"' && fputc('"', file) == EOF) {
            return false;
        }
        if (fputc(*cursor, file) == EOF) {
            return false;
        }
    }
    return fputc('"', file) != EOF;
}

bool csv_parse_line(
    const char *line,
    char fields[][CSV_FIELD_CAPACITY],
    size_t maximum_fields,
    size_t *field_count
) {
    if (line == NULL || fields == NULL || maximum_fields == 0 ||
        field_count == NULL) {
        return false;
    }

    const char *cursor = line;
    size_t count = 0;
    while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n') {
        if (count >= maximum_fields) {
            return false;
        }

        size_t length = 0;
        bool quoted = false;
        if (*cursor == '"') {
            quoted = true;
            ++cursor;
        }

        while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n') {
            if (quoted) {
                if (*cursor == '"') {
                    if (cursor[1] == '"') {
                        if (length + 1 >= CSV_FIELD_CAPACITY) {
                            return false;
                        }
                        fields[count][length++] = '"';
                        cursor += 2;
                        continue;
                    }
                    ++cursor;
                    quoted = false;
                    continue;
                }
            } else if (*cursor == ',') {
                break;
            }

            if (length + 1 >= CSV_FIELD_CAPACITY) {
                return false;
            }
            fields[count][length++] = *cursor++;
        }

        if (quoted) {
            return false;
        }
        fields[count][length] = '\0';
        ++count;

        if (*cursor == ',') {
            ++cursor;
            if (*cursor == '\0' || *cursor == '\r' || *cursor == '\n') {
                if (count >= maximum_fields) {
                    return false;
                }
                fields[count++][0] = '\0';
            }
        }
    }

    *field_count = count;
    return true;
}

