/* link.c: 硬链接与软链接 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

/* 硬链接：让 dst 指向 src 的 inode */
void hlink(const char *src, const char *dst)
{
    uint16_t src_id, dst_id;
    struct inode *inode;

    src_id = namei(src);
    if (src_id >= DIRNUM)
    {
        printf(C_BRED "\n%s does not exist\n" C_RESET, src);
        return;
    }

    /* 不能给目录创建硬链接 */
    inode = iget(dir.direct[src_id].d_ino);
    if (inode->di_mode & DIDIR)
    {
        printf(C_BRED "\nCannot create hard link to a directory\n" C_RESET);
        iput(inode);
        return;
    }

    dst_id = namei(dst);
    if (dst_id < DIRNUM)
    {
        printf(C_BRED "\n%s already exists\n" C_RESET, dst);
        iput(inode);
        return;
    }

    dst_id = iname(dst);
    if (dst_id >= DIRNUM)
    {
        printf(C_BRED "\nDirectory full\n" C_RESET);
        iput(inode);
        return;
    }

    dir.direct[dst_id].d_ino = inode->i_ino;
    dir_entry_occupy(dst_id);
    inode->di_number++;
    inode->i_flag |= IUPDATE;
    iput(inode);
    printf(C_BGREEN "Hard link '%s' -> '%s' created.\n" C_RESET, dst, src);
}

/* 软链接：创建一个新文件，内容为 target 路径 */
void slink(const char *target, const char *name)
{
    uint16_t name_id;
    struct inode *inode;
    int n;

    if (namei(name) < DIRNUM)
    {
        printf(C_BRED "\n%s already exists\n" C_RESET, name);
        return;
    }

    name_id = iname(name);
    if (name_id >= DIRNUM)
    {
        printf(C_BRED "\nDirectory full\n" C_RESET);
        return;
    }

    inode = ialloc();
    if (!inode)
    {
        printf(C_BRED "\nialloc failed\n" C_RESET);
        return;
    }

    dir.direct[name_id].d_ino = inode->i_ino;
    dir_entry_occupy(name_id);

    inode->di_mode = DEFAULTMODE | DILINK;
    inode->di_uid = user[user_id].u_uid;
    inode->di_gid = user[user_id].u_gid;
    inode->di_size = 0;
    inode->di_number = 1;
    memset(inode->di_addr, 0, sizeof(inode->di_addr));
    inode->i_flag |= IUPDATE;

    /* 把 target 路径写入文件内容 */
    n = (int)strlen(target);
    if (n > 0)
    {
        uint16_t block = balloc();
        if (block != DISKFULL)
        {
            fseek(fd, DATASTART + block * BLOCKSIZ, SEEK_SET);
            fwrite(target, 1, n, fd);
            inode->di_addr[0] = block;
            inode->di_size = n;
            inode->i_flag |= IUPDATE;
        }
    }

    iput(inode);
    printf(C_BGREEN "Symbolic link '%s' -> '%s' created.\n" C_RESET, name, target);
}
