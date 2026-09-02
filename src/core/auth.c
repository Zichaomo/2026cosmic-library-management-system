/* 认证模块 - 业务逻辑层（骨架 stub） */

#include "auth.h"

/* 当前会话（全局） */
Session g_session = {0};

/* TODO(M1): 首次启动时种子化初始管理员 admin/admin123（FR 初始账号） */
Result auth_init(void)
{
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 校验用户名密码，区分管理员/读者，失败返回原因码并建立会话 */
Result auth_login(const char *username, const char *password)
{
    (void)username;
    (void)password;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* TODO(M1): 校验学号唯一性与字段合法性，密码散列后注册新读者（FR-104） */
Result auth_register_reader(const Reader *r, const char *password)
{
    (void)r;
    (void)password;
    return RES_ERR_NOT_IMPLEMENTED;
}

/* 退出登录：清空会话字段，返回欢迎页 */
void auth_logout(void)
{
    g_session.logged_in = false;
    g_session.role = ROLE_GUEST;
    g_session.user_id = 0;
    g_session.username[0] = '\0';
    g_session.display_name[0] = '\0';
}

/* TODO(M1): 校验旧密码后更新为新密码 */
Result auth_change_password(const char *old_pwd, const char *new_pwd)
{
    (void)old_pwd;
    (void)new_pwd;
    return RES_ERR_NOT_IMPLEMENTED;
}
