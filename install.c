/* install.c: 加载文件系统 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys.h"
#include "superblock.h"

void install(void)
{
    int i, j;

    /* 打开虚拟盘文件 */
    fd = fopen("disk.img", "r+b");
    if (!fd)
    {
        printf("\nFile system can not be loaded\n");
        exit(1);
    }

    /* 读取超级块 */
    sb_read();

    /* 初始化 i-node Hash 链 */
    for (i = 0; i < NHINO; i++)
        hinode[i].i_forw = NULL;

    /* 初始化系统打开文件表 */
    for (i = 0; i < SYSOPENFILE; i++)
    {
        sys_ofile[i].f_count = 0;
        sys_ofile[i].f_inode = NULL;
    }

    /* 初始化用户表 */
    for (i = 0; i < USERNUM; i++)
    {
        user[i].u_active = 0;
        user[i].u_uid = 0;
        user[i].u_gid = 0;
        user[i].u_default_mode = DEFAULTMODE;
        for (j = 0; j < NOFILE; j++)
            user[i].u_ofile[j] = SYSOPENFILE + 1;
    }

    /* 读取 password 文件到内存 */
    fseek(fd, DATASTART + 2 * BLOCKSIZ, SEEK_SET);
    fread(pwd, 1, BLOCKSIZ, fd);

    /* 读取根目录到内存 */
    cur_path_inode = iget(1);
    dir.size = cur_path_inode->di_size / sizeof(struct direct);
    memset(dir.direct, 0, sizeof(dir.direct));

    j = 0;
    int nblocks = (int)((cur_path_inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
    for (i = 0; i < nblocks && i < NADDR; i++)
    {
        fseek(fd, DATASTART + BLOCKSIZ * cur_path_inode->di_addr[i], SEEK_SET);
        fread(&dir.direct[j], 1, BLOCKSIZ, fd);
        j += BLOCKSIZ / sizeof(struct direct);
    }
}
