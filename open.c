/* open.c: 打开文件 */
#include <stdio.h>
#include "filesys.h"

static uint16_t aopen_depth(uint16_t uid, const char *filename, uint16_t openmode, int depth)
{
    uint16_t dinodeid;
    struct inode *inode;
    int i, j;

    if (depth > 8)
    {
        printf(C_BRED "\nToo many levels of symbolic links\n" C_RESET);
        return NOFILE;
    }

    dinodeid = namei(filename);
    if (dinodeid >= DIRNUM)
    {
        printf(C_BRED "\nFile does not exist!!\n" C_RESET);
        return NOFILE;
    }

    inode = iget(dir.direct[dinodeid].d_ino);

    /* 禁止打开目录 */
    if (inode->di_mode & DIDIR)
    {
        printf(C_BRED "\n%s is a directory\n" C_RESET, filename);
        iput(inode);
        return NOFILE;
    }

    /* 软链接解析 */
    if (inode->di_mode & DILINK)
    {
        char target[256];
        uint16_t block;
        if (inode->di_size > 0 && inode->di_size < 256)
        {
            block = get_block(inode, 0);
            if (block != 0)
            {
                fseek(fd, DATASTART + block * BLOCKSIZ, SEEK_SET);
                fread(target, 1, inode->di_size, fd);
                target[inode->di_size] = '\0';
                iput(inode);
                return aopen_depth(user_id, target, openmode, depth + 1);
            }
        }
        printf(C_BRED "\nBroken symbolic link\n" C_RESET);
        iput(inode);
        return NOFILE;
    }

    {
        uint16_t acc_mode;
        if (openmode & FWRITE) acc_mode = WRITE;
        else if (openmode & FREAD) acc_mode = READ;
        else acc_mode = EXECUTE;
        if (!access(uid, inode, acc_mode))
        {
            printf(C_BRED "\nFile open access denied!!!\n" C_RESET);
            iput(inode);
            return NOFILE;
        }
    }

    /* 分配系统打开文件表项 */
    for (i = 0; i < SYSOPENFILE; i++)
        if (sys_ofile[i].f_count == 0)
            break;
    if (i == SYSOPENFILE)
    {
        printf(C_BRED "\nSystem open file table full\n" C_RESET);
        iput(inode);
        return NOFILE;
    }

    sys_ofile[i].f_inode = inode;
    sys_ofile[i].f_flag = openmode;
    sys_ofile[i].f_count = 1;

    if (openmode & FAPPEND)
        sys_ofile[i].f_off = inode->di_size;
    else
        sys_ofile[i].f_off = 0;

    /* 分配用户打开文件表项 */
    for (j = 0; j < NOFILE; j++)
        if (user[user_id].u_ofile[j] == SYSOPENFILE + 1)
            break;
    if (j == NOFILE)
    {
        printf(C_BRED "\nUser open file table full!!!\n" C_RESET);
        sys_ofile[i].f_count = 0;
        iput(inode);
        return NOFILE;
    }

    user[uid].u_ofile[j] = i;
    return (uint16_t)j;
}

uint16_t aopen(uint16_t uid, const char *filename, uint16_t openmode)
{
    return aopen_depth(uid, filename, openmode, 0);
}
