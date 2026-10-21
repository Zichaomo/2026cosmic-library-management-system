#include "utils/foundation.h"

int utf8_decode(const char *s, unsigned *cp) {
    if (!s || !cp) return -1;
    const unsigned char *p = (const unsigned char *)s;
    int n;
    unsigned value;
    if (p[0] < 0x80) { *cp = p[0]; return p[0] ? 1 : 0; }
    if (p[0] >= 0xc2 && p[0] <= 0xdf) { n = 2; value = p[0] & 31u; }
    else if (p[0] >= 0xe0 && p[0] <= 0xef) { n = 3; value = p[0] & 15u; }
    else if (p[0] >= 0xf0 && p[0] <= 0xf4) { n = 4; value = p[0] & 7u; }
    else return -1;
    for (int i = 1; i < n; ++i) {
        if (p[i] == 0 || (p[i] & 0xc0u) != 0x80u) return -1;
        value = (value << 6) | (p[i] & 63u);
    }
    if ((n == 2 && value < 0x80) || (n == 3 && value < 0x800) ||
        (n == 4 && value < 0x10000) || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff)) return -1;
    *cp = value;
    return n;
}

int utf8_columns(unsigned cp) {
    if (cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return -1;
    /* Supported scope: ASCII and ordinary CJK. Do not guess grapheme widths. */
    if (cp < 32 || (cp >= 127 && cp < 160) ||
        (cp >= 0x300 && cp <= 0x36f) ||
        (cp >= 0x200b && cp <= 0x200f) ||
        (cp >= 0x202a && cp <= 0x202e) ||
        (cp >= 0x2060 && cp <= 0x206f) ||
        (cp >= 0xfe00 && cp <= 0xfe0f) ||
        (cp >= 0x1f000 && cp <= 0x1ffff)) return -1;
    if ((cp >= 0x1100 && cp <= 0x115f) || cp == 0x2329 || cp == 0x232a ||
        (cp >= 0x2e80 && cp <= 0xa4cf) ||
        (cp >= 0xac00 && cp <= 0xd7a3) ||
        (cp >= 0xf900 && cp <= 0xfaff) ||
        (cp >= 0xfe10 && cp <= 0xfe19) ||
        (cp >= 0xfe30 && cp <= 0xfe6f) ||
        (cp >= 0xff01 && cp <= 0xff60) ||
        (cp >= 0xffe0 && cp <= 0xffe6) ||
        (cp >= 0x20000 && cp <= 0x3fffd)) return 2;
    return 1;
}

int utf8_width(const char *s) {
    if (!s) return -1;
    int columns = 0;
    while (*s) {
        unsigned cp;
        int n = utf8_decode(s, &cp);
        if (n < 1 || utf8_columns(cp) < 0) return -1;
        if (columns>CL_INT_MAX-utf8_columns(cp)) return -1;
        columns += utf8_columns(cp);
        s += n;
    }
    return columns;
}

int utf8_boundary(const char *text,int offset) {
    if (!text || offset<0 || utf8_width(text)<0) return 0;
    int pos=0;
    while (pos<offset && text[pos]) {
        unsigned cp;int n=utf8_decode(text+pos,&cp);
        if (n>CL_INT_MAX-pos) return 0;
        pos+=n;
    }
    return pos==offset;
}
int utf8_previous(const char *text,int offset,int *previous) {
    if (!previous || offset<=0 || !utf8_boundary(text,offset)) return 0;
    int pos=offset-1;
    while (pos>0 && ((unsigned char)text[pos]&0xc0u)==0x80u) --pos;
    *previous=pos;return 1;
}
int utf8_next(const char *text,int offset,int *next) {
    if (!next || !utf8_boundary(text,offset) || !text[offset]) return 0;
    unsigned cp;int n=utf8_decode(text+offset,&cp);
    if (n>CL_INT_MAX-offset) return 0;
    *next=offset+n;return 1;
}
