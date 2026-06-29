/* find.c: 递归目录查找 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

static void find_in_dir(struct inode *dir_inode, const char *prefix, const char *pattern)
{
    int i, j, nblocks;
    struct direct entries[BLOCKSIZ / sizeof(struct direct)];
    struct direct *entry_ptr;
    struct inode *child;
    char path[256];
    int use_mem_dir = 0;
    int entry_count;

    /* 如果遍历的是当前内存中的目录，直接使用 dir 结构（避免磁盘未同步问题） */
    if (dir_inode == cur_path_inode)
    {
        use_mem_dir = 1;
        entry_count = dir.size;
    }
    else
    {
        nblocks = (int)((dir_inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
        entry_count = 0; /* 从磁盘读取时通过循环计数 */
    }

    for (i = 0; ; i++)
    {
        if (use_mem_dir)
        {
            if (i >= entry_count)
                break;
            entry_ptr = &dir.direct[i];
        }
        else
        {
            if (i >= nblocks || i >= NADDR)
                break;
            /* 块号0是根目录的合法数据块 */
            fseek(fd, DATASTART + dir_inode->di_addr[i] * BLOCKSIZ, SEEK_SET);
            fread(entries, 1, BLOCKSIZ, fd);
            entry_ptr = entries;
        }

        int max_entries = use_mem_dir ? 1 : (BLOCKSIZ / sizeof(struct direct));
        for (j = 0; j < max_entries; j++)
        {
            struct direct *ep = use_mem_dir ? &entry_ptr[j] : &entry_ptr[j];
            if (ep->d_ino == 0)
                continue;
            if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
                continue;

            /* 构建完整路径 */
            if (strcmp(prefix, "/") == 0)
                snprintf(path, sizeof(path), "/%s", ep->d_name);
            else
                snprintf(path, sizeof(path), "%s/%s", prefix, ep->d_name);

            /* 名称匹配则输出 */
            if (pattern == NULL || strstr(ep->d_name, pattern) != NULL)
                printf("%s\n", path);

            /* 递归进入子目录 */
            child = iget(ep->d_ino);
            if (child->di_mode & DIDIR)
            {
                find_in_dir(child, path, pattern);
            }
            iput(child);
        }
    }
}

void file_find(const char *path, const char *pattern)
{
    uint16_t dirid;
    struct inode *inode;

    if (strcmp(path, "/") == 0)
    {
        inode = iget(1); /* 根目录 */
    }
    else
    {
        dirid = namei(path);
        if (dirid >= DIRNUM)
        {
            printf(C_BRED "\n%s does not exist\n" C_RESET, path);
            return;
        }
        inode = iget(dir.direct[dirid].d_ino);
        if (!(inode->di_mode & DIDIR))
        {
            printf(C_BRED "\n%s is not a directory\n" C_RESET, path);
            iput(inode);
            return;
        }
    }

    find_in_dir(inode, path, pattern);
    iput(inode);
}
