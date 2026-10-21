#include "migration/storage.h"
#include <string.h>

static int text_units(const char *s,int capacity,int empty_ok) {
    int units=0;
    if (!empty_ok && !*s) return 0;
    while (*s) {
        unsigned cp;int n=utf8_decode(s,&cp);
        if (n<1) return 0;
        units+=cp>65535?2:1;
        if (units>=capacity) return 0;
        s+=n;
    }
    return 1;
}
static int nonnegative(const char *s,int *out) {
    return *s!='-' && cl_int_parse(s,out) && *out>=0;
}
static int date(const char *s,int empty_ok) {
    ClDate d;return (!*s && empty_ok) || cl_date_parse(s,&d);
}
static int stamp(const char *s) {
    ClDateTime t;return cl_datetime_parse(s,&t) || date(s,0);
}
static int one_of(const char *s,const char *a,const char *b) {
    return !strcmp(s,a) || !strcmp(s,b);
}
static int row_valid(const ClRow *r,int k) {
    int id,a,b;u8 decoded[16];
    if (r->count!=cl_csv_counts[k] || !positive_number(r->field[0],&id)) return 0;
    const char (*v)[CL_CSV_BYTES]=r->field;
    if (k==CL_READERS) return text_units(v[1],32,0) && text_units(v[2],64,0) &&
        strlen(v[3])==16 && cl_hex_decode(decoded,8,v[3]) &&
        strlen(v[4])==32 && cl_hex_decode(decoded,16,v[4]) &&
        text_units(v[5],64,1) && text_units(v[6],64,1) &&
        (one_of(v[7],"正常","冻结") || !strcmp(v[7],"注销")) && stamp(v[8]);
    if (k==CL_BOOKS) return text_units(v[1],24,0) && text_units(v[2],80,0) &&
        text_units(v[3],48,0) && nonnegative(v[4],&a) && nonnegative(v[5],&b) &&
        b<=a && one_of(v[6],"available","suspended") && stamp(v[7]);
    if (!positive_number(v[1],&a) || !positive_number(v[2],&b)) return 0;
    if (k==CL_LOANS) return date(v[3],0) && date(v[4],0) && date(v[5],1) &&
        one_of(v[6],"active","returned") && nonnegative(v[7],&a) &&
        ((!strcmp(v[6],"active") && !*v[5]) || (!strcmp(v[6],"returned") && *v[5]));
    if (k==CL_FINES) return nonnegative(v[3],&a) && stamp(v[4]) &&
        one_of(v[5],"unpaid","paid") && (!*v[6] || stamp(v[6])) &&
        ((!strcmp(v[5],"unpaid") && !*v[6]) || (!strcmp(v[5],"paid") && *v[6]));
    return date(v[3],0) && date(v[4],1) &&
        (one_of(v[5],"waiting","ready") || one_of(v[5],"fulfilled","expired") ||
         !strcmp(v[5],"cancelled")) && (strcmp(v[5],"ready") || *v[4]);
}
static int has_id(const ClTable *t,const char *id) {
    int wanted;
    if (!positive_number(id,&wanted)) return 0;
    for (int i=0;i<t->count;++i) {
        int value;if (positive_number(t->rows[i].field[0],&value) && value==wanted) return 1;
    }
    return 0;
}
int cl_store_validate(const ClTable tables[CL_TABLES]) {
    if (!tables) return 0;
    for (int k=0;k<CL_TABLES;++k) {
        const ClTable *t=&tables[k];
        if (t->count<0 || t->count>CL_TABLE_LIMIT || t->count>t->capacity || (t->count && !t->rows)) return 0;
        for (int i=0;i<t->count;++i) {
            const ClRow *r=&t->rows[i];
            /* Public structs can be edited: bound scans before strlen/parser. */
            for (int c=0;c<CL_CSV_COLUMNS;++c) {
                int n=0;while (n<CL_CSV_BYTES && r->field[c][n]) ++n;
                if (n==CL_CSV_BYTES) return 0;
                const char *p=r->field[c];
                while (*p) { unsigned cp;int bytes=utf8_decode(p,&cp);if (bytes<1) return 0;p+=bytes; }
            }
            if (!row_valid(r,k)) return 0;
            int id;positive_number(r->field[0],&id);
            for (int j=0;j<i;++j) {
                int other;positive_number(t->rows[j].field[0],&other);
                if (id==other || ((k==CL_READERS || k==CL_BOOKS) &&
                    !strcmp(r->field[1],t->rows[j].field[1]))) return 0;
            }
            if (k==CL_LOANS || k==CL_RESERVATIONS) {
                if (!has_id(&tables[CL_BOOKS],r->field[1]) || !has_id(&tables[CL_READERS],r->field[2])) return 0;
            } else if (k==CL_FINES) {
                if (!has_id(&tables[CL_READERS],r->field[1]) || !has_id(&tables[CL_LOANS],r->field[2])) return 0;
            }
        }
    }
    return 1;
}
