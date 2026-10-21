#include "preview.h"
#include <string.h>

static int passed, failed;
static Preview p;

static void check(int condition,const char *name) {
    printf("[%s] %s\n",condition?"PASS":"FAIL",name);
    if (condition) ++passed; else ++failed;
}
static void key(Key key_value) {
    Event e={KEY_NONE,{0}}; e.key=key_value; preview_event(&p,e);
}
static void type(const char *s) {
    while (*s) {
        Event e={KEY_TEXT,{0}};
        unsigned cp; int n=utf8_decode(s,&cp);
        if (n<1) return;
        for (int i=0;i<n;++i) e.text[i]=s[i];
        preview_event(&p,e); s+=n;
    }
}
static void set(Edit *e,const char *s) {
    text_copy(e->text,TEXT_CAP,s); e->cursor=(int)strlen(s);
}

static void test_text(void) {
    unsigned cp=0;
    Edit e={{0},0}; char out[64]; int value;
    check(utf8_decode("中",&cp)==3 && cp==0x4e2d,"decode Chinese UTF-8");
    check(utf8_decode("\xe4\xb8",&cp)<0,"reject truncated UTF-8");
    check(utf8_decode("\xc0\xaf",&cp)<0,"reject overlong UTF-8");
    check(utf8_decode("\xed\xa0\x80",&cp)<0,"reject surrogate codepoint");
    check(utf8_decode("\xf4\x90\x80\x80",&cp)<0,"reject codepoint above Unicode limit");
    check(utf8_width("ab中文")==6,"ASCII plus Chinese display columns");
    check(utf8_width("\033[2J")<0,"reject terminal control injection");
    check(utf8_width("a\xcc\x81")<0,"unsupported combining sequence rejected");
    check(utf8_width("\xf0\x9f\x98\x80")<0,"unsupported emoji rejected");
    check(text_clip(out,64,"中文书名超长",7) && !strcmp(out,"中文..."),"truncate on character boundary");
    check(text_clip(out,64,"中文",1) && !out[0],"one-column field does not split Chinese");
    check(!text_copy(out,3,"中文"),"bounded copy fails without overflow");
    check(edit_insert(&e,"中文") && e.cursor==6,"insert Chinese into editor");
    edit_key(&e,KEY_LEFT); edit_insert(&e,"A");
    check(!strcmp(e.text,"中A文") && e.cursor==4,"insert in middle of Chinese text");
    edit_key(&e,KEY_BACKSPACE);
    check(!strcmp(e.text,"中文") && e.cursor==3,"backspace ASCII in mixed input");
    edit_key(&e,KEY_DELETE);
    check(!strcmp(e.text,"中") && e.cursor==3,"delete entire Chinese codepoint");
    edit_key(&e,KEY_HOME); edit_key(&e,KEY_DELETE);
    check(!e.text[0] && e.cursor==0,"home then delete whole first character");
    edit_insert(&e,"中A");edit_key(&e,KEY_HOME);edit_key(&e,KEY_RIGHT);
    check(e.cursor==3,"right crosses UTF-8 codepoint");
    edit_key(&e,KEY_END);edit_key(&e,KEY_BACKSPACE);edit_key(&e,KEY_BACKSPACE);
    check(!e.text[0],"backspace clears mixed input without fragments");
    for (int i=0;i<TEXT_CAP-1;++i) e.text[i]='x';
    e.text[TEXT_CAP-1]=0;e.cursor=TEXT_CAP-1;
    check(!edit_insert(&e,"中") && strlen(e.text)==TEXT_CAP-1,"full field refuses multi-byte insert");
    check(positive_number("2147483647",&value) && value==2147483647,"maximum accepted integer");
    check(!positive_number("2147483648",&value),"reject integer overflow");
    check(!positive_number("0",&value) && !positive_number("-1",&value) &&
        !positive_number("1x",&value) && !positive_number("",&value),"reject invalid copies");
}

