/* 认证模块 - 业务逻辑层（登录/注册/会话） */

#include "auth.h"

#include <string.h>

#include "../data/repository.h"
#include "../utils/date.h"
#include "../utils/hash.h"
#include "../utils/string.h"
#include "../utils/validator.h"

/* 当前会话（全局） */
Session g_session = {0};

/* 初始化：确保初始管理员 admin/admin123 存在（首次启动种子化，FR 初始账号） */
Result auth_init(void)
{
    Admin admin;

    /* 已存在则跳过种子化 */
    if (repo_find_admin_by_username("admin") != NULL)
        return RES_OK;

    /* 构造初始管理员：id 交给仓库自动分配 */
    memset(&admin, 0, sizeof(admin));
    admin.id = 0;
    str_copy(admin.username, sizeof(admin.username), "admin");
    hash_random_salt(admin.salt, sizeof(admin.salt));
    hash_password("admin123", admin.salt, admin.password_hash,
                  sizeof(admin.password_hash));
    date_now_str(admin.created_at, sizeof(admin.created_at));

    /* 入库（仓库内部自动持久化） */
    return repo_add_admin(&admin, NULL);
}

/* 登录：username + password；区分管理员 / 读者，失败返回原因码 */
Result auth_login(const char *username, const char *password)
{
    char uname[64];
    Admin *admin;
    Reader *reader;

    if (!password)
        return RES_ERR_INVALID;

    /* 去除首尾空白；空用户名直接拒绝 */
    str_copy(uname, sizeof(uname), username);
    str_trim(uname);
    if (str_empty(uname))
        return RES_ERR_INVALID;

    /* 先按管理员匹配（FR-102） */
    admin = repo_find_admin_by_username(uname);
    if (admin != NULL) {
        if (!hash_password_verify(password, admin->salt, admin->password_hash))
            return RES_ERR_INVALID; /* 密码错误 */

        g_session.logged_in = true;
        g_session.role = ROLE_ADMIN;
        g_session.user_id = 0;
        str_copy(g_session.username, sizeof(g_session.username),
                 admin->username);
        str_copy(g_session.display_name, sizeof(g_session.display_name),
                 "管理员");
        return RES_OK;
    }

    /* 再按读者学号匹配（FR-103/106） */
    reader = repo_find_reader_by_no(uname);
    if (reader == NULL)
        return RES_ERR_NOT_FOUND; /* 账号不存在 */
    if (reader->status == READER_DELETED)
        return RES_ERR_NOT_FOUND; /* 已注销视同不存在 */
    if (reader->status == READER_FROZEN)
        return RES_ERR_FROZEN; /* 账号冻结 */
    if (!hash_password_verify(password, reader->salt, reader->password_hash))
        return RES_ERR_INVALID; /* 密码错误 */

    g_session.logged_in = true;
    g_session.role = ROLE_READER;
    g_session.user_id = reader->id;
    str_copy(g_session.username, sizeof(g_session.username), reader->reader_no);
    str_copy(g_session.display_name, sizeof(g_session.display_name),
             reader->name);
    return RES_OK;
}

/* 注册读者（FR-104）：学号唯一、字段校验，密码散列后入库 */
Result auth_register_reader(const Reader *r, const char *password)
{
    Reader copy;

    if (r == NULL || password == NULL)
        return RES_ERR_INVALID;

    /* 字段校验（FR-104/105） */
    if (!valid_length(r->name, 1, 63))
        return RES_ERR_INVALID;
    if (!valid_length(r->reader_no, 1, 31) || str_empty(r->reader_no))
        return RES_ERR_INVALID;
    if (strlen(password) < 6)
        return RES_ERR_INVALID;
    if (!str_empty(r->phone) && !valid_phone(r->phone))
        return RES_ERR_INVALID;
    if (!str_empty(r->email) && !valid_email(r->email))
        return RES_ERR_INVALID;

    /* 学号唯一性（FR-104） */
    if (repo_find_reader_by_no(r->reader_no) != NULL)
        return RES_ERR_DUP;

    /* 填充：id 交给仓库自动分配，状态默认正常 */
    memset(&copy, 0, sizeof(copy));
    copy.id = 0;
    str_copy(copy.reader_no, sizeof(copy.reader_no), r->reader_no);
    str_copy(copy.name, sizeof(copy.name), r->name);
    str_copy(copy.phone, sizeof(copy.phone), r->phone);
    str_copy(copy.email, sizeof(copy.email), r->email);
    copy.status = READER_NORMAL;
    hash_random_salt(copy.salt, sizeof(copy.salt));
    hash_password(password, copy.salt, copy.password_hash,
                  sizeof(copy.password_hash));
    date_now_str(copy.created_at, sizeof(copy.created_at));

    /* 入库（仓库内部自动持久化）；注册成功后不自动登录，保持会话不变 */
    return repo_add_reader(&copy, NULL);
}

/* 退出登录：清空会话字段，返回欢迎页 */
void auth_logout(void)
{
    memset(&g_session, 0, sizeof(g_session));
    g_session.role = ROLE_GUEST;
}

/* 修改密码 */
Result auth_change_password(const char *old_pwd, const char *new_pwd)
{
    char salt[32];
    char hash[64];

    if (!g_session.logged_in)
        return RES_ERR_PERM; /* 未登录不可改密 */
    if (new_pwd == NULL || strlen(new_pwd) < 6)
        return RES_ERR_INVALID;
    if (old_pwd == NULL)
        return RES_ERR_INVALID;

    if (g_session.role == ROLE_ADMIN) {
        Admin *a = repo_find_admin_by_username(g_session.username);
        if (a == NULL)
            return RES_ERR_NOT_FOUND;
        if (!hash_password_verify(old_pwd, a->salt, a->password_hash))
            return RES_ERR_INVALID; /* 旧密码错误 */

        /* 新盐 + 新散列写回 */
        hash_random_salt(salt, sizeof(salt));
        hash_password(new_pwd, salt, hash, sizeof(hash));
        str_copy(a->salt, sizeof(a->salt), salt);
        str_copy(a->password_hash, sizeof(a->password_hash), hash);
    } else if (g_session.role == ROLE_READER) {
        Reader *rd = repo_find_reader_by_no(g_session.username);
        if (rd == NULL)
            return RES_ERR_NOT_FOUND;
        if (!hash_password_verify(old_pwd, rd->salt, rd->password_hash))
            return RES_ERR_INVALID; /* 旧密码错误 */

        hash_random_salt(salt, sizeof(salt));
        hash_password(new_pwd, salt, hash, sizeof(hash));
        str_copy(rd->salt, sizeof(rd->salt), salt);
        str_copy(rd->password_hash, sizeof(rd->password_hash), hash);
    } else {
        return RES_ERR_PERM; /* 未知会话角色 */
    }

    /* 持久化 */
    return repo_save_all();
}
