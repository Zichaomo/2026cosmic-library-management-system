#include "ui/text_edit.h"

#include <string.h>
#include <wchar.h>

static bool high_surrogate(wchar_t value) {
    return value >= 0xd800 && value <= 0xdbff;
}
static bool low_surrogate(wchar_t value) {
    return value >= 0xdc00 && value <= 0xdfff;
}
size_t text_edit_previous(const wchar_t *value, size_t position) {
    if (value == NULL || position == 0) return 0;
    --position;
    if (position > 0 && low_surrogate(value[position]) &&
        high_surrogate(value[position - 1])) --position;
    return position;
}
size_t text_edit_next(const wchar_t *value, size_t position) {
    if (value == NULL || value[position] == L'\0') return position;
    return position + (high_surrogate(value[position]) &&
        low_surrogate(value[position + 1]) ? 2 : 1);
}
bool text_edit_handle(wchar_t *value, size_t capacity, size_t *length,
    size_t *caret, const UiEvent *event) {
    if (value == NULL || length == NULL || caret == NULL ||
        event == NULL || event->type != UI_EVENT_KEY || capacity == 0) {
        return false;
    }
    if (*caret > *length) *caret = *length;
    if (*caret > 0 && *caret < *length &&
        low_surrogate(value[*caret]) && high_surrogate(value[*caret - 1])) {
        --*caret;
    }
    switch (event->key) {
        case UI_KEY_LEFT: *caret = text_edit_previous(value, *caret); break;
        case UI_KEY_RIGHT: *caret = text_edit_next(value, *caret); break;
        case UI_KEY_HOME: *caret = 0; break;
        case UI_KEY_END: *caret = *length; break;
        case UI_KEY_BACKSPACE:
            if (*caret > 0) {
                const size_t previous = text_edit_previous(value, *caret);
                memmove(value + previous, value + *caret,
                    (*length - *caret + 1) * sizeof(*value));
                *length -= *caret - previous;
                *caret = previous;
            }
            break;
        case UI_KEY_DELETE:
            if (*caret < *length) {
                const size_t next = text_edit_next(value, *caret);
                memmove(value + *caret, value + next,
                    (*length - next + 1) * sizeof(*value));
                *length -= next - *caret;
            }
            break;
        case UI_KEY_CHARACTER:
            if (*length + 1 >= capacity || event->character < L' ') return false;
            memmove(value + *caret + 1, value + *caret,
                (*length - *caret + 1) * sizeof(*value));
            value[(*caret)++] = event->character;
            ++*length;
            break;
        default: return false;
    }
    return true;
}
