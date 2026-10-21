#include "preview.h"
#include <string.h>

int main(int argc,char **argv) {
    static Preview preview;
    Page page=PAGE_LOGIN;
    int print_only=0,text_only=0;
    for (int i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--print")) print_only=1;
        else if (!strcmp(argv[i],"--text")) text_only=1;
        else if (!strcmp(argv[i],"books")) page=PAGE_BOOKS;
        else if (!strcmp(argv[i],"form")) page=PAGE_FORM;
        else if (!strcmp(argv[i],"login")) page=PAGE_LOGIN;
        else {
            fputs("Usage: ui_preview.exe [login|books|form] [--print|--text]\n",stderr);
            return 2;
        }
    }
    preview_init(&preview,page);
    preview_scene(&preview);
    if (text_only) return preview_dump_text(stdout)?0:1;
    if (print_only) {
        int ok=preview_draw(stdout,1);
        preview_terminal_reset(stdout);
        return ok?0:1;
    }
    /* stderr is unbuffered on the target Windows CRT; interactive mode must
       keep stderr connected to the terminal. No stdout flush is needed. */
    fputs("\033[?1049h",stderr);
    if (!preview_draw(stderr,1)) {
        preview_terminal_reset(stderr);
        fputs("\033[?1049l",stderr);
        return 1;
    }
    while (preview.running) {
        Event e=cl_terminal_event();
        if (e.key==KEY_NONE) continue;
        preview_event(&preview,e);
        if (!preview.running) break;
        preview_scene(&preview);
        /* Resizing can invalidate the terminal even when model cells match.
           Explicit refresh must bypass the ordinary dirty-row comparison. */
        if (!preview_draw(stderr,e.key==KEY_F5?1:2)) {
            preview_terminal_reset(stderr);
            fputs("\033[?1049l",stderr);
            return 1;
        }
    }
    preview_terminal_reset(stderr);
    fputs("\033[?1049l",stderr);
    return 0;
}
