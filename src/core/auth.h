#ifndef LMS_CORE_AUTH_H
#define LMS_CORE_AUTH_H

#include "../types.h"
#include "../entities.h"

/* ============ 登录 / 注册 / 会话（FR-101 ~ FR-106） ============ */

/* 会话状态 */
typedef struct {
    bool logged_in;
    Role role;          /* ROLE_ADMIN / ROLE_READER */
    EntityId user_id;   /* ROLE_READER 时为 reader id；ROLE_ADMIN 时为 0 */
    char username[64];
    char display_name[64];
} Session;

/* 当前会话（全局） */
extern Session g_session;

/* 初始化：确保初始管理员 admin/admin123 存在（首次启动种子化，FR 初始账号） */
Result auth_init(void);

/* 登录：username + password；区分管理员 / 读者，失败返回原因码 */
Result auth_login(const char *username, const char *password);

/* 注册读者（FR-104）：学号唯一、字段校验，密码散列后入库 */
Result auth_register_reader(const Reader *r, const char *password);

/* 退出登录，返回欢迎页 */
void auth_logout(void);

/* 修改密码 */
Result auth_change_password(const char *old_pwd, const char *new_pwd);

#endif /* LMS_CORE_AUTH_H */
