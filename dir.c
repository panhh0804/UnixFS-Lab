/* dir.c: 目录操作 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

void _dir(void)
{
    int i, k;
    int one;
    uint16_t di_mode;
    struct inode *temp_inode;

    printf(C_BOLD "\nCURRENT DIRECTORY\n" C_RESET);
    for (i = 0; i < dir.size; i++)
    {
        if (dir.direct[i].d_ino != 0)
        {
            temp_inode = iget(dir.direct[i].d_ino);
            di_mode = temp_inode->di_mode;
            if (temp_inode->di_mode & DIDIR)
                printf(C_BBLUE "%-14s " C_RESET, dir.direct[i].d_name);
            else if (temp_inode->di_mode & DILINK)
                printf(C_BCYAN "%-14s " C_RESET, dir.direct[i].d_name);
            else
                printf("%-14s ", dir.direct[i].d_name);

            /* 打印权限 rwxrwxrwx */
            for (k = 0; k < 9; k++)
            {
                one = (di_mode >> (8 - k)) & 1;
                if (one)
                {
                    if (k % 3 == 0)
                        printf("r");
                    else if (k % 3 == 1)
                        printf("w");
                    else
                        printf("x");
                }
                else
                    printf("-");
            }

            if (temp_inode->di_mode & DIDIR)
            {
                printf(C_BBLUE " <dir>\n" C_RESET);
            }
            else if (temp_inode->di_mode & DILINK)
            {
                printf(C_BCYAN " <link>\n" C_RESET);
            }
            else if (temp_inode->di_mode & DIFILE)
            {
                int bcount = (int)((temp_inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
                printf(" %8lu ", (unsigned long)temp_inode->di_size);
                printf(C_DIM "block chain:" C_RESET);
                for (k = 0; k < bcount && k < NADDR - 1; k++)
                    printf(" %4d", temp_inode->di_addr[k]);
                /* 打印间接块中的块号 */
                if (bcount > NADDR - 1 && temp_inode->di_addr[NADDR - 1] != 0)
                {
                    uint16_t ibuf[BLOCKSIZ / sizeof(uint16_t)];
                    fseek(fd, DATASTART + temp_inode->di_addr[NADDR - 1] * BLOCKSIZ, SEEK_SET);
                    fread(ibuf, 1, BLOCKSIZ, fd);
                    for (k = 0; k < bcount - (NADDR - 1); k++)
                        printf(" %4d", ibuf[k]);
                }
                printf("\n");
            }
            iput(temp_inode);
        }
    }
}

void mkdir(const char *dirname)
{
    int dirpos;
    struct inode *inode;
    struct direct buf[BLOCKSIZ / sizeof(struct direct)];
    uint16_t block;

    if (namei(dirname) < DIRNUM)
    {
        printf(C_BRED "\n%s already exists!\n" C_RESET, dirname);
        return;
    }

    dirpos = iname(dirname);
    if (dirpos >= DIRNUM)
        return;

    inode = ialloc();
    if (!inode)
        return;

    dir.direct[dirpos].d_ino = inode->i_ino;
    dir_entry_occupy((uint16_t)dirpos);

    /* 初始化新目录内容: . 和 .. */
    memset(buf, 0, BLOCKSIZ);
    strcpy(buf[0].d_name, ".");
    buf[0].d_ino = inode->i_ino;
    strcpy(buf[1].d_name, "..");
    buf[1].d_ino = cur_path_inode->i_ino;

    block = balloc();
    if (block == DISKFULL)
    {
        printf(C_BRED "\nmkdir: disk full\n" C_RESET);
        dir_entry_remove((uint16_t)dirpos);
        inode->di_number = 0;
        iput(inode);
        return;
    }

    fseek(fd, DATASTART + block * BLOCKSIZ, SEEK_SET);
    fwrite(buf, 1, BLOCKSIZ, fd);

    inode->di_size = 2 * sizeof(struct direct);
    inode->di_number = 1;
    inode->di_mode = user[user_id].u_default_mode | DIDIR;
    inode->di_uid = user[user_id].u_uid;
    inode->di_gid = user[user_id].u_gid;
    inode->di_addr[0] = block;
    inode->i_flag |= IUPDATE;

    iput(inode);
}

