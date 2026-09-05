#ifndef COSMIC_LIBRARY_DATA_CSV_H
#define COSMIC_LIBRARY_DATA_CSV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

enum { CSV_FIELD_CAPACITY = 512 };

bool csv_write_field(FILE *file, const char *value);
bool csv_parse_line(
    const char *line,
    char fields[][CSV_FIELD_CAPACITY],
    size_t maximum_fields,
    size_t *field_count
);

#endif

