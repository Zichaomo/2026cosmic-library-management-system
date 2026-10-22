#ifndef COSMIC_NEXT_TERMINAL_H
#define COSMIC_NEXT_TERMINAL_H
#include "utils/foundation.h"
enum { CL_SCREEN_W=120,CL_SCREEN_H=35,CL_EDIT_BYTES=512 };
enum { BG, PAPER, STONE, INK, TEXT, MUTED, BORDER, ACCENT, SELECTED,
       PRIMARY, PRIMARY_TEXT, FOCUS, ERROR_COLOR, COLOR_COUNT };
typedef enum { KEY_NONE, KEY_TEXT, KEY_TAB, KEY_BACKTAB, KEY_ENTER,
    KEY_ESC, KEY_BACKSPACE, KEY_DELETE, KEY_LEFT, KEY_RIGHT, KEY_HOME,
    KEY_END, KEY_UP, KEY_DOWN, KEY_PGUP, KEY_PGDN, KEY_F1, KEY_F5,
    KEY_QUIT } Key;
typedef struct { char text[CL_EDIT_BYTES]; int cursor; } Edit;
typedef struct { Key key; char text[5]; } Event;
int edit_insert(Edit *edit,const char *text);
int edit_insert_password(Edit *edit,const char *text);
int edit_key(Edit *edit,Key key);
Event cl_terminal_event(void);
void cl_ui_fill(int x,int y,int w,int h,int background);
void cl_ui_put(int x,int y,const char *text,int foreground,int columns);
void cl_ui_clipped(int x,int y,const char *text,int foreground,int columns);
void cl_ui_integer(int x,int y,int value,int foreground);
void cl_ui_line(int x,int y,int width);
void cl_ui_box(int x,int y,int width,int height,const char *label);
void cl_ui_input(int x,int y,int width,const Edit *edit,int active,int secret);
void cl_ui_button(int x,int y,const char *label,int focused);
void cl_ui_scrollbar(int x,int y,int height,int selected,int count,int page_size);
/* force: 0 dirty rows, 1 clear + full redraw, 2 full repaint without clearing.
   Interactive repaint repairs terminal damage after resizing on the next key. */
int cl_terminal_draw(FILE *stream,int force);
/* Numeric diagnostics only; never includes rendered text or user data. */
void cl_terminal_report(FILE *stream);
int cl_terminal_text(FILE *stream);
void cl_terminal_reset(FILE *stream);
#endif
