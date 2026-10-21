#include "utils/foundation.h"
#include <string.h>
#include <time.h>

static int passed,failed;
static void check(int ok,const char *name) {
    printf("[%s] %s\n",ok?"PASS":"FAIL",name);
    if (ok) ++passed;else ++failed;
}
static int date_is(ClDate d,int y,int m,int day) {
    return d.year==y && d.month==m && d.day==day;
}
static void test_text(void) {
    char out[64]="keep",small[4]="abc";int value=37;
    check(!text_copy(small,3,"abc") && !strcmp(small,"abc"),"copy failure preserves destination");
    check(text_copy(small,4,"abc") && !text_copy(small,0,""),"copy exact capacity including NUL");
    check(!text_copy(NULL,4,"a") && !text_copy(out,64,NULL),"copy rejects NULL arguments");
    text_copy(out,64,"中文");
    check(text_append(out,64,out) && !strcmp(out,"中文中文"),"append supports self alias with UTF-8");
    check(!text_append(small,4,"x") && !strcmp(small,"abc"),"append failure preserves destination");
    char unterminated[3]={'a','b','c'};
    check(!text_append(unterminated,3,"x") && unterminated[2]=='c',"append refuses missing destination NUL");
    text_copy(out,64,"keep");
    check(!text_clip(out,4,"中文超长",7) && !strcmp(out,"keep"),"clip capacity failure is atomic");
    check(text_clip(out,10,"中文超长",7) && !strcmp(out,"中文..."),"clip reserves ellipsis and terminator");
    check(text_clip(out,64,"中文",1) && !out[0] && text_clip(out,64,"abc",0) && !out[0],"narrow clip does not split codepoint");
    text_copy(out,64,"keep");
    check(!text_clip(out,64,"\xe4\xb8",9) && !strcmp(out,"keep"),"clip rejects malformed UTF-8 without mutation");
    check(cl_int_parse("2147483647",&value) && value==CL_INT_MAX &&
          cl_int_parse("-2147483648",&value) && value==CL_INT_MIN,"parse both signed integer limits");
    value=37;
    check(!cl_int_parse("2147483648",&value) && !cl_int_parse("-2147483649",&value) && value==37,"integer overflow preserves output");
    check(!cl_int_parse("-",&value) && !cl_int_parse("+1",&value) &&
          !cl_int_parse(" 1",&value) && !cl_int_parse("1x",&value) &&
          !cl_int_parse("",&value) && !cl_int_parse(NULL,&value),"strict integer grammar");
    check(cl_int_format(out,64,CL_INT_MIN) && !strcmp(out,"-2147483648"),"format INT_MIN without negation overflow");
    check(!cl_int_format(small,3,100) && !strcmp(small,"abc"),"integer formatting failure preserves output");
    check(cl_int_format(out,2,0) && !strcmp(out,"0"),"format zero in minimum buffer");
    int roundtrip=1;
    for (int i=-10000;i<=10000;++i)
        if (!cl_int_format(out,64,i) || !cl_int_parse(out,&value) || value!=i) roundtrip=0;
    check(roundtrip,"20001 signed integers format and parse round-trip");
    check(positive_number("001",&value) && value==1 && !positive_number("-0",&value) &&
          !positive_number("0",&value) && !positive_number("2147483648",&value),"positive copies preserve previous grammar");
}
static void test_utf8(void) {
    unsigned cp=99;int offset=99;
    check(utf8_decode("中",&cp)==3 && cp==0x4e2d && utf8_width("A中文")==5,"decode and measure ASCII/Chinese");
    check(utf8_decode("\xc2\x80",&cp)==2 && utf8_decode("\xf4\x8f\xbf\xbf",&cp)==4,"valid UTF-8 encoding edges");
    const char *invalid[]={"\xc0\xaf","\xe0\x80\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xe4","\xe4\xb8","\x80"};
    int rejected=1;
    for (int i=0;i<7;++i) if (utf8_decode(invalid[i],&cp)>=0) rejected=0;
    check(rejected,"reject truncated overlong surrogate and out-of-range encodings");
    check(utf8_width("\033[2J")<0 && utf8_width("\xf0\x9f\x98\x80")<0 &&
          utf8_width("a\xcc\x81")<0,"reject controls emoji and combining marks in current UI scope");
    check(!utf8_boundary("中A文",1) && utf8_boundary("中A文",3) &&
          utf8_boundary("中A文",7) && !utf8_boundary("中A文",8),"byte boundaries include NUL but not continuation bytes");
    check(utf8_previous("中A文",7,&offset) && offset==4 &&
          utf8_next("中A文",0,&offset) && offset==3,"previous and next preserve whole UTF-8 characters");
    offset=99;
    check(!utf8_next("中A文",1,&offset) && !utf8_previous("中A文",0,&offset) &&
          !utf8_next("中A文",7,&offset) && offset==99,"invalid boundary movement leaves output unchanged");
    check(utf8_width(NULL)<0 && utf8_decode(NULL,&cp)<0 &&
          !utf8_boundary("a",-1) && utf8_columns(0xd800)<0,"reject NULL negative offset and invalid direct codepoint");
}
static void test_hex(void) {
    u8 bytes[256],decoded[256]={0};char hex[513],out[32]="keep";
    for (int i=0;i<256;++i) bytes[i]=(u8)i;
    int same=cl_hex_encode(hex,513,bytes,256) && cl_hex_decode(decoded,256,hex);
    for (int i=0;i<256;++i) if (decoded[i]!=bytes[i]) same=0;
    check(same && strlen(hex)==512,"all 256 byte values encode/decode round-trip");
    check(cl_hex_decode(decoded,2,"aAfF") && decoded[0]==170 && decoded[1]==255,"hex decoder accepts mixed case");
    decoded[0]=12;decoded[1]=34;
    check(!cl_hex_decode(decoded,2,"00xz") && decoded[0]==12 && decoded[1]==34,"invalid suffix does not partially decode");
    check(!cl_hex_decode(decoded,2,"abc") && !cl_hex_decode(decoded,1,"aabb"),"hex rejects odd length and insufficient capacity");
    check(!cl_hex_encode(out,4,bytes,2) && !strcmp(out,"keep"),"hex capacity failure preserves output");
    check(cl_hex_encode(out,1,NULL,0) && !out[0] && cl_hex_decode(NULL,0,""),"zero-byte hex has explicit empty representation");
    check(cl_u64_hex(out,32,0) && !strcmp(out,"0000000000000000"),"64-bit hash format retains leading zeros");
    check(cl_u64_hex(out,32,14695981039346656037ull) && !strcmp(out,"cbf29ce484222325"),"existing FNV basis matches 16-digit lowercase format");
    check(cl_u64_hex(out,32,(u64)-1) && !strcmp(out,"ffffffffffffffff"),"format maximum 64-bit value");
}
static void test_dates(void) {
    ClDate d={2026,10,21},out={7,7,7};int days=99;char text[32]="keep";
    check(cl_date_parse("2024-02-29",&out) && date_is(out,2024,2,29),"parse valid leap day");
    check(!cl_date_parse("1900-02-29",&out) && cl_date_parse("2000-02-29",&out),"Gregorian century leap exception");
    check(!cl_date_parse("2026-04-31",&out) && !cl_date_parse("2026-2-01",&out) &&
          !cl_date_parse("0000-01-01",&out) && !cl_date_parse("2026-01-01x",&out),"reject invalid dates and noncanonical syntax");
    check(cl_date_add((ClDate){2024,2,28},1,&out) && date_is(out,2024,2,29) &&
          cl_date_add(out,1,&out) && date_is(out,2024,3,1),"calendar addition across leap February");
    check(cl_date_add((ClDate){2026,12,31},1,&out) && date_is(out,2027,1,1) &&
          cl_date_add(out,-1,&out) && date_is(out,2026,12,31),"calendar addition and subtraction across year");
    check(cl_date_add((ClDate){2026,10,21},30,&out) && date_is(out,2026,11,20) &&
          cl_date_add(out,15,&out) && date_is(out,2026,12,5),"30-day borrowing and 15-day extension cross months");
    check(cl_date_difference((ClDate){2026,10,10},(ClDate){2026,10,9},&days) && days==1 &&
          cl_date_difference(d,d,&days) && days==0,"civil difference supports overdue day boundary");
    check(!cl_date_add(d,CL_INT_MAX,&out) && !cl_date_add(d,CL_INT_MIN,&out) &&
          !cl_date_add((ClDate){1,1,1},-1,&out) &&
          !cl_date_add((ClDate){9999,12,31},1,&out),"date offsets reject extreme overflow and range escape");
    check(cl_date_serial((ClDate){1970,1,1},&days) && days==719162 &&
          cl_date_serial((ClDate){9999,12,31},&days) && days==3652058,"known epoch and final civil serial anchors");
    int valid=1;
    for (int i=0;i<146097;++i) {
        int serial;ClDate current;
        if (!cl_date_from_serial(i,&current) || !cl_date_serial(current,&serial) || serial!=i ||
            !cl_date_format(text,32,current) || !cl_date_parse(text,&out) ||
            !date_is(out,current.year,current.month,current.day)) { valid=0;break; }
    }
    check(valid,"all 146097 days in a 400-year Gregorian cycle round-trip");
    text_copy(text,32,"keep");
    check(!cl_date_format(text,10,d) && !strcmp(text,"keep") &&
          !cl_date_format(text,32,(ClDate){2026,2,30}) && !strcmp(text,"keep"),"date formatting validates before writing");
    ClDateTime t;long long timestamp;
    check(cl_datetime_from_unix(0,&t) && date_is(t.date,1970,1,1) && t.hour==8,"Unix epoch maps to fixed UTC+8");
    check(cl_datetime_from_unix(-28801,&t) && date_is(t.date,1969,12,31) &&
          t.hour==23 && t.minute==59 && t.second==59,"negative timestamp uses floor division across midnight");
    check(cl_datetime_from_unix(-62135625600ll,&t) && date_is(t.date,1,1,1) &&
          t.hour==0 && cl_datetime_to_unix(t,&timestamp) && timestamp==-62135625600ll,"earliest supported Beijing instant");
    check(cl_datetime_from_unix(253402271999ll,&t) && date_is(t.date,9999,12,31) &&
          t.hour==23 && t.second==59 && cl_datetime_to_unix(t,&timestamp) && timestamp==253402271999ll,"latest supported Beijing instant");
    check(!cl_datetime_from_unix(-9223372036854775807ll-1,&t) &&
          !cl_datetime_from_unix(9223372036854775807ll,&t),"timestamp extremes rejected before UTC offset addition");
    check(cl_datetime_parse("2026-10-09 23:59:59",&t) && cl_datetime_to_unix(t,&timestamp) &&
          cl_datetime_from_unix(timestamp+1,&t) && date_is(t.date,2026,10,10) &&
          t.hour==0 && t.minute==0 && t.second==0,"one second crosses Beijing business midnight");
    check(cl_datetime_parse("2026-10-21 08:09:07",&t) && cl_datetime_format(text,32,t) &&
          !strcmp(text,"2026-10-21 08:09:07"),"timestamp CSV format stays YYYY-MM-DD HH:MM:SS");
    check(!cl_datetime_parse("2026-10-21 24:00:00",&t) &&
          !cl_datetime_parse("2026-10-21 08:60:00",&t) &&
          !cl_datetime_parse("2026-10-21 08:00:60",&t),"reject invalid clock components");
    check(cl_datetime_now(&t) && cl_datetime_format(text,32,t),"time() converts to a valid Beijing timestamp");
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--clock")) {
        time_t now=time(NULL);ClDateTime t;char text[20];
        if (now==(time_t)-1 || !cl_datetime_from_unix((long long)now,&t) ||
            !cl_datetime_format(text,20,t)) return 1;
        printf("%lld|%s\n",(long long)now,text);return 0;
    }
    test_text();test_utf8();test_hex();test_dates();
    printf("\n%d passed, %d failed\n",passed,failed);return failed?1:0;
}
