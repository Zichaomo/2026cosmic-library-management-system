#include "ui_next/terminal.h"
#include <string.h>

static int previous_boundary(const Edit *e) {
    int pos = e->cursor;
    if (pos) --pos;
    while (pos && ((unsigned char)e->text[pos] & 0xc0u) == 0x80u) --pos;
    return pos;
}

static int insert(Edit *e,const char *text,int password) {
    int old = (int)strlen(e->text), n = (int)strlen(text);
    if (n<1 || old+n>=CL_EDIT_BYTES) return 0;
    for (const char *p=text;*p;) {
        unsigned cp;int bytes=utf8_decode(p,&cp);
        if (bytes<1 || (password?(cp<32 || (cp>=127 && cp<160)):(utf8_columns(cp)<0))) return 0;
        p+=bytes;
    }
    for (int i = old; i >= e->cursor; --i) e->text[i + n] = e->text[i];
    for (int i = 0; i < n; ++i) e->text[e->cursor + i] = text[i];
    e->cursor += n;
    return 1;
}
int edit_insert(Edit *e,const char *text) {return insert(e,text,0);}
int edit_insert_password(Edit *e,const char *text) {return insert(e,text,1);}

int edit_key(Edit *e, Key key) {
    int n = (int)strlen(e->text), end = e->cursor, start = end;
    unsigned cp;
    if (key == KEY_HOME) e->cursor = 0;
    else if (key == KEY_END) e->cursor = n;
    else if (key == KEY_LEFT && e->cursor) e->cursor = previous_boundary(e);
    else if (key == KEY_RIGHT && e->cursor < n)
        e->cursor += utf8_decode(e->text + e->cursor, &cp);
    else if (key == KEY_BACKSPACE && e->cursor) {
        start = previous_boundary(e);
        e->cursor = start;
    } else if (key == KEY_DELETE && e->cursor < n)
        end += utf8_decode(e->text + end, &cp);
    if (end > start) {
        for (int i = end; i <= n; ++i) e->text[start + i - end] = e->text[i];
        return 1;
    }
    return 0;
}

