#include "core/auth.h"

#include <string.h>

typedef struct {
    const wchar_t *username;
    const wchar_t *password;
    UserRole role;
    bool frozen;
} SeedAccount;

static const SeedAccount SEED_ACCOUNTS[] = {
    {L"admin", L"admin123", USER_ROLE_ADMIN, false},
    {L"reader001", L"reader123", USER_ROLE_READER, false}
};

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
    if (username == NULL || password == NULL) {
        return AUTH_RESULT_ACCOUNT_NOT_FOUND;
    }

    const size_t account_count =
        sizeof(SEED_ACCOUNTS) / sizeof(SEED_ACCOUNTS[0]);
    for (size_t index = 0; index < account_count; ++index) {
        const SeedAccount *account = &SEED_ACCOUNTS[index];
        if (wcscmp(username, account->username) != 0) {
            continue;
        }
        if (account->frozen) {
            return AUTH_RESULT_ACCOUNT_FROZEN;
        }
        if (wcscmp(password, account->password) != 0) {
            return AUTH_RESULT_WRONG_PASSWORD;
        }
        if (session != NULL) {
            auth_session_clear(session);
            session->authenticated = true;
            session->role = account->role;
            wcsncpy(session->username, username, 31);
            session->username[31] = L'\0';
        }
        return AUTH_RESULT_SUCCESS;
    }

    return AUTH_RESULT_ACCOUNT_NOT_FOUND;
}
