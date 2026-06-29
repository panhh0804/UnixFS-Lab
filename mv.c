/* mv.c: 移动/重命名文件 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

static int valid_name(const char *name)
{
    return name && *name && strlen(name) < DIRSIZ && strchr(name, '/') == NULL;
}

static int load_dir_entries(struct inode *inode, struct direct *entries, int *entry_count)
{
    int i, nblocks, offset = 0;

    if (!(inode->di_mode & DIDIR))
        return 0;

    memset(entries, 0, sizeof(struct direct) * DIRNUM);
    *entry_count = (int)(inode->di_size / sizeof(struct direct));
    nblocks = (int)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);

    for (i = 0; i < nblocks && i < NADDR; i++)
    {
        fseek(fd, DATASTART + inode->di_addr[i] * BLOCKSIZ, SEEK_SET);
        fread(&entries[offset], 1, BLOCKSIZ, fd);
        offset += BLOCKSIZ / sizeof(struct direct);
    }
    return 1;
}

static int write_dir_entries(struct inode *inode, struct direct *entries, int entry_count)
{
    int i, old_nblocks, new_nblocks;
    uint16_t block;

    old_nblocks = (int)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
    new_nblocks = (int)((entry_count * sizeof(struct direct) + BLOCKSIZ - 1) / BLOCKSIZ);
    if (new_nblocks > NADDR)
    {
        printf(C_BRED "\nDirectory too large\n" C_RESET);
        return 0;
    }

    for (i = 0; i < new_nblocks; i++)
    {
        block = inode->di_addr[i];
        if (block == 0 && !(inode->i_ino == 1 && i == 0))
        {
            block = balloc();
            if (block == DISKFULL)
            {
                printf(C_BRED "\nDisk full during mv\n" C_RESET);
                return 0;
            }
            inode->di_addr[i] = block;
        }
        fseek(fd, DATASTART + block * BLOCKSIZ, SEEK_SET);
        fwrite(&entries[i * (BLOCKSIZ / sizeof(struct direct))], 1, BLOCKSIZ, fd);
    }

    for (i = new_nblocks; i < old_nblocks && i < NADDR; i++)
    {
        if (inode->di_addr[i] != 0)
            bfree(inode->di_addr[i]);
        inode->di_addr[i] = 0;
    }

    inode->di_size = entry_count * sizeof(struct direct);
    inode->i_flag |= IUPDATE;
    return 1;
}

void mv(const char *src, const char *dst)
{
    uint16_t src_id, dst_id;
    struct inode *inode;
    char parent[DIRSIZ + 1], newname[DIRSIZ + 1];
    char *slash;
    int i, entry_count, free_id;
    uint16_t src_ino;
    int src_is_dir;
    struct direct entries[DIRNUM];
    struct inode *dst_dir_inode;

    src_id = namei(src);
    if (src_id >= DIRNUM)
    {
        printf(C_BRED "\n%s does not exist\n" C_RESET, src);
        return;
    }

    dst_id = namei(dst);
    if (dst_id < DIRNUM)
    {
        printf(C_BRED "\n%s already exists\n" C_RESET, dst);
        return;
    }

    inode = iget(dir.direct[src_id].d_ino);
    src_ino = inode->i_ino;
    src_is_dir = (inode->di_mode & DIDIR) != 0;
    if (!access(user_id, inode, WRITE))
    {
        printf(C_BRED "\nPermission denied\n" C_RESET);
        iput(inode);
        return;
    }
    iput(inode);

    slash = strchr(dst, '/');
    if (slash)
    {
        if (src_is_dir)
        {
            printf(C_BRED "\nMoving directories across directories is not supported\n" C_RESET);
            return;
        }
        if (dst[0] == '/' || strchr(slash + 1, '/') != NULL)
        {
            printf(C_BRED "\nOnly moves to an immediate child directory are supported\n" C_RESET);
            return;
        }

        if ((size_t)(slash - dst) >= sizeof(parent) || strlen(slash + 1) >= sizeof(newname))
        {
            printf(C_BRED "\nDestination name too long\n" C_RESET);
            return;
        }

        strncpy(parent, dst, (size_t)(slash - dst));
        parent[slash - dst] = '\0';
        strcpy(newname, slash + 1);
        if (!valid_name(parent) || !valid_name(newname))
        {
            printf(C_BRED "\nInvalid destination\n" C_RESET);
            return;
        }
        if (strcmp(parent, ".") == 0 || strcmp(parent, "..") == 0)
        {
            printf(C_BRED "\nDestination parent must be a child directory\n" C_RESET);
            return;
        }

        dst_id = namei(parent);
        if (dst_id >= DIRNUM)
        {
            printf(C_BRED "\n%s does not exist\n" C_RESET, parent);
            return;
        }

        dst_dir_inode = iget(dir.direct[dst_id].d_ino);
        if (!(dst_dir_inode->di_mode & DIDIR))
        {
            printf(C_BRED "\n%s is not a directory\n" C_RESET, parent);
            iput(dst_dir_inode);
            return;
        }
        if (dst_dir_inode->i_ino == src_ino)
        {
            printf(C_BRED "\nCannot move a file into itself\n" C_RESET);
            iput(dst_dir_inode);
            return;
        }
        if (!access(user_id, dst_dir_inode, WRITE))
        {
            printf(C_BRED "\nPermission denied\n" C_RESET);
            iput(dst_dir_inode);
            return;
        }
        if (!load_dir_entries(dst_dir_inode, entries, &entry_count))
        {
            iput(dst_dir_inode);
            return;
        }

        free_id = -1;
        for (i = 0; i < DIRNUM; i++)
        {
            if (i < entry_count && entries[i].d_ino != 0 && strcmp(entries[i].d_name, newname) == 0)
            {
                printf(C_BRED "\n%s already exists\n" C_RESET, newname);
                iput(dst_dir_inode);
                return;
            }
            if (free_id < 0 && (i >= entry_count || entries[i].d_ino == 0))
                free_id = i;
        }
        if (free_id < 0)
        {
            printf(C_BRED "\nDirectory full\n" C_RESET);
            iput(dst_dir_inode);
            return;
        }

        strcpy(entries[free_id].d_name, newname);
        entries[free_id].d_ino = dir.direct[src_id].d_ino;
        if (free_id >= entry_count)
            entry_count = free_id + 1;

        if (!write_dir_entries(dst_dir_inode, entries, entry_count))
        {
            iput(dst_dir_inode);
            return;
        }
        iput(dst_dir_inode);

        dir_entry_remove(src_id);
        cur_path_inode->i_flag |= IUPDATE;
        printf(C_BGREEN "Moved '%s' -> '%s'.\n" C_RESET, src, dst);
        return;
    }

    if (!valid_name(dst))
    {
        printf(C_BRED "\nInvalid destination\n" C_RESET);
        return;
    }

    /* 同目录重命名：修改目录项名称 */
    strcpy(dir.direct[src_id].d_name, dst);
    cur_path_inode->i_flag |= IUPDATE;
    printf(C_BGREEN "Renamed '%s' -> '%s'.\n" C_RESET, src, dst);
}
