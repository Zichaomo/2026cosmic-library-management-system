#ifndef COSMIC_LIBRARY_UTILS_TEXT_H
#define COSMIC_LIBRARY_UTILS_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

bool text_wide_to_utf8(
    const wchar_t *source,
    char *destination,
    size_t destination_size
);
bool text_utf8_to_wide(
    const char *source,
    wchar_t *destination,
    size_t destination_count
);

#endif

