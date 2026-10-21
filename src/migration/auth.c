#include "migration/auth.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

static int text_count(const char *s) {
    if (!s) return -1;
    int count=0;
    while (*s) {
        unsigned cp;int n=utf8_decode(s,&cp);if (n<1) return -1;
        s+=n;count+=cp>65535?2:1;
    }
    return count;
}
int cl_password_hash(const char *password,const char *salt,char out[17]) {
    u8 bytes[16];
    if (!password || !salt || !out || strlen(password)>=256 || text_count(password)<0 ||
        strlen(salt)!=32 || !cl_hex_decode(bytes,16,salt)) return 0;
    u64 hash=14695981039346656037ull;
    for (int i=0;i<16;++i) { hash^=bytes[i];hash*=1099511628211ull; }
    for (const unsigned char *p=(const unsigned char *)password;*p;++p) {
        hash^=*p;hash*=1099511628211ull;
    }
    return cl_u64_hex(out,17,hash);
}
int cl_password_verify(const char *password,const char *salt,const char *expected) {
    char hash[17];
    return expected && strlen(expected)==16 && cl_password_hash(password,salt,hash) && !strcmp(hash,expected);
}
int cl_password_create(const char *password,char salt[33],char hash[17]) {
    static int seeded;u8 bytes[16];char new_salt[33],new_hash[17];
    if (!password || !salt || !hash) return 0;
    if (!seeded) {
        time_t now=time(NULL);if (now==(time_t)-1) return 0;
        srand((unsigned)now);seeded=1;
    }
    for (int i=0;i<16;++i) bytes[i]=(u8)(rand()%256);
    if (!cl_hex_encode(new_salt,33,bytes,16) || !cl_password_hash(password,new_salt,new_hash)) return 0;
    text_copy(salt,33,new_salt);text_copy(hash,17,new_hash);return 1;
}
void cl_session_clear(ClSession *session) {
    if (session) { ClSession empty={0};*session=empty; }
}
int cl_auth_login(const ClStore *s,const char *username,const char *password,ClSession *session) {
    cl_session_clear(session);
    if (!s || s->blocked || s->generation<1) return CL_AUTH_STORAGE_ERROR;
    if (!username || !password || !session) return CL_AUTH_NOT_FOUND;
    int role=CL_ROLE_NONE,id=0;
    if (!strcmp(username,"admin")) {
        if (strcmp(password,"admin123")) return CL_AUTH_WRONG_PASSWORD;
        role=CL_ROLE_ADMIN;
    } else {
        const ClRow *found=NULL;
        for (int i=0;i<s->table[CL_READERS].count;++i)
            if (!strcmp(username,s->table[CL_READERS].rows[i].field[1])) found=&s->table[CL_READERS].rows[i];
        if (!found || !strcmp(found->field[7],"注销")) return CL_AUTH_NOT_FOUND;
        if (!strcmp(found->field[7],"冻结")) return CL_AUTH_FROZEN;
        if (!cl_password_verify(password,found->field[4],found->field[3])) return CL_AUTH_WRONG_PASSWORD;
        positive_number(found->field[0],&id);role=CL_ROLE_READER;
    }
    if (!text_copy(session->username,96,username)) return CL_AUTH_NOT_FOUND;
    session->authenticated=1;session->role=role;session->reader_id=id;return CL_AUTH_OK;
}
static int number_valid(const char *s) {
    int count=text_count(s);
    if (count<3 || count>=32) return 0;
    while (*s) {
        unsigned cp;int n=utf8_decode(s,&cp);
        if (!((cp>='a' && cp<='z') || (cp>='A' && cp<='Z') ||
              (cp>='0' && cp<='9') || cp=='_' || cp=='-' || (cp>=0x4e00 && cp<=0x9fff))) return 0;
        s+=n;
    }
    return 1;
}
static int phone_valid(const char *s) {
    if (!s || strlen(s)!=11 || s[0]!='1' || s[1]<'3' || s[1]>'9') return 0;
    for (int i=2;i<11;++i) if (s[i]<'0' || s[i]>'9') return 0;
    return 1;
}
int cl_auth_register(ClStore *s,const ClRegistration *r) {
    if (!s || s->blocked || s->generation<1) return CL_REGISTER_STORAGE_ERROR;
    if (!r || !r->number || !*r->number || !r->name || !*r->name || !r->password ||
        !*r->password || !r->confirmation || !*r->confirmation || !r->phone || !*r->phone) return CL_REGISTER_EMPTY;
    if (!number_valid(r->number)) return CL_REGISTER_BAD_NUMBER;
    if (!strcmp(r->number,"admin")) return CL_REGISTER_DUPLICATE;
    if (!phone_valid(r->phone)) return CL_REGISTER_BAD_PHONE;
    int length=text_count(r->name);
    if (length<1 || length>=64 || utf8_width(r->name)<0) return CL_REGISTER_EMPTY;
    if (text_count(r->password)<6 || strlen(r->password)>=256) return CL_REGISTER_WEAK_PASSWORD;
    if (strcmp(r->password,r->confirmation)) return CL_REGISTER_MISMATCH;
    int id=0;ClTable *t=&s->table[CL_READERS];
    for (int i=0;i<t->count;++i) {
        if (!strcmp(t->rows[i].field[1],r->number)) return CL_REGISTER_DUPLICATE;
        int stored;positive_number(t->rows[i].field[0],&stored);if (stored>id) id=stored;
    }
    if (id==CL_INT_MAX) return CL_REGISTER_STORAGE_ERROR;
    ClRow row={0};ClDateTime now;row.count=9;
    if (!cl_int_format(row.field[0],512,id+1) || !text_copy(row.field[1],512,r->number) ||
        !text_copy(row.field[2],512,r->name) || !cl_password_create(r->password,row.field[4],row.field[3]) ||
        !text_copy(row.field[5],512,r->phone) || !text_copy(row.field[7],512,"正常") ||
        !cl_datetime_now(&now) || !cl_datetime_format(row.field[8],512,now) || !cl_table_append(t,&row))
        return CL_REGISTER_STORAGE_ERROR;
    int result=cl_store_commit(s);
    if (result==CL_STORE_FAILED) { --t->count;return CL_REGISTER_STORAGE_ERROR; }
    if (result==CL_STORE_UNCERTAIN) return CL_REGISTER_UNCERTAIN;
    return result==CL_STORE_PENDING_EXPORT?CL_REGISTER_PENDING_EXPORT:CL_REGISTER_OK;
}
int cl_auth_toggle_frozen(ClStore *s,int id) {
    if (!s || s->blocked || s->generation<1) return CL_STORE_FAILED;
    for (int i=0;i<s->table[CL_READERS].count;++i) {
        ClRow *r=&s->table[CL_READERS].rows[i];int current;positive_number(r->field[0],&current);
        if (current!=id || !strcmp(r->field[7],"注销")) continue;
        int frozen=!strcmp(r->field[7],"冻结");
        text_copy(r->field[7],512,frozen?"正常":"冻结");
        int result=cl_store_commit(s);
        if (result==CL_STORE_FAILED) text_copy(r->field[7],512,frozen?"冻结":"正常");
        return result;
    }
    return CL_STORE_FAILED;
}