/* 将当前内存中的目录内容写回磁盘 */
static void writeback_dir(struct inode *inode)
{
    int i, nblocks;
    uint16_t block;

    nblocks = (dir.size * sizeof(struct direct) + BLOCKSIZ - 1) / BLOCKSIZ;

    /* 释放旧的块 */
    int old_nblocks = (int)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
    for (i = 0; i < old_nblocks && i < NADDR; i++)
    {
        bfree(inode->di_addr[i]);
        inode->di_addr[i] = 0;
    }

    /* 分配新块并写回 */
    for (i = 0; i < nblocks && i < NADDR; i++)
    {
        block = balloc();
        if (block == DISKFULL)
        {
            printf("\nchdir: disk full during writeback\n");
            break;
        }
        inode->di_addr[i] = block;
        fseek(fd, DATASTART + block * BLOCKSIZ, SEEK_SET);
        fwrite(&dir.direct[i * (BLOCKSIZ / sizeof(struct direct))], 1, BLOCKSIZ, fd);
    }

    inode->di_size = dir.size * sizeof(struct direct);
    inode->i_flag |= IUPDATE;
}

int chdir(const char *dirname)
{
    uint16_t dirid;
    struct inode *inode;
    int i, j, nblocks;

    if (strcmp(dirname, ".") == 0)
        return 1;

    dirid = namei(dirname);
    if (dirid < DIRNUM && dir.direct[dirid].d_ino == cur_path_inode->i_ino)
    {
        /* 切换到当前目录本身，无需操作 */
        return 1;
    }
    if (dirid >= DIRNUM)
    {
        printf(C_BRED "\n%s does not exist\n" C_RESET, dirname);
        return 0;
    }

    inode = iget(dir.direct[dirid].d_ino);
    if (!access(user_id, inode, EXECUTE))
    {
        printf(C_BRED "\nNo access to directory %s\n" C_RESET, dirname);
        iput(inode);
        return 0;
    }

    if (!(inode->di_mode & DIDIR))
    {
        printf(C_BRED "\n%s is not a directory\n" C_RESET, dirname);
        iput(inode);
        return 0;
    }

    /* 写回当前目录 */
    writeback_dir(cur_path_inode);
    iput(cur_path_inode);

    /* 切换到新目录 */
    cur_path_inode = inode;

    /* 读取新目录内容到内存 */
    dir.size = cur_path_inode->di_size / sizeof(struct direct);
    memset(dir.direct, 0, sizeof(dir.direct));

    nblocks = (int)((dir.size * sizeof(struct direct) + BLOCKSIZ - 1) / BLOCKSIZ);
    j = 0;
    for (i = 0; i < nblocks && i < NADDR; i++)
    {
        fseek(fd, DATASTART + cur_path_inode->di_addr[i] * BLOCKSIZ, SEEK_SET);
        fread(&dir.direct[j], 1, BLOCKSIZ, fd);
        j += BLOCKSIZ / sizeof(struct direct);
    }

    return 1;
}

void rmdir(const char *dirname)
{
    uint16_t dirid;
    struct inode *inode;
    int i, j, nblocks;
    struct direct tmp_buf[BLOCKSIZ / sizeof(struct direct)];

    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0)
    {
        printf(C_BRED "\nCannot remove . or ..\n" C_RESET);
        return;
    }

    dirid = namei(dirname);
    if (dirid >= DIRNUM)
    {
        printf(C_BRED "\n%s does not exist\n" C_RESET, dirname);
        return;
    }

    inode = iget(dir.direct[dirid].d_ino);
    if (!(inode->di_mode & DIDIR))
    {
        printf(C_BRED "\n%s is not a directory\n" C_RESET, dirname);
        iput(inode);
        return;
    }

    /* 检查目录是否为空（只有 . 和 ..） */
    nblocks = (int)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
    for (i = 0; i < nblocks && i < NADDR; i++)
    {
        if (inode->di_addr[i] == 0)
            continue;
        fseek(fd, DATASTART + inode->di_addr[i] * BLOCKSIZ, SEEK_SET);
        fread(tmp_buf, 1, BLOCKSIZ, fd);
        for (j = (i == 0) ? 2 : 0; j < BLOCKSIZ / sizeof(struct direct); j++)
        {
            if (tmp_buf[j].d_ino != 0)
            {
                printf(C_BRED "\nDirectory %s is not empty\n" C_RESET, dirname);
                iput(inode);
                return;
            }
        }
    }

    if (!access(user_id, inode, WRITE))
    {
        printf(C_BRED "\nPermission denied\n" C_RESET);
        iput(inode);
        return;
    }

    /* 释放目录数据块 */
    for (i = 0; i < NADDR; i++)
    {
        if (inode->di_addr[i] != 0)
        {
            bfree(inode->di_addr[i]);
            inode->di_addr[i] = 0;
        }
    }

    inode->di_number--;
    iput(inode);

    /* 从父目录移除 */
    dir_entry_remove(dirid);
}
