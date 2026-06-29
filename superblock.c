/* superblock.c: 超级块管理模块
 *
 * 封装超级块的所有磁盘读写与初始化操作，
 * 将原本散落在 format.c / install.c / halt.c 中的超级块逻辑统一到此。
 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"
#include "superblock.h"

/* 格式化时初始化超级块，并建立成组链接法的组长块链 */
void sb_init_format(void)
{
    struct filsys filsys_local;
    uint16_t total_inodes;
    int i;
    int pos;
    int next_count;
    uint16_t free_seq[FILEBLK - 3];
    uint16_t block_buf[BLOCKSIZ / sizeof(uint16_t)];

    /* 初始化超级块元数据。这里的 s_fsize 是数据区块数，不包含前面的保留块、超级块和 inode 区。 */
    memset(&filsys_local, 0, sizeof(filsys_local));
    filsys_local.s_isize = DINODEBLK;
    filsys_local.s_fsize = FILEBLK;
    total_inodes = (uint16_t)(DINODEBLK * BLOCKSIZ / DINODESIZ);
    filsys_local.s_ninode = total_inodes - 4; /* 0,1,2,3 已预分配 */

    /* 超级块只缓存前 NICINOD 个空闲 inode 号；剩余空闲 inode 以后按需扫描 inode 区补充。 */
    for (i = 0; i < filsys_local.s_ninode && i < NICINOD; i++)
        filsys_local.s_inode[i] = (uint16_t)(4 + i);

    filsys_local.s_pinode = (uint16_t)filsys_local.s_ninode;
    if (filsys_local.s_pinode > NICINOD)
        filsys_local.s_pinode = NICINOD;
    filsys_local.s_rinode = (uint16_t)(4 + filsys_local.s_pinode);

    /*
     * balloc() 在 s_pfree == NICFREE - 1 时，把当前块当作组长块读取。
     * 因此每组最后一个块必须保存下一组空闲块栈，且盘上顺序要与
     * balloc() 的反向装栈逻辑匹配。
     */
    /* 数据块 0、1、2 被根目录、/etc 和 /etc/password 预占用，不能进入空闲块链。 */
    for (i = 0; i < FILEBLK - 3; i++)
        free_seq[i] = (uint16_t)(FILEBLK - 1 - i);

    /* s_free[] 是超级块中的当前空闲块号栈，格式化时先装入第一组。 */
    filsys_local.s_nfree = FILEBLK - 3;
    filsys_local.s_pfree = 0;
    for (i = 0; i < NICFREE; i++)
        filsys_local.s_free[i] = free_seq[i];

    /* 把后续空闲块号按组写入磁盘上的“组长块”，形成成组链接。 */
    for (pos = NICFREE - 1; pos < FILEBLK - 3; pos += NICFREE)
    {
        memset(block_buf, 0, BLOCKSIZ);
        next_count = FILEBLK - 3 - (pos + 1);
        if (next_count > NICFREE)
            next_count = NICFREE;

        /* 组长块第 0 项保存下一组数量，后续项保存下一组空闲块号。 */
        block_buf[0] = (uint16_t)next_count;
        for (i = 0; i < next_count; i++)
            block_buf[i + 1] = free_seq[pos + next_count - i];

        fseek(fd, DATASTART + free_seq[pos] * BLOCKSIZ, SEEK_SET);
        fwrite(block_buf, 1, BLOCKSIZ, fd);
    }

    /* 最后一组之后没有下一组，用全 0 块作为链尾标记。 */
    memset(block_buf, 0, BLOCKSIZ);
    fseek(fd, DATASTART + free_seq[FILEBLK - 4] * BLOCKSIZ, SEEK_SET);
    fwrite(block_buf, 1, BLOCKSIZ, fd);

    /* 将超级块写入磁盘 1# 块 */
    fseek(fd, BLOCKSIZ, SEEK_SET);
    fwrite(&filsys_local, 1, sizeof(struct filsys), fd);
    fflush(fd);

    /* 将超级块加载到全局变量 */
    memcpy(&filsys, &filsys_local, sizeof(filsys));
}

/* 从磁盘 1# 块读取超级块到全局 filsys */
void sb_read(void)
{
    fseek(fd, BLOCKSIZ, SEEK_SET);
    fread(&filsys, 1, sizeof(struct filsys), fd);
}

/* 将全局 filsys 写回磁盘 1# 块 */
void sb_write(void)
{
    fseek(fd, BLOCKSIZ, SEEK_SET);
    fwrite(&filsys, 1, sizeof(struct filsys), fd);
}

/* 若超级块被标记为脏(s_fmod)，则同步写回磁盘 */
void sb_sync(void)
{
    if (filsys.s_fmod & SUPDATE)
    {
        sb_write();
        filsys.s_fmod = 0;
    }
}
