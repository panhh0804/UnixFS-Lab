/* name.c */
#include <string.h>
#include <stdio.h>
#include "filesys.h"

int valid_dir_name(const char *name)
{
    if (!name || *name == '\0')
        return 0;
    if (strlen(name) >= DIRSIZ)
        return 0;
    if (strchr(name, '/') != NULL)
        return 0;
    if (strchr(name, ' ') != NULL)
        return 0;
    return 1;
}

void dir_entry_occupy(uint16_t index)
{
    if (index < DIRNUM && index >= dir.size)
        dir.size = index + 1;
}

void dir_entry_remove(uint16_t index)
{
    if (index >= DIRNUM)
        return;

    dir.direct[index].d_ino = 0;
    memset(dir.direct[index].d_name, 0, DIRSIZ);

    while (dir.size > 0 && dir.direct[dir.size - 1].d_ino == 0)
        dir.size--;
}

/* 在当前目录中查找文件名，返回目录项索引；未找到返回 DIRNUM (128) */
uint16_t namei(const char *name)
{
    int i;
    if (!name)
        return DIRNUM;
    if (!valid_dir_name(name) && strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
        return DIRNUM;

    for (i = 0; i < dir.size; i++)
    {
        if (dir.direct[i].d_ino != 0 && strcmp(dir.direct[i].d_name, name) == 0)
            return (uint16_t)i;
    }
    return DIRNUM; /* not found */
}

/* 在当前目录中找一个空目录项，返回其索引；目录满返回 DIRNUM */
uint16_t iname(const char *name)
{
    int i;
    if (!valid_dir_name(name))
    {
        printf(C_BRED "\nInvalid name: use 1-%d characters without '/'\n" C_RESET, DIRSIZ - 1);
        return DIRNUM;
    }

    for (i = 0; i < DIRNUM; i++)
    {
        if (dir.direct[i].d_ino == 0)
        {
            memset(dir.direct[i].d_name, 0, DIRSIZ);
            strcpy(dir.direct[i].d_name, name);
            return (uint16_t)i;
        }
    }
    printf("\nThe current directory is full!!\n");
    return DIRNUM;
}
