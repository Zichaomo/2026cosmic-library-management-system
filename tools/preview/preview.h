#ifndef COSMIC_PREVIEW_H
#define COSMIC_PREVIEW_H

#include <stdio.h>
#include "ui_next/terminal.h"

/* Fixed character canvas; physical aspect ratio depends on terminal font cells. */
enum { PREVIEW_W = CL_SCREEN_W, PREVIEW_H = CL_SCREEN_H, TEXT_CAP = CL_EDIT_BYTES,
       MAX_BOOKS = 32, PAGE_ROWS = 8 };
typedef enum { PAGE_LOGIN, PAGE_BOOKS, PAGE_FORM } Page;
typedef struct {
    char isbn[64], title[TEXT_CAP], author[TEXT_CAP];
    int copies, available, suspended;
} PreviewBook;
typedef struct {
    Page page;
    Edit login[2], form[4], search;
    PreviewBook books[MAX_BOOKS];
    int count, matches[MAX_BOOKS], match_count, selected;
    int focus, searching, running, help, detail, dirty_form;
    int confirm_cancel;
    char message[TEXT_CAP];
} Preview;

void preview_init(Preview *p, Page page);
void preview_filter(Preview *p);
void preview_event(Preview *p, Event event);
void preview_scene(const Preview *p);
#define preview_draw cl_terminal_draw
#define preview_dump_text cl_terminal_text
#define preview_terminal_reset cl_terminal_reset

#endif
