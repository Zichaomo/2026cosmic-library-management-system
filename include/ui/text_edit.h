#ifndef COSMIC_LIBRARY_UI_TEXT_EDIT_H
#define COSMIC_LIBRARY_UI_TEXT_EDIT_H

#include <stddef.h>
#include "ui/input.h"

/* Carets are insertion positions measured in UTF-16 code units. */
size_t text_edit_previous(const wchar_t *value, size_t position);
size_t text_edit_next(const wchar_t *value, size_t position);
bool text_edit_handle(wchar_t *value, size_t capacity, size_t *length,
    size_t *caret, const UiEvent *event);

#endif
