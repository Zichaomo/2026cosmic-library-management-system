#ifndef COSMIC_MIGRATION_AUTH_H
#define COSMIC_MIGRATION_AUTH_H
#include "migration/storage.h"

typedef struct { int authenticated,role,reader_id;char username[96]; } ClSession;
enum { CL_ROLE_NONE,CL_ROLE_ADMIN,CL_ROLE_READER };
enum { CL_AUTH_OK,CL_AUTH_NOT_FOUND,CL_AUTH_WRONG_PASSWORD,CL_AUTH_FROZEN,
       CL_AUTH_STORAGE_ERROR };
typedef struct { const char *number,*name,*password,*confirmation,*phone; } ClRegistration;
enum { CL_REGISTER_OK,CL_REGISTER_EMPTY,CL_REGISTER_BAD_NUMBER,
       CL_REGISTER_BAD_PHONE,CL_REGISTER_WEAK_PASSWORD,CL_REGISTER_MISMATCH,
       CL_REGISTER_DUPLICATE,CL_REGISTER_STORAGE_ERROR,CL_REGISTER_UNCERTAIN,
       CL_REGISTER_PENDING_EXPORT };

int cl_password_hash(const char *password,const char *salt,char out[17]);
int cl_password_verify(const char *password,const char *salt,const char *expected);
int cl_password_create(const char *password,char salt[33],char hash[17]);
void cl_session_clear(ClSession *session);
int cl_auth_login(const ClStore *store,const char *username,const char *password,ClSession *session);
int cl_auth_register(ClStore *store,const ClRegistration *request);
int cl_auth_toggle_frozen(ClStore *store,int reader_id);
#endif
