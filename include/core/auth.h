#ifndef COSMIC_LIBRARY_CORE_AUTH_H
#define COSMIC_LIBRARY_CORE_AUTH_H

#include <stdbool.h>
#include <wchar.h>

typedef enum {
    USER_ROLE_NONE = 0,
    USER_ROLE_ADMIN,
    USER_ROLE_READER
} UserRole;

typedef struct {
    bool authenticated;
    wchar_t username[32];
    UserRole role;
} UserSession;

typedef enum {
    AUTH_RESULT_SUCCESS = 0,
    AUTH_RESULT_ACCOUNT_NOT_FOUND,
    AUTH_RESULT_WRONG_PASSWORD,
    AUTH_RESULT_ACCOUNT_FROZEN
} AuthResult;

void auth_session_clear(UserSession *session);
AuthResult auth_login(
    const wchar_t *username,
    const wchar_t *password,
    UserSession *session
);

#endif

