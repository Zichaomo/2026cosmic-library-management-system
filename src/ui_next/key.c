#include "ui_next/terminal.h"
#include <conio.h>

static Key extended_key(int c) {
    switch (c) {
        case 15: return KEY_BACKTAB;
        case 59: return KEY_F1;
        case 63: return KEY_F5;
        case 71: return KEY_HOME;
        case 72: return KEY_UP;
        case 73: return KEY_PGUP;
        case 75: return KEY_LEFT;
        case 77: return KEY_RIGHT;
        case 79: return KEY_END;
        case 80: return KEY_DOWN;
        case 81: return KEY_PGDN;
        case 83: return KEY_DELETE;
        default: return KEY_NONE;
    }
}

Event cl_terminal_event(void) {
    Event event={KEY_NONE,{0}};
    int c=getch(), second=-1;
    if (c==0) { event.key=extended_key(getch()); return event; }
    /* Extended-key prefix 0xe0 is also a possible UTF-8 lead byte. */
    if (c==0xe0) {
        second=getch();
        if (second<0xa0 || second>0xbf) { event.key=extended_key(second); return event; }
    }
    switch (c) {
        case 9: event.key=KEY_TAB; return event;
        case 13: event.key=KEY_ENTER; return event;
        case 27: event.key=KEY_ESC; return event;
        case 8: event.key=KEY_BACKSPACE; return event;
        case 17: event.key=KEY_QUIT; return event;
        default: break;
    }
    if (c>=32 && c<127) { event.key=KEY_TEXT; event.text[0]=(char)c; return event; }
    int count=c>=0xc2&&c<=0xdf?2:(c>=0xe0&&c<=0xef?3:(c>=0xf0&&c<=0xf4?4:0));
    if (!count) return event;
    event.text[0]=(char)c;
    for (int i=1;i<count;++i) {
        int next=i==1 && second>=0?second:getch();
        if (next<0x80 || next>0xbf) {
            if (next==17) event.key=KEY_QUIT;
            else if (next==27) event.key=KEY_ESC;
            event.text[0]=0; return event;
        }
        event.text[i]=(char)next;
    }
    unsigned cp;
    /* The focused editor decides display support. Secret fields are masked
       and may accept supplementary/combining characters from legacy passwords. */
    if (utf8_decode(event.text,&cp)>0) event.key=KEY_TEXT;
    else event.text[0]=0;
    return event;
}

