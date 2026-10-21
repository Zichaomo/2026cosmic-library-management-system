#include "migration/auth.h"
#include <string.h>

static int passed,failed;
static char root[512];
static void check(int ok,const char *name) {
    printf("[%s] %s\n",ok?"PASS":"FAIL",name);if (ok) ++passed;else ++failed;
}
static int path(char out[512],const char *suffix) {
    return text_copy(out,512,root) && text_append(out,512,"/") && text_append(out,512,suffix);
}
static int write_bytes(const char *name,const char *bytes) {
    FILE *f=fopen(name,"wb");if (!f) return 0;
    int ok=fwrite(bytes,1,strlen(bytes),f)==strlen(bytes);
    if (fclose(f)) ok=0;
    return ok;
}
static int find(const ClStore *s,int k,const char *key) {
    for (int i=0;i<s->table[k].count;++i) if (!strcmp(s->table[k].rows[i].field[1],key)) return i;
    return -1;
}
static int reopen(ClStore *s,const char *directory) {
    cl_store_dispose(s);return cl_store_open(s,directory);
}
static void test_csv(void) {
    char name[512];path(name,"parser.csv");
    const char *samples[]={"a,\"中文,标题\",\"a\"\"b\",\r\n",
        "\"第一行\r\n第二行\",tail\r\n","a,b,", "\"unterminated", "x\"bad,y",
        "\"a\"x,b", "\xc0\xaf,x"};
    for (int i=0;i<7;++i) {
        check(write_bytes(name,samples[i]),"prepare isolated CSV parser fixture");
        FILE *f=fopen(name,"rb");ClCsvReader reader;ClRow row={0};row.count=77;
        int status=f && cl_csv_begin(&reader,f)?cl_csv_read(&reader,&row):-1;
        if (i==0) check(status==1 && row.count==4 && !strcmp(row.field[1],"中文,标题") &&
            !strcmp(row.field[2],"a\"b") && !row.field[3][0],"CSV commas escaped quotes and trailing empty field");
        else if (i==1) check(status==1 && !strcmp(row.field[0],"第一行\r\n第二行"),"quoted CRLF survives as field data");
        else if (i==2) check(status==1 && row.count==3 && !*row.field[2],"trailing empty field at EOF without newline");
        else check(status==-1 && row.count==77,"malformed CSV/UTF-8 rejected without changing output");
        if (f) check(fclose(f)==0,"close parser fixture");
    }
    char large[600];for (int i=0;i<599;++i) large[i]='x';large[599]=0;
    write_bytes(name,large);FILE *f=fopen(name,"rb");ClCsvReader reader;ClRow row={0};
    check(f && cl_csv_begin(&reader,f) && cl_csv_read(&reader,&row)==-1,"reject oversized field instead of truncating");
    if (f) fclose(f);
    row.count=1;text_copy(row.field[0],512,"带逗号,和\"引号\"\n换行");
    f=fopen(name,"wb");int ok=f && cl_csv_write(f,&row);if (f && fclose(f)) ok=0;
    f=fopen(name,"rb");ClRow decoded;
    ok=ok && f && cl_csv_begin(&reader,f) && cl_csv_read(&reader,&decoded)==1 &&
        !strcmp(row.field[0],decoded.field[0]) && cl_csv_read(&reader,&decoded)==0;
    if (f && fclose(f)) ok=0;
    check(ok,"CSV writer and multiline reader round-trip Unicode");
    path(name,"rename-a");char other[512],unused[512];path(other,"rename-b");path(unused,"rename-c");
    write_bytes(name,"old");write_bytes(other,"new");
    check(rename(name,other)!=0,"target CRT rename refuses existing destination");
    check(rename(name,unused)==0,"target CRT rename works for absent destination");
}
static void test_auth_and_recovery(void) {
    char directory[512];path(directory,"fresh");ClStore s={0};ClSession session={0};
    int opened=cl_store_open(&s,directory);check(opened,"import explicit five-file empty dataset and create baseline snapshot");
    if (!opened) return;
    check(s.generation==1 && s.table[CL_READERS].count==0,"baseline generation does not invent accounts");
    check(cl_auth_login(&s,"admin","admin123",&session)==CL_AUTH_OK && session.role==CL_ROLE_ADMIN,"administrator login preserves old credentials");
    check(cl_auth_login(&s,"admin","wrong",&session)==CL_AUTH_WRONG_PASSWORD && !session.authenticated,"failed login clears earlier authenticated session");
    ClRegistration r={"persist_new","张, \"读者\"","reader123","reader123","13800138000"};
    ClRegistration invalid=r;invalid.phone="138001380003";
    check(cl_auth_register(&s,&invalid)==CL_REGISTER_BAD_PHONE && s.table[0].count==0,"bad phone cannot create or persist account");
    invalid=r;invalid.number="admin";
    check(cl_auth_register(&s,&invalid)==CL_REGISTER_DUPLICATE,"reserve administrator number");
    invalid=r;invalid.number="bad/no";
    check(cl_auth_register(&s,&invalid)==CL_REGISTER_BAD_NUMBER,"reject reader number separators");
    invalid=r;invalid.confirmation="different";
    check(cl_auth_register(&s,&invalid)==CL_REGISTER_MISMATCH,"password confirmation checked before persistence");
    invalid=r;invalid.password=invalid.confirmation="short";
    check(cl_auth_register(&s,&invalid)==CL_REGISTER_WEAK_PASSWORD,"weak password rejected");
    check(cl_auth_register(&s,&r)==CL_REGISTER_OK && s.table[0].count==1,"register quoted Chinese name in UTF-8 and persist");
    check(cl_auth_register(&s,&r)==CL_REGISTER_DUPLICATE && s.table[0].count==1,"duplicate registration cannot append another row");
    check(reopen(&s,directory) && cl_auth_login(&s,r.number,r.password,&session)==CL_AUTH_OK &&
        session.reader_id==1 && !strcmp(s.table[0].rows[0].field[2],r.name),"restart preserves password and escaped Chinese name");
    char first_salt[33];text_copy(first_salt,33,s.table[0].rows[0].field[4]);
    r.number="reader_two";
    check(cl_auth_register(&s,&r)==CL_REGISTER_OK && strcmp(first_salt,s.table[0].rows[1].field[4]),"same password gets independent demonstration salts");
    check(cl_auth_toggle_frozen(&s,1)==CL_STORE_OK &&
        cl_auth_login(&s,"persist_new","reader123",&session)==CL_AUTH_FROZEN && !session.authenticated,"frozen account cannot authenticate");
    check(reopen(&s,directory) && cl_auth_toggle_frozen(&s,1)==CL_STORE_OK &&
        cl_auth_login(&s,"persist_new","reader123",&session)==CL_AUTH_OK,"unfreeze persists and restores login");
    char saved[512];text_copy(saved,512,s.directory);text_append(s.directory,512,"/not-prepared");
    r.number="cannot_save";int before=s.table[0].count;
    check(cl_auth_register(&s,&r)==CL_REGISTER_STORAGE_ERROR && s.table[0].count==before,"failed snapshot open rolls back memory registration");
    text_copy(s.directory,512,saved);
    r.number="interrupted_before";cl_store_test_stop(1);
    check(cl_auth_register(&s,&r)==CL_REGISTER_UNCERTAIN && s.blocked &&
        cl_auth_login(&s,"admin","admin123",&session)==CL_AUTH_STORAGE_ERROR,"interrupted pre-footer operation blocks unsafe retries");
    check(reopen(&s,directory) && find(&s,0,r.number)<0 && s.table[0].count==before && s.recovered,"restart discards incomplete snapshot and retains previous generation");
    r.number="committed_later";cl_store_test_stop(2);
    check(cl_auth_register(&s,&r)==CL_REGISTER_PENDING_EXPORT && s.pending_export,"committed snapshot with interrupted export is not reported as unregistered");
    char csv[512];text_copy(csv,512,directory);text_append(csv,512,"/readers.csv");ClTable disk={0};
    check(cl_table_load(&disk,csv,CL_READERS) && disk.count==before,"CSV is still previous generation before recovery");cl_table_free(&disk);
    check(reopen(&s,directory) && cl_auth_login(&s,r.number,r.password,&session)==CL_AUTH_OK &&
        cl_table_load(&disk,csv,CL_READERS) && disk.count==before+1,"restart rolls committed snapshot forward into CSV");cl_table_free(&disk);
    ClRow book={0},loan={0},fine={0},reservation={0};book.count=8;loan.count=8;fine.count=7;reservation.count=6;
    const char *bv[]={"1","9787300000001","教材, \"特别版\"\r\n第二行","测试作者","1","0","available","2026-10-21"};
    const char *lv[]={"1","1","2","2026-10-21","2026-11-20","","active","0"};
    const char *fv[]={"1","2","1","50","2026-10-21","unpaid",""};
    const char *rv[]={"1","1","1","2026-10-21","","waiting"};
    for (int i=0;i<8;++i) {text_copy(book.field[i],512,bv[i]);text_copy(loan.field[i],512,lv[i]);}
    for (int i=0;i<7;++i) text_copy(fine.field[i],512,fv[i]);
    for (int i=0;i<6;++i) text_copy(reservation.field[i],512,rv[i]);
    int appended=cl_table_append(&s.table[1],&book) && cl_table_append(&s.table[2],&loan) &&
        cl_table_append(&s.table[3],&fine) && cl_table_append(&s.table[4],&reservation);
    cl_store_test_stop(3);
    check(appended && cl_store_commit(&s)==CL_STORE_PENDING_EXPORT,"five-table commit interrupted during CSV export");
    check(reopen(&s,directory) && s.table[1].count==1 && s.table[2].count==1 &&
        s.table[3].count==1 && s.table[4].count==1 &&
        !strcmp(s.table[1].rows[0].field[2],bv[2]) && !strcmp(s.table[1].rows[0].field[5],"0"),"restart restores all five tables and multiline title consistently");
    text_copy(s.table[1].rows[0].field[5],512,"2");int generation=s.generation;
    check(cl_store_commit(&s)==CL_STORE_FAILED && s.generation==generation,"reject available inventory above total before any commit");
    text_copy(s.table[1].rows[0].field[5],512,"0");
    text_copy(s.table[2].rows[0].field[1],512,"999");
    check(cl_store_commit(&s)==CL_STORE_FAILED,"reject dangling book reference");text_copy(s.table[2].rows[0].field[1],512,"1");
    ClRow duplicate=s.table[0].rows[0];text_copy(duplicate.field[1],512,"other-number");
    int duplicated=cl_table_append(&s.table[0],&duplicate);
    check(duplicated && cl_store_commit(&s)==CL_STORE_FAILED,"duplicate numeric ID rejected before persistence");
    if (duplicated) --s.table[0].count;
    text_copy(s.table[0].rows[0].field[4],512,"00112233445566778899aabbccddeexff");
    check(cl_store_commit(&s)==CL_STORE_FAILED,"invalid salt hex cannot replace a valid snapshot");
    check(reopen(&s,directory),"invalid in-memory edits do not damage committed data");
    text_copy(s.table[0].rows[0].field[7],512,"注销");
    check(cl_store_commit(&s)==CL_STORE_OK &&
        cl_auth_login(&s,"persist_new","reader123",&session)==CL_AUTH_NOT_FOUND,"cancelled reader status round-trips and cannot login");
    /* Leave a normal account for the external old-runtime compatibility probe. */
    text_copy(s.table[0].rows[0].field[7],512,"正常");check(cl_store_commit(&s)==CL_STORE_OK,"restore compatibility fixture account");
    text_copy(s.table[1].rows[0].field[2],512,"最终标题");check(cl_store_commit(&s)==CL_STORE_OK,"commit next generation before corrupt-slot test");
    char slot[512];text_copy(slot,512,directory);text_append(slot,512,s.active_slot?"/state.1":"/state.0");
    write_bytes(slot,"CORRUPTED\r\n");
    check(reopen(&s,directory) && s.recovered && !strcmp(s.table[1].rows[0].field[2],bv[2]),"corrupt newest slot falls back to complete previous snapshot with recovery flag");
    cl_store_dispose(&s);
}
static void test_legacy(void) {
    char directory[512];path(directory,"legacy");ClStore s={0};ClSession session={0};
    int opened=cl_store_open(&s,directory);check(opened,"import all five CSV files generated by actual legacy repositories");
    if (!opened) return;
    check(cl_auth_login(&s,"legacy001","旧密码123",&session)==CL_AUTH_OK && session.reader_id==1,"new FNV implementation authenticates old BCrypt-salted account");
    check(cl_auth_login(&s,"legacy001","wrong",&session)==CL_AUTH_WRONG_PASSWORD,"old account wrong password rejected");
    check(cl_auth_login(&s,"legacy_unicode","密码🔒ab",&session)==CL_AUTH_OK,
        "legacy supplementary-character password is independent of UI glyph-width restrictions");
    check(s.table[1].count>0 && s.table[2].count>0 && s.table[3].count>0 && s.table[4].count>0,"legacy catalog loan fine and reservation records retained");
    ClRegistration r={"new_legacy","新读者","reader123","reader123","13900139000"};
    check(cl_auth_register(&s,&r)==CL_REGISTER_OK && reopen(&s,directory) &&
        cl_auth_login(&s,"new_legacy","reader123",&session)==CL_AUTH_OK,"new account persists alongside legacy records after restart");
    cl_store_dispose(&s);
}
int main(int argc,char **argv) {
    if (argc==3 && !strcmp(argv[1],"--probe")) {
        ClStore s={0};int ok=cl_store_open(&s,argv[2]);cl_store_dispose(&s);return ok?0:2;
    }
    if (argc==3 && !strcmp(argv[1],"--persist")) {
        ClStore s={0};ClSession session={0};
        int ok=cl_store_open(&s,argv[2]) && cl_auth_login(&s,"persist_new","reader123",&session)==CL_AUTH_OK;
        cl_store_dispose(&s);return ok?0:2;
    }
    if (argc!=2 || strstr(argv[1],"build/migration/run-")!=argv[1] || !text_copy(root,512,argv[1])) return 2;
    test_csv();test_auth_and_recovery();test_legacy();
    char invalid[512];path(invalid,"invalid");ClStore invalid_store={0};
    check(!cl_store_open(&invalid_store,invalid),"invalid header dataset fails closed instead of creating blank tables");
    path(invalid,"missing");check(!cl_store_open(&invalid_store,invalid),"unprepared directory does not become a new empty library");
    path(invalid,"broken");int initialized=cl_store_open(&invalid_store,invalid);
    int baseline=initialized && cl_store_commit(&invalid_store)==CL_STORE_OK;
    cl_store_dispose(&invalid_store);
    char slot[512];text_copy(slot,512,invalid);text_append(slot,512,"/state.0");int damaged=write_bytes(slot,"BROKEN\r\n");
    text_copy(slot,512,invalid);text_append(slot,512,"/state.1");damaged=damaged && write_bytes(slot,"BROKEN\r\n");
    check(baseline && damaged && !cl_store_open(&invalid_store,invalid),"both snapshots corrupt: refuse silent fallback to possibly mixed CSV");
    printf("\n%d passed, %d failed\n",passed,failed);return failed?1:0;
}
