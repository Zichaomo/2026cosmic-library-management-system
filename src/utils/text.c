#include "utils/text.h"

#include <limits.h>
#include <windows.h>

bool text_wide_to_utf8(
    const wchar_t *source,
    char *destination,
    size_t destination_size
) {
    if (source == NULL || destination == NULL ||
        destination_size == 0 || destination_size > INT_MAX) {
        return false;
    }

    const int result = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        source,
        -1,
        destination,
        (int)destination_size,
        NULL,
        NULL
    );
    return result > 0;
}

bool text_utf8_to_wide(
    const char *source,
    wchar_t *destination,
    size_t destination_count
) {
    if (source == NULL || destination == NULL ||
        destination_count == 0 || destination_count > INT_MAX) {
        return false;
    }

    const int result = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        source,
        -1,
        destination,
        (int)destination_count
    );
    return result > 0;
}

