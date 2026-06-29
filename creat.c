/* creat.c: 创建文件 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

uint16_t creat(uint16_t uid, const char *filename, uint16_t mode)
{
    uint16_t di_ith, di_ino;
    struct inode *inode;
    int i, j;

    di_ino = namei(filename);
    if (di_ino < DIRNUM)
    {
        /* 文件已存在，覆盖 */
        inode = iget(dir.direct[di_ino].d_ino);
        if (inode->di_mode & DIDIR)
        {
            printf(C_BRED "\n%s is a directory, cannot create over it\n" C_RESET, filename);
            iput(inode);
            return NOFILE;
        }
        if (!access(user_id, inode, WRITE))
        {
            iput(inode);
            printf(C_BRED "\ncreat access not allowed\n" C_RESET);
            return NOFILE;
        }

        /* 分配用户打开文件表项 */
        for (j = 0; j < NOFILE; j++)
            if (user[uid].u_ofile[j] == SYSOPENFILE + 1)
                break;
        if (j == NOFILE)
        {
            printf(C_BRED "\nUser open file table full\n" C_RESET);
            iput(inode);
            return NOFILE;
        }

        for (i = 0; i < SYSOPENFILE; i++)
            if (sys_ofile[i].f_count == 0)
                break;
        if (i == SYSOPENFILE)
        {
            printf(C_BRED "\nSystem open file table full\n" C_RESET);
            iput(inode);
            return NOFILE;
        }

        /* 释放旧文件的所有数据块 */
        free_all_blocks(inode);
        inode->di_size = 0;
        inode->i_flag |= IUPDATE;

        /* 重置已打开该文件的文件表项偏移 */
        for (int k = 0; k < SYSOPENFILE; k++)
        {
            if (sys_ofile[k].f_inode == inode)
            {
                sys_ofile[k].f_off = 0;
            }
        }

        user[uid].u_ofile[j] = i;
        sys_ofile[i].f_inode = inode;
        sys_ofile[i].f_flag = FWRITE;
        sys_ofile[i].f_count = 1;
        sys_ofile[i].f_off = 0;
        return (uint16_t)j;
    }
    else
    {
        /* 文件不存在，新建 */
        di_ith = iname(filename);
        if (di_ith >= DIRNUM)
            return NOFILE;

        for (i = 0; i < SYSOPENFILE; i++)
            if (sys_ofile[i].f_count == 0)
                break;
        if (i == SYSOPENFILE)
        {
            printf(C_BRED "\nSystem open file table full\n" C_RESET);
            return NOFILE;
        }

        for (j = 0; j < NOFILE; j++)
            if (user[uid].u_ofile[j] == SYSOPENFILE + 1)
                break;
        if (j == NOFILE)
        {
            printf(C_BRED "\nUser open file table full\n" C_RESET);
            return NOFILE;
        }

        inode = ialloc();
        if (!inode)
            return NOFILE;

        dir.direct[di_ith].d_ino = inode->i_ino;
        dir_entry_occupy(di_ith);

        inode->di_mode = (mode & 0777) | DIFILE;
        inode->di_uid = user[uid].u_uid;
        inode->di_gid = user[uid].u_gid;
        inode->di_size = 0;
        inode->di_number = 1;
        memset(inode->di_addr, 0, sizeof(inode->di_addr));
        inode->i_flag |= IUPDATE;

        user[user_id].u_ofile[j] = i;
        sys_ofile[i].f_inode = inode;
        sys_ofile[i].f_flag = FWRITE;
        sys_ofile[i].f_count = 1;
        sys_ofile[i].f_off = 0;
        return (uint16_t)j;
    }
}
