#include "ui_next/terminal.h"
#include <string.h>

typedef struct { unsigned char r, g, b; } Color;
static const Color colors[COLOR_COUNT] = {
    {247,246,242}, {255,255,255}, {241,238,232}, {53,58,50},
    {78,78,78}, {113,107,99}, {216,208,199}, {72,97,79},
    {233,238,231}, {220,229,217}, {52,76,61}, {111,133,111}, {163,75,59}
};
typedef struct { char glyph[5]; int fg, bg; } Cell;
static Cell cells[CL_SCREEN_H][CL_SCREEN_W], previous[CL_SCREEN_H][CL_SCREEN_W];
static int previous_valid;
static char output[200000];
static int output_length, output_ok;

static void add(const char *s) {
    for (; *s; ++s) {
        if (output_length + 1 >= (int)sizeof(output)) { output_ok = 0; return; }
        output[output_length++] = *s;
    }
    output[output_length] = 0;
}

static void number(int value) {
    char digits[16]; int n = 0;
    do { digits[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (n) { char one[2] = { digits[--n], 0 }; add(one); }
}

static void position(int x, int y) {
    add("\033["); number(y + 1); add(";"); number(x + 1); add("H");
}

static void color(int fg, int bg) {
    add("\033[38;2;"); number(colors[fg].r); add(";"); number(colors[fg].g);
    add(";"); number(colors[fg].b); add(";48;2;"); number(colors[bg].r);
    add(";"); number(colors[bg].g); add(";"); number(colors[bg].b); add("m");
}

/* Covering either half of a wide glyph must erase the entire old glyph.
   Otherwise an overlay leaves a lead byte cell whose continuation became a
   border: serializing both would advance the terminal one column too far. */
static void erase_glyph(int x, int y) {
    unsigned cp;
    if (x<0 || x>=CL_SCREEN_W || y<0 || y>=CL_SCREEN_H) return;
    if (!cells[y][x].glyph[0] && x>0 &&
        utf8_decode(cells[y][x-1].glyph,&cp)>0 && utf8_columns(cp)==2) {
        cells[y][x-1].glyph[0]=' ';cells[y][x-1].glyph[1]=0;
    } else if (utf8_decode(cells[y][x].glyph,&cp)>0 &&
               utf8_columns(cp)==2 && x+1<CL_SCREEN_W) {
        cells[y][x+1].glyph[0]=' ';cells[y][x+1].glyph[1]=0;
    }
    cells[y][x].glyph[0]=' ';cells[y][x].glyph[1]=0;
}

void cl_ui_fill(int x, int y, int w, int h, int bg) {
    if (x<0 || y<0 || w<0 || h<0 || x>CL_SCREEN_W || y>CL_SCREEN_H ||
        w>CL_SCREEN_W-x || h>CL_SCREEN_H-y) return;
    for (int row = y; row < y + h && row < CL_SCREEN_H; ++row)
        for (int col = x; col < x + w && col < CL_SCREEN_W; ++col) {
            erase_glyph(col,row);
            cells[row][col].glyph[0] = ' '; cells[row][col].glyph[1] = 0;
            cells[row][col].fg = TEXT; cells[row][col].bg = bg;
        }
}

void cl_ui_put(int x, int y, const char *s, int fg, int limit) {
    if (!s || x<0 || x>=CL_SCREEN_W || y<0 || y>=CL_SCREEN_H || limit<=0) return;
    if (limit>CL_SCREEN_W-x) limit=CL_SCREEN_W-x;
    int end = x + limit;
    while (*s && y >= 0 && y < CL_SCREEN_H && x < end && x < CL_SCREEN_W) {
        unsigned cp; int n = utf8_decode(s, &cp);
        int w = n > 0 ? utf8_columns(cp) : -1;
        if (n < 1 || w < 1 || x < 0 || x + w > end || x + w > CL_SCREEN_W) return;
        for (int i=0;i<w;++i) erase_glyph(x+i,y);
        for (int i = 0; i < n; ++i) cells[y][x].glyph[i] = s[i];
        cells[y][x].glyph[n] = 0; cells[y][x].fg = fg;
        if (w == 2) { cells[y][x+1].glyph[0] = 0; cells[y][x+1].fg = fg; }
        x += w; s += n;
    }
}

void cl_ui_clipped(int x, int y, const char *s, int fg, int limit) {
    char text[CL_EDIT_BYTES],safe[CL_EDIT_BYTES];int used=0;
    if (!s) return;
    while (*s && used<CL_EDIT_BYTES-1) {
        unsigned cp;int n=utf8_decode(s,&cp);
        if (n<1) {safe[used++]='?';++s;continue;}
        if (utf8_columns(cp)<1) {safe[used++]=(cp=='\n' || cp=='\r' || cp=='\t')?' ':'?';s+=n;continue;}
        if (n>=CL_EDIT_BYTES-used) break;
        for (int i=0;i<n;++i) safe[used++]=s[i];
        s+=n;
    }
    safe[used]=0;
    if (text_clip(text,CL_EDIT_BYTES,safe,limit)) cl_ui_put(x,y,text,fg,limit);
}

void cl_ui_integer(int x, int y, int value, int fg) {
    char digits[16], text[16]; int n = 0, i = 0;
    do { digits[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (n) text[i++] = digits[--n];
    text[i] = 0; cl_ui_put(x,y,text,fg,15);
}

void cl_ui_line(int x, int y, int w) {
    for (int i = 0; i < w; ++i) cl_ui_put(x+i,y,"─",BORDER,1);
}

void cl_ui_box(int x, int y, int w, int h, const char *label) {
    cl_ui_fill(x,y,w,h,PAPER); cl_ui_line(x+1,y,w-2); cl_ui_line(x+1,y+h-1,w-2);
    cl_ui_put(x,y,"┌",BORDER,1); cl_ui_put(x+w-1,y,"┐",BORDER,1);
    cl_ui_put(x,y+h-1,"└",BORDER,1); cl_ui_put(x+w-1,y+h-1,"┘",BORDER,1);
    for (int i = y+1; i < y+h-1; ++i) {
        cl_ui_put(x,i,"│",BORDER,1); cl_ui_put(x+w-1,i,"│",BORDER,1);
    }
    cl_ui_put(x+3,y," ",INK,1); cl_ui_put(x+4,y,label,INK,w-8);
    cl_ui_put(x+4+utf8_width(label),y," ",INK,1);
}

static int character_count(const char *s, int until) {
    int pos = 0, count = 0;
    while (pos < until && s[pos]) {
        unsigned cp; int n = utf8_decode(s+pos,&cp);
        if (n < 1) break;
        ++count; pos += n;
    }
    return count;
}

void cl_ui_input(int x, int y, int w, const Edit *e, int active, int secret) {
    int bg = active ? SELECTED : STONE;
    int cursor_col = 0, start = 0;
    cl_ui_fill(x,y,w,3,bg);
    if (active) for (int row = y; row < y+3; ++row) cl_ui_put(x,row,"│",FOCUS,1);
    for (int pos = 0; pos < e->cursor;) {
        unsigned cp; int n = utf8_decode(e->text+pos,&cp);
        cursor_col += secret ? 1 : utf8_columns(cp); pos += n;
    }
    while (cursor_col > w-5 && e->text[start]) {
        unsigned cp; int n = utf8_decode(e->text+start,&cp);
        cursor_col -= secret ? 1 : utf8_columns(cp); start += n;
    }
    if (secret) {
        int n = character_count(e->text + start, (int)strlen(e->text+start));
        for (int i = 0; i < n && i < w-4; ++i) cl_ui_put(x+2+i,y+1,"*",TEXT,1);
    } else cl_ui_put(x+2,y+1,e->text+start,active?ACCENT:TEXT,w-4);
    if (active) {
        int cursor_x = x+2+cursor_col, glyph_w = 1;
        if (!secret && e->text[e->cursor]) {
            unsigned cp; utf8_decode(e->text+e->cursor,&cp); glyph_w = utf8_columns(cp);
        }
        for (int i = 0; i < glyph_w; ++i) cells[y+1][cursor_x+i].bg = PRIMARY;
        if (!e->text[e->cursor]) cl_ui_put(cursor_x,y+1,"│",ACCENT,1);
    }
}

void cl_ui_button(int x, int y, const char *label, int focused) {
    /* Like cl_ui_input fields, every cl_ui_button is neutral until it owns keyboard focus. */
    cl_ui_fill(x,y,18,3,focused?SELECTED:STONE);
    /* Selected option A: a quiet filled surface and a direction marker,
       with no enclosing frame. All buttons follow the same focus rule. */
    int width=utf8_width(label);if (width>14) width=14;
    int start=x+(18-width-2)/2;
    if (focused) cl_ui_put(start,y+1,">",ACCENT,1);
    /* Reserve the marker and one blank even when unfocused: no label jump. */
    cl_ui_clipped(start+2,y+1,label,focused?ACCENT:TEXT,14);

}

void cl_ui_scrollbar(int x,int y,int height,int selected,int count,int page_size) {
    if (x<0 || x>=CL_SCREEN_W || y<0 || height<2 || y>CL_SCREEN_H-height ||
        page_size<1 || count<=page_size || selected<0 || selected>=count) return;
    int pages=1+(count-1)/page_size,page=selected/page_size;
    int thumb=(int)((long long)height*page_size/count);
    if (thumb<1) thumb=1;
    if (thumb>=height) thumb=height-1;
    int start=(int)((long long)page*(height-thumb)/(pages-1));
    for (int i=0;i<height;++i)
        cl_ui_put(x,y+i,i>=start && i<start+thumb?"┃":"│",
                  i>=start && i<start+thumb?ACCENT:BORDER,1);
}

static int row_changed(int y) {
    for (int x=0;x<CL_SCREEN_W;++x)
        if (cells[y][x].fg!=previous[y][x].fg || cells[y][x].bg!=previous[y][x].bg ||
            strcmp(cells[y][x].glyph,previous[y][x].glyph)) return 1;
    return 0;
}

int cl_terminal_draw(FILE *stream, int force) {
    int changed=0, fg=-1,bg=-1;
    output_length=0; output_ok=1; output[0]=0;
    if (!previous_valid || force==1) { add("\033[?25l\033[?7l"); color(TEXT,BG); add("\033[2J"); }
    for (int y=0;y<CL_SCREEN_H;++y) {
        if (previous_valid && !force && !row_changed(y)) continue;
        changed=1; position(0,y);
        for (int x=0;x<CL_SCREEN_W;++x) {
            Cell *c=&cells[y][x];
            if (!c->glyph[0]) continue;
            if (fg!=c->fg || bg!=c->bg) { color(c->fg,c->bg); fg=c->fg; bg=c->bg; }
            add(c->glyph);
        }
    }
    if (!changed) return 1;
    position(0,0);
    if (!output_ok || fwrite(output,1,(size_t)output_length,stream)!=(size_t)output_length || ferror(stream)) return 0;
    for (int y=0;y<CL_SCREEN_H;++y)
        for (int x=0;x<CL_SCREEN_W;++x) previous[y][x]=cells[y][x];
    previous_valid=1;
    return 1;
}

int cl_terminal_text(FILE *stream) {
    for (int y=0;y<CL_SCREEN_H;++y) {
        for (int x=0;x<CL_SCREEN_W;++x) if (fputs(cells[y][x].glyph,stream)==EOF) return 0;
        if (fputc('\n',stream)==EOF) return 0;
    }
    return !ferror(stream);
}

void cl_terminal_reset(FILE *stream) {
    fputs("\033[?7h\033[0m\033[?25h\033[35;1H\n",stream);
}
