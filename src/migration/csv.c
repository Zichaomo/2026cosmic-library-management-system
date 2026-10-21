#include "migration/storage.h"
#include <string.h>
#include <stdlib.h>

const char *const cl_csv_names[CL_TABLES]={
    "readers.csv","books.csv","loans.csv","fines.csv","reservations.csv"};
const char *const cl_csv_headers[CL_TABLES]={
    "id,reader_no,name,password_hash,salt,phone,email,status,created_at",
    "id,isbn,title,author,total_copies,available_copies,status,created_at",
    "id,book_id,reader_id,borrow_date,due_date,return_date,status,renew_count",
    "id,reader_id,loan_id,amount_cents,created_at,status,paid_at",
    "id,book_id,reader_id,reserve_date,expire_date,status"};
const int cl_csv_counts[CL_TABLES]={9,8,8,7,6};

static int valid_utf8(const char *s) {
    while (*s) { unsigned cp;int n=utf8_decode(s,&cp);if (n<1) return 0;s+=n; }
    return 1;
}
int cl_csv_begin(ClCsvReader *r,FILE *f) {
    if (!r || !f) return 0;
    unsigned char bom[3];size_t n=fread(bom,1,3,f);
    if (ferror(f)) return 0;
    if (n!=3 || bom[0]!=239 || bom[1]!=187 || bom[2]!=191) {
        if (fseek(f,0,SEEK_SET)) return 0;
    }
    r->file=f;r->pending=-1;return 1;
}
static int next_byte(ClCsvReader *r) {
    if (r->pending>=0) { int c=r->pending;r->pending=-1;return c; }
    return fgetc(r->file);
}
int cl_csv_read(ClCsvReader *r,ClRow *out) {
    if (!r || !r->file || !out) return -1;
    ClRow row={0};int col=0,used=0,state=0,any=0;
    /* state: 0=start, 1=unquoted, 2=quoted, 3=closed quote. */
    for (;;) {
        int c=next_byte(r);
        if (c==EOF && ferror(r->file)) return -1;
        if (c==0) return -1; /* embedded NUL is not a C string */
        if (state==2) {
            if (c==EOF) return -1;
            if (c=='"') { state=3;continue; }
        } else if (state==3 && c=='"') { state=2;
        } else if (c==',' || c=='\r' || c=='\n' || c==EOF) {
            if (c==EOF && !any && col==0 && used==0 && state==0) return 0;
            row.field[col][used]=0;
            if (!valid_utf8(row.field[col])) return -1;
            if (c==',') {
                if (++col>=CL_CSV_COLUMNS) return -1;
                used=0;state=0;any=1;continue;
            }
            if (c=='\r') {
                int following=next_byte(r);
                if (following!=EOF && following!='\n') r->pending=following;
                if (ferror(r->file)) return -1;
            }
            row.count=col+1;*out=row;return 1;
        } else if (state==3) return -1;
        else if (c=='"') {
            if (state!=0) return -1;
            state=2;any=1;continue;
        } else state=1;
        if (used>=CL_CSV_BYTES-1) return -1;
        row.field[col][used++]=(char)c;any=1;
    }
}
int cl_csv_write(FILE *f,const ClRow *row) {
    if (!f || !row || row->count<1 || row->count>CL_CSV_COLUMNS) return 0;
    for (int col=0;col<row->count;++col) {
        const char *s=row->field[col];int quote=0;
        int length=0;while (length<CL_CSV_BYTES && s[length]) ++length;
        if (length==CL_CSV_BYTES) return 0;
        if (!valid_utf8(s)) return 0;
        for (const char *p=s;*p;++p) if (*p==',' || *p=='"' || *p=='\r' || *p=='\n') quote=1;
        if (quote && fputc('"',f)==EOF) return 0;
        for (;*s;++s) {
            if (*s=='"' && fputc('"',f)==EOF) return 0;
            if (fputc(*s,f)==EOF) return 0;
        }
        if (quote && fputc('"',f)==EOF) return 0;
        if (col+1<row->count && fputc(',',f)==EOF) return 0;
    }
    return fputs("\r\n",f)>=0 && !ferror(f);
}
int cl_table_append(ClTable *t,const ClRow *row) {
    if (!t || !row || t->count<0 || t->count>=CL_TABLE_LIMIT ||
        t->capacity<0 || t->capacity>CL_TABLE_LIMIT || t->count>t->capacity ||
        (t->capacity && !t->rows)) return 0;
    if (t->count==t->capacity) {
        int capacity=t->capacity?t->capacity*2:16;
        if (capacity>CL_TABLE_LIMIT) capacity=CL_TABLE_LIMIT;
        ClRow *rows=realloc(t->rows,(size_t)capacity*sizeof(*rows));
        if (!rows) return 0;
        t->rows=rows;t->capacity=capacity;
    }
    t->rows[t->count++]=*row;return 1;
}
void cl_table_free(ClTable *t) {
    if (t) { ClTable empty={0};free(t->rows);*t=empty; }
}
static int header_matches(const ClRow *row,int kind) {
    char header[512]="";
    if (row->count!=cl_csv_counts[kind]) return 0;
    for (int i=0;i<row->count;++i) {
        if ((i && !text_append(header,512,",")) || !text_append(header,512,row->field[i])) return 0;
    }
    return !strcmp(header,cl_csv_headers[kind]);
}
int cl_table_load(ClTable *t,const char *path,int kind) {
    if (!t || !path || kind<0 || kind>=CL_TABLES) return 0;
    FILE *f=fopen(path,"rb");if (!f) return 0;
    ClTable loaded={0};ClCsvReader reader;ClRow row;int status=-1;
    int ok=cl_csv_begin(&reader,f) && cl_csv_read(&reader,&row)==1 && header_matches(&row,kind);
    while (ok && (status=cl_csv_read(&reader,&row))==1)
        ok=row.count==cl_csv_counts[kind] && cl_table_append(&loaded,&row);
    if (status!=0) ok=0;
    if (fclose(f)) ok=0;
    if (!ok) { cl_table_free(&loaded);return 0; }
    cl_table_free(t);*t=loaded;return 1;
}
int cl_table_write(const ClTable *t,const char *path,int kind) {
    if (!t || !path || kind<0 || kind>=CL_TABLES || t->count<0 ||
        t->count>t->capacity || t->count>CL_TABLE_LIMIT || (t->count && !t->rows)) return 0;
    FILE *f=fopen(path,"wb");if (!f) return 0;
    int ok=fwrite("\xef\xbb\xbf",1,3,f)==3 && fputs(cl_csv_headers[kind],f)>=0 && fputs("\r\n",f)>=0;
    for (int i=0;ok && i<t->count;++i)
        ok=t->rows[i].count==cl_csv_counts[kind] && cl_csv_write(f,&t->rows[i]);
    if (ferror(f)) ok=0;
    if (fclose(f)) ok=0;
    return ok;
}
