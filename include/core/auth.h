#ifndef COSMIC_LIBRARY_CORE_AUTH_H
#define COSMIC_LIBRARY_CORE_AUTH_H

#include <stdbool.h>
#include <wchar.h>

#include "data/reader_repository.h"

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

typedef struct {
    const wchar_t *reader_no;
    const wchar_t *name;
    const wchar_t *password;
    const wchar_t *password_confirmation;
    const wchar_t *contact;
} ReaderRegistrationRequest;

typedef enum {
    REGISTER_RESULT_SUCCESS = 0,
    REGISTER_RESULT_EMPTY_FIELD,
    REGISTER_RESULT_INVALID_READER_NO,
    REGISTER_RESULT_WEAK_PASSWORD,
    REGISTER_RESULT_PASSWORD_MISMATCH,
    REGISTER_RESULT_DUPLICATE_READER_NO,
    REGISTER_RESULT_STORAGE_ERROR
} RegisterResult;

void auth_session_clear(UserSession *session);
AuthResult auth_login(
    const ReaderRepository *readers,
    const wchar_t *username,
    const wchar_t *password,
    UserSession *session
);
RegisterResult auth_register_reader(
    ReaderRepository *readers,
    const ReaderRegistrationRequest *request
);

#endif
