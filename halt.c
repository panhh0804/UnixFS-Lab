/* halt.c: 退出并保存文件系统 */
#include <stdio.h>
#include <stdlib.h>
#include "filesys.h"
#include "superblock.h"

void halt(void)
{
    int i, j;

    /* 写回当前目录 */
    {
        int i, old_nblocks, new_nblocks;
        uint16_t block;

        old_nblocks = (int)((cur_path_inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
        new_nblocks = (int)((dir.size * sizeof(struct direct) + BLOCKSIZ - 1) / BLOCKSIZ);

        for (i = 0; i < new_nblocks && i < NADDR; i++)
        {
            block = cur_path_inode->di_addr[i];
            if (block == 0 && !(cur_path_inode->i_ino == 1 && i == 0))
            {
                block = balloc();
                if (block == DISKFULL)
                {
                    printf("\nhalt: disk full during directory writeback\n");
                    break;
                }
                cur_path_inode->di_addr[i] = block;
            }
            fseek(fd, DATASTART + block * BLOCKSIZ, SEEK_SET);
            fwrite(&dir.direct[i * (BLOCKSIZ / sizeof(struct direct))], 1, BLOCKSIZ, fd);
        }

        for (i = new_nblocks; i < old_nblocks && i < NADDR; i++)
        {
            if (cur_path_inode->di_addr[i] != 0)
                bfree(cur_path_inode->di_addr[i]);
            cur_path_inode->di_addr[i] = 0;
        }
        cur_path_inode->di_size = dir.size * sizeof(struct direct);
        cur_path_inode->i_flag |= IUPDATE;
        iput(cur_path_inode);
    }

    /* 关闭所有打开的文件 */
    for (i = 0; i < USERNUM; i++)
    {
        if (user[i].u_active)
        {
            for (j = 0; j < NOFILE; j++)
            {
                if (user[i].u_ofile[j] != SYSOPENFILE + 1)
                {
                    close(i, j);
                }
            }
        }
    }

    /* 写回超级块 */
    sb_write();

    /* 关闭虚拟盘文件 */
    fclose(fd);
    printf("\nGood Bye. See You Next Time.\n");
    exit(0);
}
