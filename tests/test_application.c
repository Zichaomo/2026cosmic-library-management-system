#include "ui_next/app.h"
#include <string.h>

static int passed,failed;static char root[512];static NextApp app;
static void check(int ok,const char *name) {
    printf("[%s] %s\n",ok?"PASS":"FAIL",name);if (ok) ++passed;else ++failed;
}
static void pathname(char out[512],const char *name) {text_copy(out,512,root);text_append(out,512,"/");text_append(out,512,name);}
static int open_case(ClStore *s,const char *name) {char dir[512];pathname(dir,name);return cl_store_open(s,dir);}
static ClDate day(int year,int month,int d) {ClDate date={year,month,d};return date;}
static void actors(ClStore *s,ClSession *admin,ClSession *reader) {
    check(cl_auth_login(s,"admin","admin123",admin)==CL_AUTH_OK,"authenticate administrator for domain operations");
    check(cl_auth_login(s,"reader001","reader123",reader)==CL_AUTH_OK,"authenticate persisted reader for domain operations");
}
static int register_reader(ClStore *s,const char *number,ClSession *actor) {
    ClRegistration r={number,"测试读者","reader123","reader123","13900139000"};
    return cl_auth_register(s,&r)==CL_REGISTER_OK && cl_auth_login(s,number,"reader123",actor)==CL_AUTH_OK;
}
static int book(const ClStore *s,int id,int field) {
    int index=cl_library_find(s,1,id);return index<0?-1:cl_library_number(&s->table[1].rows[index],field);
}
static void test_circulation(void) {
    ClStore s={0};ClSession admin={0},reader={0},other={0};ClDate date=day(2026,10,1);
    if (!open_case(&s,"circulation")) {check(0,"open circulation fixture");return;}
    actors(&s,&admin,&reader);check(register_reader(&s,"reader002",&other),"register second actor");
    int loan=0;
    check(cl_library_borrow(&s,&other,reader.reader_id,1,date,&loan)==CL_LIB_DENIED && book(&s,1,5)==3,"reader cannot borrow on another reader's behalf");
    check(cl_library_add_book(&s,&reader,"X","无权限","作者",1,date)==CL_LIB_DENIED,"reader cannot add catalog records");
    check(cl_library_borrow(&s,&reader,reader.reader_id,1,date,&loan)==CL_LIB_OK && loan==1 && book(&s,1,5)==2,"borrow commits inventory and loan in one transaction");
    int duplicate=99;
    check(cl_library_borrow(&s,&reader,reader.reader_id,1,date,&duplicate)==CL_LIB_DUPLICATE && duplicate==99 && book(&s,1,5)==2,"repeat borrow does not consume a second copy");
    int li=cl_library_find(&s,2,loan);
    check(!strcmp(s.table[2].rows[li].field[4],"2026-10-31"),"borrow period is thirty calendar days");
    check(cl_library_overdue(&s.table[2].rows[li],day(2026,10,31))==0 &&
        cl_library_overdue(&s.table[2].rows[li],day(2026,11,1))==1,"due date valid all day; next date is one day overdue");
    check(cl_library_return(&s,&other,loan,date,NULL)==CL_LIB_DENIED,"reader cannot return another reader's loan");
    check(cl_library_renew(&s,&reader,loan,date)==CL_LIB_OK,"first renewal succeeds");
    li=cl_library_find(&s,2,loan);
    check(!strcmp(s.table[2].rows[li].field[4],"2026-11-15"),"renewal adds fifteen calendar days to original due date");
    check(cl_library_renew(&s,&reader,loan,date)==CL_LIB_RENEW_LIMIT,"second renewal rejected");
    int fine=0;
    check(cl_library_return(&s,&reader,loan,day(2026,11,16),&fine)==CL_LIB_OK && fine==50 && book(&s,1,5)==3,"return one day late creates exactly fifty cents and restores inventory");
    check(cl_library_return(&s,&reader,loan,day(2026,11,16),&fine)==CL_LIB_CLOSED && s.table[3].count==1,"repeat return cannot create another fine");
    int fid=cl_library_number(&s.table[3].rows[0],0);
    check(cl_library_pay(&s,&other,fid,date)==CL_LIB_DENIED,"reader cannot pay another reader's fine");
    check(cl_library_pay(&s,&reader,fid,day(2026,11,16))==CL_LIB_OK &&
        cl_library_pay(&s,&reader,fid,day(2026,11,16))==CL_LIB_CLOSED,"simulated payment is idempotent under repeated requests");
    check(cl_library_book_action(&s,&admin,2,0,date)==CL_LIB_OK &&
        cl_library_borrow(&s,&reader,reader.reader_id,2,date,NULL)==CL_LIB_NO_COPY,"suspended book cannot be borrowed");
    check(cl_library_book_action(&s,&admin,2,0,date)==CL_LIB_OK &&
        cl_library_book_action(&s,&admin,2,1,date)==CL_LIB_OK && book(&s,2,4)==4,"resume and add-copy persist catalog changes");
    check(cl_library_add_book(&s,&admin,"BOUNDARY","上限图书","作者",CL_INT_MAX,date)==CL_LIB_OK,"accept maximum valid inventory count");
    int max_id=cl_library_number(&s.table[1].rows[s.table[1].count-1],0);
    check(cl_library_book_action(&s,&admin,max_id,1,date)==CL_LIB_INVALID,"inventory increment refuses signed overflow");
    check(cl_library_add_book(&s,&admin,"BOUNDARY","重复","作者",1,date)==CL_LIB_DUPLICATE,"duplicate ISBN rejected by business layer");
    cl_store_dispose(&s);
}
static void test_limits(void) {
    ClStore s={0};ClSession admin={0},r={0};ClDate date=day(2026,10,1);
    if (!open_case(&s,"limits")) {check(0,"open limit fixture");return;}actors(&s,&admin,&r);
    int ok=1;
    for (int b=1;b<=5;++b) if (cl_library_borrow(&s,&r,r.reader_id,b,date,NULL)!=CL_LIB_OK) ok=0;
    check(ok && cl_library_borrow(&s,&r,r.reader_id,6,date,NULL)==CL_LIB_LIMIT,"fifth active loan succeeds; sixth rejected");
    check(cl_library_renew(&s,&r,1,day(2026,11,1))==CL_LIB_OVERDUE,"overdue loan cannot renew");
    int cents=0;
    check(cl_library_return(&s,&r,1,day(2026,11,20),&cents)==CL_LIB_OK && cents==1000,"twenty late dates accrue 1000 cents");
    check(cl_library_borrow(&s,&r,r.reader_id,6,date,NULL)==CL_LIB_BLOCKED,"unpaid fine threshold blocks borrowing");
    check(cl_library_pay(&s,&r,1,day(2026,11,20))==CL_LIB_OK &&
        cl_library_borrow(&s,&r,r.reader_id,6,date,NULL)==CL_LIB_OK,"payment clears fine threshold and permits borrowing");
    check(cl_library_reader_action(&s,&admin,r.reader_id,date)==CL_LIB_OK &&
        cl_library_borrow(&s,&r,r.reader_id,7,date,NULL)==CL_LIB_DENIED,"session cannot bypass a newly frozen reader state");
    check(cl_library_return(&s,&r,2,date,NULL)==CL_LIB_OK,"frozen reader may still return an existing loan");
    check(cl_library_reader_action(&s,&admin,r.reader_id,date)==CL_LIB_OK,"unfreeze remains available to administrator");
    cl_store_dispose(&s);
}
static void test_queue(void) {
    ClStore s={0};ClSession admin={0},a={0},b={0},c={0};ClDate date=day(2026,10,1);
    if (!open_case(&s,"queue")) {check(0,"open queue fixture");return;}actors(&s,&admin,&a);
    check(register_reader(&s,"queue002",&b) && register_reader(&s,"queue003",&c),"register FIFO queue actors");
    check(cl_library_add_book(&s,&admin,"ONE","最后一个副本","作者",1,date)==CL_LIB_OK,"create single-copy book");
    int bid=cl_library_number(&s.table[1].rows[s.table[1].count-1],0),loan=0;
    check(cl_library_borrow(&s,&a,a.reader_id,bid,date,&loan)==CL_LIB_OK &&
        cl_library_reserve(&s,&b,b.reader_id,bid,date)==CL_LIB_OK &&
        cl_library_reserve(&s,&c,c.reader_id,bid,date)==CL_LIB_OK,"readers queue in order after last copy is borrowed");
    check(cl_library_reserve(&s,&b,b.reader_id,bid,date)==CL_LIB_DUPLICATE &&
        cl_library_reserve(&s,&a,a.reader_id,bid,date)==CL_LIB_DUPLICATE,"duplicate queue entry and reserve-own-loan rejected");
    check(cl_library_reserve(&s,&b,b.reader_id,2,date)==CL_LIB_AVAILABLE,"reservation rejected while copies are available");
    check(cl_library_renew(&s,&a,loan,date)==CL_LIB_QUEUE,"open reservation prevents renewal");
    check(cl_library_return(&s,&a,loan,day(2026,10,2),NULL)==CL_LIB_OK &&
        !strcmp(s.table[4].rows[0].field[5],"ready") &&
        !strcmp(s.table[4].rows[0].field[4],"2026-10-04") && book(&s,bid,5)==0,"return holds one copy for first reader through two-day expiry date");
    check(cl_library_borrow(&s,&c,c.reader_id,bid,day(2026,10,2),NULL)==CL_LIB_NO_COPY,"later reader cannot take a held copy");
    check(cl_library_refresh(&s,day(2026,10,4))==CL_LIB_OK && !strcmp(s.table[4].rows[0].field[5],"ready"),"hold stays valid throughout its expiry date");
    check(cl_library_refresh(&s,day(2026,10,5))==CL_LIB_OK &&
        !strcmp(s.table[4].rows[0].field[5],"expired") && !strcmp(s.table[4].rows[1].field[5],"ready") &&
        !strcmp(s.table[4].rows[1].field[4],"2026-10-07") && book(&s,bid,5)==0,"next date expires hold and allocates next FIFO reservation atomically");
    check(cl_library_borrow(&s,&c,c.reader_id,bid,day(2026,10,5),&loan)==CL_LIB_OK &&
        book(&s,bid,5)==0 && !strcmp(s.table[4].rows[1].field[5],"fulfilled"),"ready reader picks up held copy without a second stock decrement");
    check(cl_library_return(&s,&c,loan,day(2026,10,6),NULL)==CL_LIB_OK && book(&s,bid,5)==1,"return after pickup restores exactly one copy");
    check(cl_library_borrow(&s,&a,a.reader_id,bid,day(2026,10,6),&loan)==CL_LIB_OK &&
        cl_library_reserve(&s,&b,b.reader_id,bid,day(2026,10,6))==CL_LIB_OK &&
        cl_library_reserve(&s,&c,c.reader_id,bid,day(2026,10,6))==CL_LIB_OK &&
        cl_library_return(&s,&a,loan,day(2026,10,6),NULL)==CL_LIB_OK,"prepare cancellation while copy is held");
    int reservation=cl_library_number(&s.table[4].rows[2],0);
    check(cl_library_cancel(&s,&c,reservation,day(2026,10,6))==CL_LIB_DENIED,"reader cannot cancel another reader's reservation");
    check(cl_library_cancel(&s,&b,reservation,day(2026,10,6))==CL_LIB_OK &&
        !strcmp(s.table[4].rows[3].field[5],"ready") && book(&s,bid,5)==0,"cancel held reservation releases then reallocates copy");
    check(cl_library_cancel(&s,&b,reservation,day(2026,10,6))==CL_LIB_CLOSED,"repeat cancellation rejected");
    cl_store_dispose(&s);
}
static void test_failure(void) {
    ClStore s={0};ClSession admin={0},reader={0};ClDate date=day(2026,10,1);
    if (!open_case(&s,"failure")) {check(0,"open failure fixture");return;}actors(&s,&admin,&reader);
    int loan=99;cl_store_test_stop(1);
    check(cl_library_borrow(&s,&reader,reader.reader_id,1,date,&loan)==CL_LIB_UNCERTAIN && s.blocked && loan==99,"pre-footer interruption blocks caller and leaves output ID untouched");
    check(cl_library_refresh(&s,date)==CL_LIB_OK && s.table[2].count==0 && book(&s,1,5)==3,"F5-style recovery restores both stock and loan count before retry");
    cl_store_test_stop(2);
    check(cl_library_borrow(&s,&reader,reader.reader_id,1,date,&loan)==CL_LIB_PENDING && loan>0,"post-commit interruption reports committed borrow");
    check(cl_library_refresh(&s,date)==CL_LIB_OK && s.table[2].count==1 && book(&s,1,5)==2,"restart recovery does not duplicate committed borrow");
    cl_store_test_stop(3);
    check(cl_library_return(&s,&reader,loan,day(2026,11,1),NULL)==CL_LIB_PENDING &&
        cl_library_refresh(&s,day(2026,11,1))==CL_LIB_OK && s.table[3].count==1 && book(&s,1,5)==3,
        "interrupted return restores stock loan fine from same committed generation");
    cl_store_dispose(&s);
}
static void key(Key k) {Event e={KEY_NONE,{0}};e.key=k;cl_app_event(&app,e,day(2026,10,21));}
static void type(const char *s) {
    while (*s) {Event e={KEY_TEXT,{0}};unsigned cp;int n=utf8_decode(s,&cp);if (n<1) return;
        for (int i=0;i<n;++i) e.text[i]=s[i];
        cl_app_event(&app,e,day(2026,10,21));s+=n;}
}
static void login(const char *user) {
    cl_app_enter(&app,N_LOGIN);type(user);key(KEY_ENTER);type(!strcmp(user,"admin")?"admin123":"reader123");key(KEY_ENTER);key(KEY_ENTER);
}
static void test_ui(void) {
    char directory[512];pathname(directory,"ui");
    if (!cl_app_init(&app,directory,day(2026,10,21))) {check(0,"open UI fixture");return;}
    check(app.page==N_WELCOME && app.running,"new application starts on welcome page");
    cl_app_enter(&app,N_LOGIN);type("admn");key(KEY_LEFT);type("i");
    check(!strcmp(app.field[0].text,"admin") && app.field[0].cursor==4,"PPT example inserts i at the caret without replacing n");
    key(KEY_END);key(KEY_ENTER);type("admin123");key(KEY_ENTER);key(KEY_ENTER);
    check(app.page==N_ADMIN && app.session.role==CL_ROLE_ADMIN,"edited PPT example authenticates through the actual login form");
    login("reader001");check(app.page==N_READER && app.session.authenticated && !*app.field[1].text,"keyboard login uses migrated authentication and clears password");
    key(KEY_ENTER);check(app.page==N_CATALOG && app.match_count==20,"reader home opens persisted catalog");
    type("k");type("srlj");check(app.match_count==1 && cl_app_selected_id(&app)==2,"catalog initials search finds the real persisted Chinese title");
    cl_app_enter(&app,N_CATALOG);type("k");type("SHENRU");check(app.match_count==1 && cl_app_selected_id(&app)==2,"catalog uppercase full-pinyin search finds the same record");
    cl_app_enter(&app,N_CATALOG);type("k");type("demo-20");check(app.match_count==1 && cl_app_selected_id(&app)==20,"catalog ISBN search ignores ASCII case");
    cl_app_enter(&app,N_CATALOG);
    type("J");check(app.command==CMD_BORROW && app.store.table[2].count==0,"borrow key asks confirmation before writing");
    type("N");check(app.command==0 && app.store.table[2].count==0,"cancelled confirmation leaves storage unchanged");
    type("J");type("Y");check(app.store.table[2].count==1 && book(&app.store,1,5)==2,"keyboard confirmation completes persisted borrow");
    type("K");type("N");check(app.searching && app.page==N_CATALOG && !app.command,"letters in search do not invoke commands");
    key(KEY_BACKSPACE);key(KEY_ENTER);key(KEY_ESC);key(KEY_TAB);key(KEY_ENTER);
    check(app.page==N_MY_LOANS && app.match_count==1,"reader can navigate to own persisted loans");
    type("k");type("chengxu");check(app.match_count==1,"loan search resolves book ID to a pinyin-searchable title");
    cl_app_enter(&app,N_MY_LOANS);
    type("g");check(app.command==CMD_RETURN && app.command_id==cl_app_selected_id(&app),"G return binds the selected persisted loan");type("N");
    type("x");check(app.command==CMD_RENEW && app.command_id==cl_app_selected_id(&app),"X renewal binds the selected persisted loan");type("N");
    NextPage before=app.page;cl_app_enter(&app,N_READERS);check(app.page==before,"reader cannot navigate into administrator pages");
    key(KEY_ESC);login("admin");check(app.page==N_ADMIN,"administrator login opens administrator workspace");
    static const NextPage menus[]={N_CATALOG,N_BOOKS,N_READERS,N_LOANS,N_BORROW,N_RESERVATIONS,N_FINES,N_STATS};
    int navigation=1;
    for (int i=0;i<8;++i) {cl_app_enter(&app,N_ADMIN);for (int j=0;j<i;++j) key(KEY_TAB);key(KEY_ENTER);if (app.page!=menus[i]) navigation=0;}
    check(navigation,"administrator menu reaches all eight management destinations");
    cl_app_enter(&app,N_READERS);type("k");type("sldz");check(app.match_count==1,"administrator can find a reader by name initials");
    cl_app_enter(&app,N_READERS);type("d");check(app.command==CMD_FREEZE,"D opens reader freeze confirmation");type("N");
    cl_app_enter(&app,N_BOOKS);type("z");check(app.command==CMD_COPY,"Z asks before increasing copies");type("N");
    type("t");check(app.command==CMD_SUSPEND,"T asks before changing lending status");type("N");
    type("y");check(app.page==N_BORROW && app.form_reserve,"Y opens administrator reservation form");
    cl_app_enter(&app,N_BOOKS);type("c");check(app.page==N_NEW_BOOK,"C opens new book without conflicting with X renewal");
    cl_app_enter(&app,N_BOOKS);type("k");type("jzycdt");check(app.searching && !app.command && app.page==N_BOOKS,"all action letters remain text while searching");
    key(KEY_ENTER);type("f");check(app.page==N_ADMIN,"F returns from the list to the workspace");
    cl_app_enter(&app,N_NEW_BOOK);type("UI-NEW");key(KEY_TAB);type("中文图书");key(KEY_TAB);type("张老师");key(KEY_TAB);
    key(KEY_HOME);key(KEY_DELETE);type("0");key(KEY_TAB);key(KEY_ENTER);
    check(app.page==N_NEW_BOOK && app.focus==3 && !strcmp(app.field[1].text,"中文图书"),"book validation preserves Chinese fields and focuses invalid copies");
    key(KEY_HOME);key(KEY_DELETE);type("2");key(KEY_TAB);key(KEY_ENTER);
    check(app.page==N_BOOKS && app.store.table[1].count==21,"administrator form saves catalog through transactional service");
    cl_app_enter(&app,N_REGISTER);type("ui_new");key(KEY_TAB);type("中文新读者");key(KEY_TAB);type("reader123");key(KEY_TAB);type("reader123");key(KEY_TAB);type("13900139000");key(KEY_TAB);key(KEY_ENTER);
    check(app.page==N_LOGIN && !strcmp(app.field[0].text,"ui_new") && !*app.field[2].text,"registration page persists reader and clears secret fields");
    login("ui_new");check(app.page==N_READER,"newly registered reader logs in through same UI");
    cl_app_enter(&app,N_MY_LOANS);check(app.match_count==0,"another reader cannot see first reader's loan rows");
    cl_app_enter(&app,N_REGISTER);type("unicode_secret");key(KEY_TAB);type("特殊密码读者");key(KEY_TAB);
    type("密码🔒ab");key(KEY_TAB);type("密码🔒ab");key(KEY_TAB);type("13800138000");key(KEY_TAB);key(KEY_ENTER);
    check(app.page==N_LOGIN,"registration accepts masked supplementary-character password");
    type("密码🔒ab");key(KEY_ENTER);key(KEY_ENTER);
    check(app.page==N_READER,"masked password editor can authenticate supplementary characters");
    login("admin");
    char frame[512],name[32];static char buffer[80000];
    for (int page=0;page<N_PAGE_COUNT;++page) {
        cl_session_clear(&app.session);
        if (page>=N_ADMIN) cl_auth_login(&app.store,(page==N_READER || (page>=N_MY_LOANS && page<=N_MY_FINES))?"reader001":"admin",
            (page==N_READER || (page>=N_MY_LOANS && page<=N_MY_FINES))?"reader123":"admin123",&app.session);
        cl_app_enter(&app,(NextPage)page);cl_app_scene(&app);
        cl_int_format(name,32,page);pathname(frame,"page-");text_append(frame,512,name);text_append(frame,512,".ans");
        FILE *f=fopen(frame,"wb");int ok=f && cl_terminal_draw(f,1);if (f) {cl_terminal_reset(f);if (fclose(f)) ok=0;}
        pathname(frame,"grid.txt");f=fopen(frame,"wb+");ok=ok && f && cl_terminal_text(f);int rows=0;
        if (f) {
            rewind(f);while (fgets(buffer,80000,f)) {int n=(int)strlen(buffer);while(n && (buffer[n-1]=='\r'||buffer[n-1]=='\n')) buffer[--n]=0;
                if (utf8_width(buffer)!=120) ok=0;
                ++rows;}
            if (fclose(f)) ok=0;
        }
        check(ok && rows==35,"each of seventeen pages fits 120x35 valid UTF-8 grid");
    }
    login("admin");cl_app_enter(&app,N_READERS);key(KEY_ENTER);cl_app_scene(&app);
    pathname(frame,"reader-detail.txt");FILE *f=fopen(frame,"wb+");int hidden=f && cl_terminal_text(f);
    if (f) {rewind(f);size_t n=fread(buffer,1,sizeof(buffer)-1,f);buffer[n]=0;
        for (int i=0;i<app.store.table[0].count;++i) if (strstr(buffer,app.store.table[0].rows[i].field[3]) ||
            strstr(buffer,app.store.table[0].rows[i].field[4])) hidden=0;
        if (fclose(f)) hidden=0;}
    check(hidden,"reader detail does not disclose salts or password hashes");
    key(KEY_ESC);text_copy(app.search.text,512,app.store.table[0].rows[0].field[3]);cl_app_rebuild(&app);
    check(app.match_count==0,"reader search excludes salts and password hashes");
    key(KEY_QUIT);check(!app.running,"Ctrl+Q exits application loop");cl_app_dispose(&app);
}
static void test_scrollbar(void) {
    char file[512],line[1024];pathname(file,"scrollbar-grid.txt");
    int counts[]={8,24,24,24},selected[]={0,0,8,23};
    int first_expected[]={-1,13,18,23},last_expected[]={-1,17,22,27};
    const char *names[]={"short lists hide the scrollbar","first page thumb starts at the top","middle page thumb moves down","final page thumb reaches the bottom"};
    for (int i=0;i<4;++i) {
        cl_ui_fill(0,0,120,35,BG);cl_ui_scrollbar(114,13,15,selected[i],counts[i],8);
        FILE *f=fopen(file,"wb+");int ok=f && cl_terminal_text(f),first=-1,last=-1,row=0;
        if (f) {
            rewind(f);while (fgets(line,1024,f)) {
                if (strstr(line,"┃")) {if (first<0) first=row;last=row;}
                ++row;
            }
            if (fclose(f)) ok=0;
        }
        check(ok && row==35 && first==first_expected[i] && last==last_expected[i],names[i]);
    }
}
int main(int argc,char **argv) {
    if (argc==3 && !strcmp(argv[1],"--restart")) {
        ClStore s={0};ClSession r={0};int ok=cl_store_open(&s,argv[2]) &&
            cl_auth_login(&s,"reader001","reader123",&r)==CL_AUTH_OK && s.table[2].count==1 && book(&s,1,5)==2;
        cl_store_dispose(&s);return ok?0:2;
    }
    if (argc!=2 || strstr(argv[1],"build/application/run-")!=argv[1]) return 2;
    text_copy(root,512,argv[1]);test_circulation();test_limits();test_queue();test_failure();test_ui();test_scrollbar();
    printf("\n%d passed, %d failed\n",passed,failed);return failed?1:0;
}
