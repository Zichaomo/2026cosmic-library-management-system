#ifndef COSMIC_LIBRARY_UI_CIRCULATION_SCREEN_H
#define COSMIC_LIBRARY_UI_CIRCULATION_SCREEN_H

#include "data/library_repository.h"
#include "data/reader_repository.h"
#include "ui/console.h"

void circulation_screen_draw_books(
    const Console *console,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
);
void circulation_screen_draw_reader_loans(
    const Console *console,
    const LibraryRepository *library,
    int reader_id,
    int selection,
    const wchar_t *message
);
void circulation_screen_draw_reader_fines(
    const Console *console,
    const LibraryRepository *library,
    int reader_id,
    int selection,
    const wchar_t *message
);
void circulation_screen_draw_admin_loans(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
);
void circulation_screen_draw_admin_fines(
    const Console *console,
    const ReaderRepository *readers,
    const LibraryRepository *library,
    int selection,
    const wchar_t *message
);

#endif
