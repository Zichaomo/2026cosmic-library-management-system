#include "core/auth.h"

#include <string.h>

void auth_session_clear(UserSession *session) {
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
}

AuthResult auth_login(
    const wchar_t *username,
    const wchar_t *password,
    UserSession *session
) {
    if (username == NULL || wcscmp(username, L"admin") != 0) {
        return AUTH_RESULT_ACCOUNT_NOT_FOUND;
    }
    if (password == NULL || wcscmp(password, L"admin123") != 0) {
        return AUTH_RESULT_WRONG_PASSWORD;
    }

    if (session != NULL) {
        auth_session_clear(session);
        session->authenticated = true;
        session->role = USER_ROLE_ADMIN;
        wcsncpy(session->username, username, 31);
        session->username[31] = L'\0';
    }
    return AUTH_RESULT_SUCCESS;
}

