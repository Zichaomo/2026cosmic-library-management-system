#include "migration/storage.h"
#include <string.h>

#ifdef CL_MIGRATION_TEST
static int stop_stage;
void cl_store_test_stop(int stage) { stop_stage=stage; }
static int interrupted(int stage) {
    if (stop_stage==stage) { stop_stage=0;return 1; }return 0;
}
#else
static int interrupted(int stage) { (void)stage;return 0; }
#endif

static int path(char *out,const ClStore *s,const char *name) {
    return text_copy(out,CL_PATH_BYTES,s->directory) &&
        text_append(out,CL_PATH_BYTES,"/") && text_append(out,CL_PATH_BYTES,name);
}
static int slot_path(char *out,const ClStore *s,int slot) {
    return path(out,s,slot?"state.1":"state.0");
}
void cl_store_dispose(ClStore *s) {
    if (s) {
        ClStore empty={0};
        for (int k=0;k<CL_TABLES;++k) cl_table_free(&s->table[k]);
        *s=empty;
    }
}
static int hash_prefix(const char *name,long length,char out[17]) {
    FILE *f=fopen(name,"rb");if (!f) return 0;
    u64 hash=14695981039346656037ull;unsigned char bytes[4096];
    int ok=length>=0;
    while (ok && length>0) {
        size_t wanted=length>(long)sizeof(bytes)?sizeof(bytes):(size_t)length;
        size_t n=fread(bytes,1,wanted,f);
        if (n!=wanted) { ok=0;break; }
        for (size_t i=0;i<n;++i) { hash^=bytes[i];hash*=1099511628211ull; }
        length-=(long)n;
    }
    if (ferror(f)) ok=0;
    if (fclose(f)) ok=0;
    return ok && cl_u64_hex(out,17,hash);
}
/* 1=valid snapshot, 2=explicit unused marker, 0=invalid, -1=I/O failure.
   A corrupt newest slot never destroys the valid preceding generation. */
