/* ballfre.c: 磁盘块分配与回收 */
#include <stdio.h>
#include "filesys.h"

/* 组长块缓存。成组链接法中，组长块第 0 项是下一组数量，后续项是下一组块号。 */
static uint16_t block_buf[BLOCKSIZ / sizeof(uint16_t)];

uint16_t balloc(void)
{
    uint16_t free_block, free_block_num;
    int i;

    if (filsys.s_nfree == 0)
    {
        printf("\nDisk Full!!!\n");
        return DISKFULL;
    }

    /* 优先从超级块缓存的当前组中取一个空闲块号。 */
    free_block = filsys.s_free[filsys.s_pfree];
    if (free_block >= FILEBLK || free_block < 3)
    {
        printf("\nInvalid free block %d, disk corrupted\n", free_block);
        filsys.s_nfree = 0;
        return DISKFULL;
    }

    if (filsys.s_pfree == NICFREE - 1)
    {
        /* 当前组即将用完，free_block 作为组长块，里面保存下一组空闲块号。 */
        fseek(fd, DATASTART + free_block * BLOCKSIZ, SEEK_SET);
        fread(block_buf, 1, BLOCKSIZ, fd);
        free_block_num = block_buf[0]; /* 第一字为计数 */
        if (free_block_num == 0 || free_block_num > NICFREE)
        {
            filsys.s_nfree = 0;
            filsys.s_pfree = NICFREE;
            return free_block;
        }
        /* 将组长块中的下一组空闲块号重新装入超级块缓存栈。 */
        for (i = 0; i < free_block_num; i++)
        {
            filsys.s_free[NICFREE - 1 - i] = block_buf[i + 1];
        }
        filsys.s_pfree = NICFREE - free_block_num;
    }
    else
    {
        filsys.s_pfree++;
    }

    filsys.s_nfree--;
    filsys.s_fmod = SUPDATE;
    return free_block;
}

void bfree(uint16_t block_num)
{
    int i;

    if (block_num < 3)
        return; /* 系统保留块（0#根目录, 1#etc, 2#password），不回收 */

    if (filsys.s_pfree == 0) /* 栈满 */
    {
        /* 超级块缓存栈满：把当前这一组写入被回收块，使它成为新的组长块。 */
        block_buf[0] = NICFREE - 1; /* 计数 */
        for (i = 0; i < NICFREE - 1; i++)
        {
            block_buf[i + 1] = filsys.s_free[NICFREE - 1 - i];
        }
        fseek(fd, DATASTART + block_num * BLOCKSIZ, SEEK_SET);
        fwrite(block_buf, 1, BLOCKSIZ, fd);
        filsys.s_pfree = NICFREE - 1;
    }
    else
    {
        /* 超级块缓存栈未满，直接把回收块压入当前组。 */
        filsys.s_pfree--;
    }

    filsys.s_free[filsys.s_pfree] = block_num;
    filsys.s_nfree++;
    filsys.s_fmod = SUPDATE;
}