static void test_flow(void) {
    preview_init(&p,PAGE_LOGIN);
    type("admn");key(KEY_LEFT);type("i");key(KEY_END);key(KEY_TAB);
    check(!strcmp(p.login[0].text,"admin") && p.focus==1,"edit account then Tab to password");
    type("wrong");key(KEY_ENTER);key(KEY_ENTER);
    check(p.page==PAGE_LOGIN && strstr(p.message,"错误"),"failed demo login stays on form");
    p.focus=1;set(&p.login[1],"admin123");key(KEY_ENTER);key(KEY_ENTER);
    check(p.page==PAGE_BOOKS && !p.login[1].text[0],"demo login clears password on success");
    key(KEY_PGDN);check(p.selected==PAGE_ROWS,"page down reaches second page");
    key(KEY_END);check(p.selected==19,"end reaches final sample");
    key(KEY_DOWN);check(p.selected==19,"list lower boundary clamped");
    key(KEY_HOME);key(KEY_UP);check(p.selected==0,"list upper boundary clamped");
    type("K");type("中文");
    check(p.searching && p.match_count==1,"live Chinese search matches long sample title");
    type("C");check(p.page==PAGE_BOOKS && p.searching,"C inside search cannot open new-book form");
    key(KEY_BACKSPACE);key(KEY_ENTER);key(KEY_ENTER);
    check(p.detail,"Enter opens selected-book detail");key(KEY_ESC);
    type("K");set(&p.search,"not-present");preview_filter(&p);
    check(p.match_count==0,"search empty state");key(KEY_ENTER);
    key(KEY_PGDN);key(KEY_ENTER);type("Z");type("T");
    check(p.selected==0 && !p.detail,"empty list actions remain safe");
    set(&p.search,"");preview_filter(&p);type("Z");
    check(p.books[0].copies==4 && p.books[0].available==3,"increase copy in memory only");
    type("T");check(p.books[0].suspended,"toggle sample book suspended state");
    type("C");check(p.page==PAGE_FORM && !p.form[0].text[0],"C opens clean form");
    type("DEMO-NEW");key(KEY_TAB);type("中文图书");key(KEY_TAB);type("张老师");key(KEY_TAB);
    key(KEY_HOME);key(KEY_DELETE);type("0");key(KEY_TAB);key(KEY_ENTER);
    check(p.page==PAGE_FORM && p.focus==3 && strstr(p.message,"错误"),"invalid copies focus field");
    check(!strcmp(p.form[1].text,"中文图书") && !strcmp(p.form[2].text,"张老师"),"validation preserves Chinese form data");
    key(KEY_HOME);key(KEY_DELETE);type("2");key(KEY_TAB);key(KEY_ENTER);
    check(p.page==PAGE_BOOKS && p.count==21 && p.selected==20,"valid form adds memory sample and selects it");
    check(!strcmp(p.books[20].title,"中文图书") && p.books[20].copies==2,"saved sample content matches input");
    type("C");type("unsaved");key(KEY_ESC);
    check(p.confirm_cancel && p.page==PAGE_FORM,"dirty form requires discard choice");
    type("N");check(!p.confirm_cancel && !strcmp(p.form[0].text,"unsaved"),"cancel discard preserves input");
    key(KEY_ESC);type("Y");check(p.page==PAGE_BOOKS && !p.confirm_cancel,"confirmed discard returns to list");
    key(KEY_F1);check(p.help,"F1 opens help");key(KEY_ESC);check(!p.help,"Escape closes help");
    key(KEY_ESC);check(p.page==PAGE_LOGIN && !p.login[1].text[0],"logout returns to login without password");
    key(KEY_BACKTAB);check(p.focus==3,"backtab wraps to last control");
    key(KEY_QUIT);check(!p.running,"Ctrl-Q exit event");
}

static void test_render(void) {
    static char bytes[80000];
    preview_init(&p,PAGE_LOGIN);set(&p.login[1],"SECRET-NOT-VISIBLE");
    preview_scene(&p);
    FILE *file=fopen("build/preview-test-frame.ans","wb+");
    if (!file) { check(0,"open isolated render fixture");return; }
    check(preview_draw(file,1),"render full ANSI frame");
    long initial=ftell(file);
    check(preview_draw(file,0) && ftell(file)==initial,"unchanged frame emits no bytes");
    type("中");preview_scene(&p);preview_draw(file,0);
    long changed=ftell(file);
    check(changed>initial && changed-initial<initial/2,"edit updates dirty rows instead of full screen");
    check(preview_draw(file,1) && ftell(file)-changed>=initial,
        "explicit refresh redraws unchanged cells after terminal resize");
    long repair_start=ftell(file);
    check(preview_draw(file,2) && ftell(file)-repair_start>=initial-20,
        "interactive repaint emits the entire unchanged frame after terminal damage");
    fseek(file,repair_start,SEEK_SET);
    size_t repair_n=fread(bytes,1,sizeof(bytes)-1,file);bytes[repair_n]=0;
    check(!strstr(bytes,"\033[2J"),"interactive repair avoids clearing between keystrokes");
    fseek(file,0,SEEK_END);
    rewind(file);size_t n=fread(bytes,1,sizeof(bytes)-1,file);bytes[n]=0;
    check(strstr(bytes,"\033[?7l")!=NULL,"fixed canvas disables auto wrap in a narrow viewport");
    check(strstr(bytes,"38;2;") && strstr(bytes,"48;2;"),"frame contains ANSI truecolor");
    check(!strstr(bytes,"SECRET-NOT-VISIBLE"),"masked password never appears in terminal frame");
    check(strstr(bytes,"中")!=NULL,"Chinese editor content included as UTF-8");
    check(fclose(file)==0,"close isolated render fixture");
    for (int scene=0;scene<6;++scene) {
        preview_init(&p,(Page)(scene<3?scene:PAGE_BOOKS));
        if (scene==3) { p.selected=19;p.detail=1; }
        if (scene==4) p.help=1;
        if (scene==5) { p.page=PAGE_FORM;p.confirm_cancel=1; }
        preview_scene(&p);
        file=fopen("build/preview-test-grid.txt","wb+");
        if (!file) {check(0,"open grid fixture");continue;}
        int valid=preview_dump_text(file),rows=0;
        rewind(file);
        while (fgets(bytes,(int)sizeof(bytes),file)) {
            int len=(int)strlen(bytes);
            while (len && (bytes[len-1]=='\r'||bytes[len-1]=='\n')) bytes[--len]=0;
            if (utf8_width(bytes)!=PREVIEW_W) valid=0;
            ++rows;
        }
        check(valid && rows==PREVIEW_H,scene<3?"page output fits 120x35 character grid":
            "overlay crossing Chinese cells preserves 120-column rows");
        fclose(file);
    }
}

int main(void) {
    test_text();test_flow();test_render();
    printf("\n%d passed, %d failed\n",passed,failed);
    return failed?1:0;
}
