#include "utils/foundation.h"
#include <string.h>

int text_copy(char *out, int capacity, const char *in) {
    if (!out || !in || capacity<1) return 0;
    size_t n=strlen(in);
    if (n>=(size_t)capacity) return 0;
    for (size_t i=0;i<=n;++i) out[i]=in[i];
    return 1;
}

int text_append(char *out, int capacity, const char *in) {
    if (!out || !in || capacity<1) return 0;
    int used=0;
    while (used<capacity && out[used]) ++used;
    if (used==capacity) return 0;
    size_t n=strlen(in);
    if (n>=(size_t)(capacity-used)) return 0;
    for (size_t i=0;i<n;++i) out[used+i]=in[i];
    out[used+n]=0;
    return 1;
}

int text_clip(char *out, int capacity, const char *in, int columns) {
    if (!out || !in || capacity<1 || columns<0) return 0;
    int total=utf8_width(in),used=0,bytes=0;
    if (total<0) return 0;
    int dots=total>columns && columns>=3;
    int limit=dots?columns-3:columns;
    while (in[bytes]) {
        unsigned cp;
        int n=utf8_decode(in+bytes,&cp),w=utf8_columns(cp);
        if (w>limit-used) break;
        if (n>=capacity-bytes) return 0;
        bytes+=n;used+=w;
    }
    if (dots && capacity-bytes<=3) return 0;
    for (int i=0;i<bytes;++i) out[i]=in[i];
    if (dots) for (int i=0;i<3;++i) out[bytes++]='.';
    out[bytes]=0;
    return 1;
}

int cl_int_parse(const char *text, int *value) {
    if (!text || !value) return 0;
    int negative=*text=='-';
    if (negative) ++text;
    if (!*text) return 0;
    unsigned magnitude=0,limit=negative?2147483648u:2147483647u;
    for (;*text;++text) {
        unsigned digit=(unsigned)(*text-'0');
        if (digit>9 || magnitude>(limit-digit)/10) return 0;
        magnitude=magnitude*10+digit;
    }
    *value=negative?(magnitude==2147483648u?CL_INT_MIN:-(int)magnitude):(int)magnitude;
    return 1;
}

int positive_number(const char *text, int *value) {
    int parsed;
    if (!text || *text=='-' || !value || !cl_int_parse(text,&parsed) || parsed<=0) return 0;
    *value=parsed;
    return 1;
}

int cl_int_format(char *out, int capacity, int value) {
    char reverse[10],result[12];int n=0,used=0;
    unsigned magnitude=value<0?(unsigned)(-(value+1))+1u:(unsigned)value;
    do { reverse[n++]=(char)('0'+magnitude%10);magnitude/=10; } while (magnitude);
    if (value<0) result[used++]='-';
    while (n) result[used++]=reverse[--n];
    result[used]=0;
    return text_copy(out,capacity,result);
}

static int hex_digit(char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
}

int cl_hex_encode(char *out, int capacity, const u8 *bytes, int count) {
    static const char digits[]="0123456789abcdef";
    if (!out || capacity<1 || count<0 || (!bytes && count) || count>(capacity-1)/2) return 0;
    for (int i=0;i<count;++i) {
        out[i*2]=digits[bytes[i]>>4];out[i*2+1]=digits[bytes[i]&15];
    }
    out[count*2]=0;
    return 1;
}

int cl_hex_decode(u8 *out, int capacity, const char *hex) {
    if (!hex || capacity<0) return 0;
    size_t length=strlen(hex);
    if (length%2 || length/2>(size_t)capacity || (!out && length)) return 0;
    for (size_t i=0;i<length;++i) if (hex_digit(hex[i])<0) return 0;
    for (size_t i=0;i<length/2;++i)
        out[i]=(u8)(hex_digit(hex[i*2])*16+hex_digit(hex[i*2+1]));
    return 1;
}

int cl_u64_hex(char *out, int capacity, u64 value) {
    static const char digits[]="0123456789abcdef";
    if (!out || capacity<17) return 0;
    for (int i=15;i>=0;--i) { out[i]=digits[value&15];value>>=4; }
    out[16]=0;
    return 1;
}