static int read_slot(const ClStore *base,int slot,ClStore *loaded) {
    char name[CL_PATH_BYTES];
    if (!slot_path(name,base,slot)) return -1;
    FILE *f=fopen(name,"rb");if (!f) return -1;
    ClCsvReader reader;ClRow row;ClStore candidate={0};int counts[CL_TABLES];
    int resource_error=0;
    int ok=cl_csv_begin(&reader,f) && cl_csv_read(&reader,&row)==1;
    if (ok && row.count==1 && !strcmp(row.field[0],"EMPTY")) {
        ok=cl_csv_read(&reader,&row)==0;
        if (fclose(f)) ok=0;
        return ok?2:0;
    }
    ok=ok && row.count==8 && !strcmp(row.field[0],"COSMIC-SNAPSHOT") &&
        !strcmp(row.field[1],"1") && positive_number(row.field[2],&candidate.generation);
    for (int k=0;ok && k<CL_TABLES;++k)
        ok=cl_int_parse(row.field[3+k],&counts[k]) && counts[k]>=0 && counts[k]<=CL_TABLE_LIMIT;
    for (int k=0;ok && k<CL_TABLES;++k) {
        for (int i=0;ok && i<counts[k];++i) {
            ok=cl_csv_read(&reader,&row)==1 && row.count==cl_csv_counts[k];
            if (ok && !cl_table_append(&candidate.table[k],&row)) {
                resource_error=1;ok=0;
            }
        }
    }
    long prefix=ftell(f);char expected[17]="",actual[17];
    ok=ok && prefix>=0 && cl_csv_read(&reader,&row)==1 && row.count==2 &&
        !strcmp(row.field[0],"COMMIT") && strlen(row.field[1])==16 &&
        text_copy(expected,17,row.field[1]) && cl_csv_read(&reader,&row)==0;
    int io_error=ferror(f) || resource_error;
    if (fclose(f)) io_error=1;
    if (ok && !io_error && !hash_prefix(name,prefix,actual)) io_error=1;
    ok=ok && !io_error && !strcmp(actual,expected) &&
        cl_store_validate(candidate.table);
    if (!ok) { cl_store_dispose(&candidate);return io_error?-1:0; }
    candidate.active_slot=slot;
    if (!text_copy(candidate.directory,CL_PATH_BYTES,base->directory)) {
        cl_store_dispose(&candidate);return 0;
    }
    *loaded=candidate;return 1;
}
static int export_csv(ClStore *s,int inject) {
    char name[CL_PATH_BYTES];s->pending_export=1;
    for (int k=0;k<CL_TABLES;++k) {
        if (!path(name,s,cl_csv_names[k]) || !cl_table_write(&s->table[k],name,k)) return 0;
        if (inject && k==0 && interrupted(3)) return 0;
    }
    s->pending_export=0;return 1;
}
int cl_store_commit(ClStore *s) {
    if (!s || s->blocked || s->generation==CL_INT_MAX || !*s->directory ||
        !cl_store_validate(s->table)) return CL_STORE_FAILED;
    char name[CL_PATH_BYTES];int slot=s->active_slot<0?0:1-s->active_slot;
    if (!slot_path(name,s,slot)) return CL_STORE_FAILED;
    FILE *f=fopen(name,"wb");if (!f) return CL_STORE_FAILED;
    int ok=fprintf(f,"COSMIC-SNAPSHOT,1,%d",s->generation+1)>=0;
    for (int k=0;ok && k<CL_TABLES;++k) ok=fprintf(f,",%d",s->table[k].count)>=0;
    if (ok) ok=fputs("\r\n",f)>=0;
    for (int k=0;ok && k<CL_TABLES;++k)
        for (int i=0;ok && i<s->table[k].count;++i) ok=cl_csv_write(f,&s->table[k].rows[i]);
    long prefix=ftell(f);
    if (ferror(f) || prefix<0) ok=0;
    if (fclose(f)) ok=0;
    if (!ok) return CL_STORE_FAILED; /* no footer, so cannot be committed */
    if (interrupted(1)) { s->blocked=1;return CL_STORE_UNCERTAIN; }
    char checksum[17];
    if (!hash_prefix(name,prefix,checksum)) return CL_STORE_FAILED;
    f=fopen(name,"ab");if (!f) return CL_STORE_FAILED;
    ok=fprintf(f,"COMMIT,%s\r\n",checksum)>=0;
    if (ferror(f)) ok=0;
    if (fclose(f)) ok=0;
    /* Even a close error may have written a complete footer. Verify before
       deciding; if validation is impossible, block retries until reopen. */
    ClStore verified={0};int status=read_slot(s,slot,&verified);
    if (status!=1 || verified.generation!=s->generation+1) {
        cl_store_dispose(&verified);
        s->blocked=1;return CL_STORE_UNCERTAIN;
    }
    cl_store_dispose(&verified);s->generation++;s->active_slot=slot;
    s->pending_export=1;
    if (interrupted(2) || !export_csv(s,1)) return CL_STORE_PENDING_EXPORT;
    return CL_STORE_OK;
}
int cl_store_open(ClStore *out,const char *directory) {
    if (!out || !directory || !*directory) return 0;
    for (const unsigned char *p=(const unsigned char *)directory;*p;++p)
        if (*p<32 || *p>126) return 0; /* narrow CRT filenames, no UTF-8 claim */
    ClStore base={0},a={0},b={0};base.active_slot=-1;
    if (!text_copy(base.directory,CL_PATH_BYTES,directory)) return 0;
    int sa=read_slot(&base,0,&a),sb=read_slot(&base,1,&b);
    if (sa<0 || sb<0) { cl_store_dispose(&a);cl_store_dispose(&b);return 0; }
    if (sa==1 || sb==1) {
        if (sa==1 && (sb!=1 || a.generation>b.generation)) {
            base=a;cl_store_dispose(&b);
        } else if (sb==1 && (sa!=1 || b.generation>a.generation)) {
            base=b;cl_store_dispose(&a);
        } else { cl_store_dispose(&a);cl_store_dispose(&b);return 0; }
        base.recovered=sa==0 || sb==0;
        if (!export_csv(&base,0)) { cl_store_dispose(&base);return 0; }
    } else {
        /* Never silently treat missing/unreadable/corrupt files as empty. */
        if (sa!=2 || sb!=2) return 0;
        char name[CL_PATH_BYTES];
        for (int k=0;k<CL_TABLES;++k) {
            if (!path(name,&base,cl_csv_names[k]) || !cl_table_load(&base.table[k],name,k)) {
                cl_store_dispose(&base);return 0;
            }
        }
        if (!cl_store_validate(base.table) || cl_store_commit(&base)!=CL_STORE_OK) {
            cl_store_dispose(&base);return 0;
        }
    }
    *out=base;return 1;
}
