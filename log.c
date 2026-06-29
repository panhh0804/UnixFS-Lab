/* log.c: 用户登录与注销 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

int login(uint16_t uid, const char *passwd)
{
    int i, j;
    for (i = 0; i < PWDNUM; i++)
    {
        if (uid == pwd[i].p_uid && strcmp(passwd, pwd[i].password) == 0)
        {
            /* 不能用 u_uid==0 判断空闲槽，因为 root 用户的 uid 也是 0。 */
            for (j = 0; j < USERNUM; j++)
                if (!user[j].u_active)
                    break;
            if (j == USERNUM)
            {
                printf("\nToo many users in the System, please wait to login\n");
                return 0;
            }
            /* 登录成功后把密码表中的 uid/gid 复制到当前会话槽。 */
            user[j].u_active = 1;
            user[j].u_uid = uid;
            user[j].u_gid = pwd[i].p_gid;
            user[j].u_default_mode = DEFAULTMODE;
            user_id = j;
            return 1;
        }
    }
    printf("\nIncorrect password or user id\n");
    return 0;
}

int logout(uint16_t uid)
{
    int i, j, sys_no;
    struct inode *inode;
    /* 只在活跃会话中查找指定 uid，避免 root(uid=0) 与空闲槽冲突。 */
    for (i = 0; i < USERNUM; i++)
        if (user[i].u_active && uid == user[i].u_uid)
            break;
    if (i == USERNUM)
    {
        printf("\nNo such user\n");
        return 0;
    }
    for (j = 0; j < NOFILE; j++)
    {
        if (user[i].u_ofile[j] != SYSOPENFILE + 1)
        {
            /* 注销时关闭该用户仍然打开的文件，必要时写回 inode。 */
            sys_no = user[i].u_ofile[j];
            inode = sys_ofile[sys_no].f_inode;
            if (sys_ofile[sys_no].f_count > 0)
                sys_ofile[sys_no].f_count--;
            if (sys_ofile[sys_no].f_count == 0)
            {
                iput(inode);
                sys_ofile[sys_no].f_inode = NULL;
                sys_ofile[sys_no].f_flag = 0;
            }
            user[i].u_ofile[j] = SYSOPENFILE + 1;
        }
    }
    user[i].u_active = 0;
    user[i].u_uid = 0;
    user[i].u_gid = 0;
    return 1;
}
