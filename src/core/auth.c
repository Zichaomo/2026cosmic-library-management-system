#include "core/auth.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wctype.h>

#include "utils/hash.h"

static bool is_empty(const wchar_t *text) {
    return text == NULL || text[0] == L'\0';
}
static bool reader_number_is_valid(const wchar_t *reader_no) {
    if (reader_no == NULL) {
        return false;
    }

    const size_t length = wcslen(reader_no);
    if (length < 3 || length >= READER_NUMBER_CAPACITY) {
        return false;
    }
    for (size_t index = 0; index < length; ++index) {
        const wchar_t character = reader_no[index];
        if (!iswalnum(character) && character != L'_' &&
            character != L'-') {
            return false;
        }
    }
    return true;
}

static void create_session(
    UserSession *session,
    const wchar_t *username,
    UserRole role
) {
    if (session == NULL) {
        return;
    }
    auth_session_clear(session);
    session->authenticated = true;
    session->role = role;
    wcsncpy(session->username, username, 31);
    session->username[31] = L'\0';
}

void auth_session_clear(UserSession *session) {
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
}

AuthResult auth_login(
    const ReaderRepository *readers,
    const wchar_t *username,
    const wchar_t *password,
    UserSession *session
) {
    if (username == NULL || password == NULL) {
        return AUTH_RESULT_ACCOUNT_NOT_FOUND;
    }

    if (wcscmp(username, L"admin") == 0) {
        if (wcscmp(password, L"admin123") != 0) {
            return AUTH_RESULT_WRONG_PASSWORD;
        }
        create_session(session, username, USER_ROLE_ADMIN);
        return AUTH_RESULT_SUCCESS;
    }

    const ReaderAccount *reader = reader_repository_find_by_number(
        readers, username
    );
    if (reader == NULL || reader->status == READER_STATUS_CANCELLED) {
        return AUTH_RESULT_ACCOUNT_NOT_FOUND;
    }
    if (reader->status == READER_STATUS_FROZEN) {
        return AUTH_RESULT_ACCOUNT_FROZEN;
    }
    if (!password_hash_verify(
            password, reader->salt, reader->password_hash
        )) {
        return AUTH_RESULT_WRONG_PASSWORD;
    }

    create_session(session, reader->reader_no, USER_ROLE_READER);
    return AUTH_RESULT_SUCCESS;
}

RegisterResult auth_register_reader(
    ReaderRepository *readers,
    const ReaderRegistrationRequest *request
) {
    if (readers == NULL || request == NULL ||
        is_empty(request->reader_no) || is_empty(request->name) ||
        is_empty(request->password) ||
        is_empty(request->password_confirmation) ||
        is_empty(request->contact)) {
        return REGISTER_RESULT_EMPTY_FIELD;
    }
    if (!reader_number_is_valid(request->reader_no)) {
        return REGISTER_RESULT_INVALID_READER_NO;
    }
    if (wcslen(request->name) >= READER_NAME_CAPACITY ||
        wcslen(request->contact) >= READER_CONTACT_CAPACITY) {
        return REGISTER_RESULT_EMPTY_FIELD;
    }
    if (wcslen(request->password) < 6) {
        return REGISTER_RESULT_WEAK_PASSWORD;
    }
    if (wcscmp(
            request->password, request->password_confirmation
        ) != 0) {
        return REGISTER_RESULT_PASSWORD_MISMATCH;
    }
    if (reader_repository_find_by_number(
            readers, request->reader_no
        ) != NULL) {
        return REGISTER_RESULT_DUPLICATE_READER_NO;
    }

    ReaderAccount account;
    memset(&account, 0, sizeof(account));
    account.id = reader_repository_next_id(readers);
    account.status = READER_STATUS_NORMAL;
    wcsncpy(
        account.reader_no, request->reader_no, READER_NUMBER_CAPACITY - 1
    );
    wcsncpy(account.name, request->name, READER_NAME_CAPACITY - 1);
    wcsncpy(
        account.contact, request->contact, READER_CONTACT_CAPACITY - 1
    );

    if (!password_hash_create(
            request->password, account.salt, account.password_hash
        )) {
        return REGISTER_RESULT_STORAGE_ERROR;
    }

    const time_t now = time(NULL);
    struct tm local_time;
    if (now == (time_t)-1 || localtime_s(&local_time, &now) != 0 ||
        strftime(
            account.created_at,
            sizeof(account.created_at),
            "%Y-%m-%d %H:%M:%S",
            &local_time
        ) == 0) {
        return REGISTER_RESULT_STORAGE_ERROR;
    }

    return reader_repository_add(readers, &account)
        ? REGISTER_RESULT_SUCCESS
        : REGISTER_RESULT_STORAGE_ERROR;
}
