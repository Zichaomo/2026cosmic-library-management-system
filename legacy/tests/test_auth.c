#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "core/auth.h"
#include "data/reader_repository.h"

static int passed_count = 0;
static int failed_count = 0;

static void check(bool condition, const char *test_name) {
    if (condition) {
        ++passed_count;
        printf("[PASS] %s\n", test_name);
    } else {
        ++failed_count;
        printf("[FAIL] %s\n", test_name);
    }
}

static void clean_test_data(void) {
    DeleteFileW(L"tests\\tmp_data\\readers.csv.tmp");
    DeleteFileW(L"tests\\tmp_data\\readers.csv");
    RemoveDirectoryW(L"tests\\tmp_data");
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    clean_test_data();

    ReaderRepository readers;
    check(
        reader_repository_init(&readers, L"tests\\tmp_data"),
        "initialize empty reader repository"
    );

    UserSession session;
    check(
        auth_login(&readers, L"admin", L"admin123", &session) ==
            AUTH_RESULT_SUCCESS &&
            session.role == USER_ROLE_ADMIN,
        "admin login succeeds and gets admin role"
    );
    check(
        auth_login(&readers, L"admin", L"wrong-password", &session) ==
            AUTH_RESULT_WRONG_PASSWORD,
        "admin wrong password is rejected"
    );

    const ReaderRegistrationRequest registration = {
        L"2026001",
        L"测试读者",
        L"reader123",
        L"reader123",
        L"13800138000"
    };
    ReaderRegistrationRequest invalid_contact = registration;
    invalid_contact.contact = L"138001380003";
    check(auth_register_reader(&readers, &invalid_contact) ==
        REGISTER_RESULT_INVALID_CONTACT && readers.count == 0,
        "reject twelve-digit phone without saving account");
    invalid_contact.contact = L"15303347256.";
    check(auth_register_reader(&readers, &invalid_contact) ==
        REGISTER_RESULT_INVALID_CONTACT && readers.count == 0,
        "reject trailing punctuation in phone");
    invalid_contact.contact = L"12800138000";
    check(auth_register_reader(&readers, &invalid_contact) ==
        REGISTER_RESULT_INVALID_CONTACT && readers.count == 0,
        "reject invalid mainland mobile prefix");
    check(!auth_mobile_number_is_valid(L"13800A38000") &&
        !auth_mobile_number_is_valid(L"１３８００１３８０００") &&
        !auth_mobile_number_is_valid(L"1380013800") &&
        !auth_mobile_number_is_valid(NULL) &&
        auth_mobile_number_is_valid(L"19900138000"),
        "phone validation accepts only eleven ASCII digits with valid prefix");
    ReaderRegistrationRequest reserved_account = registration;
    reserved_account.reader_no = L"admin";
    check(auth_register_reader(&readers, &reserved_account) ==
        REGISTER_RESULT_DUPLICATE_READER_NO && readers.count == 0,
        "admin account is reserved and cannot register as reader");
    check(
        auth_register_reader(&readers, &registration) ==
            REGISTER_RESULT_SUCCESS,
        "reader registration succeeds"
    );
    check(readers.count == 1, "registered reader is added to repository");
    check(
        strlen(readers.items[0].password_hash) == 16 &&
            strlen(readers.items[0].salt) == 32 &&
            strcmp(readers.items[0].password_hash, "reader123") != 0,
        "password is stored as salted hash"
    );
    check(
        auth_register_reader(&readers, &registration) ==
            REGISTER_RESULT_DUPLICATE_READER_NO,
        "duplicate reader number is rejected"
    );

    ReaderRegistrationRequest mismatch = registration;
    mismatch.reader_no = L"2026002";
    mismatch.password_confirmation = L"different";
    check(
        auth_register_reader(&readers, &mismatch) ==
            REGISTER_RESULT_PASSWORD_MISMATCH,
        "password confirmation mismatch is rejected"
    );
    check(
        auth_login(&readers, L"2026001", L"wrong-password", &session) ==
            AUTH_RESULT_WRONG_PASSWORD,
        "reader wrong password is rejected"
    );
    check(
        auth_login(&readers, L"2026001", L"reader123", &session) ==
            AUTH_RESULT_SUCCESS &&
            session.role == USER_ROLE_READER,
        "registered reader can log in"
    );

    reader_repository_dispose(&readers);
    check(
        reader_repository_init(&readers, L"tests\\tmp_data") &&
            readers.count == 1,
        "reader data reloads from CSV after restart"
    );
    check(
        auth_login(&readers, L"2026001", L"reader123", &session) ==
            AUTH_RESULT_SUCCESS &&
            session.role == USER_ROLE_READER,
        "reloaded reader can still log in"
    );

    reader_repository_dispose(&readers);
    clean_test_data();
    printf(
        "\nTest result: %d passed, %d failed.\n",
        passed_count,
        failed_count
    );
    return failed_count == 0 ? 0 : 1;
}

