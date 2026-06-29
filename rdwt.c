/* rdwt.c: 文件读写 + 多级索引支持 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

/* 逻辑块号 → 物理块号（支持直接索引 + 一次间址） */
uint16_t get_block(struct inode *inode, uint16_t logical_block)
{
    if (logical_block < NADDR - 1)
    {
        /* 前 9 个逻辑块直接由 inode->di_addr[0..8] 指向数据块。 */
        return inode->di_addr[logical_block];
    }
    else if (logical_block < NADDR - 1 + BLOCKSIZ / sizeof(uint16_t))
    {
        /* 超过直接索引范围后，通过 di_addr[9] 指向的一级间接索引块查找。 */
        uint16_t indirect = inode->di_addr[NADDR - 1];
        uint16_t buf[BLOCKSIZ / sizeof(uint16_t)];
        if (indirect == 0)
            return 0;
        fseek(fd, DATASTART + indirect * BLOCKSIZ, SEEK_SET);
        fread(buf, 1, BLOCKSIZ, fd);
        return buf[logical_block - (NADDR - 1)];
    }
    return 0;
}

/* 设置逻辑块号对应的物理块号，返回是否成功 */
int set_block(struct inode *inode, uint16_t logical_block, uint16_t physical_block)
{
    if (logical_block < NADDR - 1)
    {
        /* 直接索引：inode 地址项直接保存数据块号。 */
        inode->di_addr[logical_block] = physical_block;
        return 1;
    }
    else if (logical_block < NADDR - 1 + BLOCKSIZ / sizeof(uint16_t))
    {
        uint16_t indirect = inode->di_addr[NADDR - 1];
        uint16_t buf[BLOCKSIZ / sizeof(uint16_t)];
        if (indirect == 0)
        {
            /* 第一次使用间接索引时，先分配一个专门保存块号的间接索引块。 */
            indirect = balloc();
            if (indirect == DISKFULL)
                return 0;
            inode->di_addr[NADDR - 1] = indirect;
            memset(buf, 0, BLOCKSIZ);
            fseek(fd, DATASTART + indirect * BLOCKSIZ, SEEK_SET);
            fwrite(buf, 1, BLOCKSIZ, fd);
        }
        fseek(fd, DATASTART + indirect * BLOCKSIZ, SEEK_SET);
        fread(buf, 1, BLOCKSIZ, fd);
        /* 间接索引块内部是 uint16_t 数组，每一项保存一个数据块号。 */
        buf[logical_block - (NADDR - 1)] = physical_block;
        fseek(fd, DATASTART + indirect * BLOCKSIZ, SEEK_SET);
        fwrite(buf, 1, BLOCKSIZ, fd);
        return 1;
    }
    return 0;
}

/* 释放 inode 占用的所有数据块（直接索引 + 一次间址） */
void free_all_blocks(struct inode *inode)
{
    int i;
    uint16_t nblocks;

    if (inode->di_size == 0)
    {
        memset(inode->di_addr, 0, sizeof(inode->di_addr));
        return;
    }

    /* 向上取整计算文件实际占用块数，避免漏掉不足 512 字节的最后一块。 */
    nblocks = (uint16_t)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);

    /* 释放直接索引块 */
    for (i = 0; i < NADDR - 1 && i < nblocks; i++)
    {
        if (inode->di_addr[i] != 0)
            bfree(inode->di_addr[i]);
    }

    /* 释放间接索引块指向的数据块及间接块本身 */
    if (inode->di_addr[NADDR - 1] != 0)
    {
        /* 先释放间接索引块指向的数据块，最后再释放间接索引块本身。 */
        uint16_t buf[BLOCKSIZ / sizeof(uint16_t)];
        fseek(fd, DATASTART + inode->di_addr[NADDR - 1] * BLOCKSIZ, SEEK_SET);
        fread(buf, 1, BLOCKSIZ, fd);
        for (i = 0; i < BLOCKSIZ / sizeof(uint16_t) && i + NADDR - 1 < nblocks; i++)
        {
            if (buf[i] != 0)
                bfree(buf[i]);
        }
        bfree(inode->di_addr[NADDR - 1]);
    }

    memset(inode->di_addr, 0, sizeof(inode->di_addr));
}

uint16_t read(uint16_t fd_num, char *buf, uint16_t size)
{
    uint32_t off;
    int block, block_off;
    struct inode *inode;
    char *temp_buf;
    uint16_t bytes_left, to_read;
    uint16_t physical_block;

    if (fd_num >= NOFILE || user[user_id].u_ofile[fd_num] >= SYSOPENFILE)
    {
        printf("\nInvalid file descriptor\n");
        return 0;
    }

    if (!(sys_ofile[user[user_id].u_ofile[fd_num]].f_flag & FREAD))
    {
        printf("\nThe file is not opened for read\n");
        return 0;
    }

    inode = sys_ofile[user[user_id].u_ofile[fd_num]].f_inode;
    temp_buf = buf;
    off = sys_ofile[user[user_id].u_ofile[fd_num]].f_off;

    if (off >= inode->di_size)
        return 0;
    if (off + size > inode->di_size)
        size = (uint16_t)(inode->di_size - off);

    bytes_left = size;
    while (bytes_left > 0)
    {
        block = (int)(off / BLOCKSIZ);
        block_off = (int)(off % BLOCKSIZ);

        physical_block = get_block(inode, block);
        if (physical_block == 0)
            break;

        to_read = BLOCKSIZ - block_off;
        if (to_read > bytes_left)
            to_read = bytes_left;

        fseek(fd, DATASTART + physical_block * BLOCKSIZ + block_off, SEEK_SET);
        fread(temp_buf, 1, to_read, fd);

        temp_buf += to_read;
        off += to_read;
        bytes_left -= to_read;
    }

    sys_ofile[user[user_id].u_ofile[fd_num]].f_off = off;
    return size - bytes_left;
}

uint16_t write(uint16_t fd_num, char *buf, uint16_t size)
{
    uint32_t off;
    int block, block_off;
    struct inode *inode;
    char *temp_buf;
    uint16_t bytes_left, to_write;
    uint16_t physical_block;

    if (fd_num >= NOFILE || user[user_id].u_ofile[fd_num] >= SYSOPENFILE)
    {
        printf("\nInvalid file descriptor\n");
        return 0;
    }

    if (!(sys_ofile[user[user_id].u_ofile[fd_num]].f_flag & FWRITE))
    {
        printf("\nThe file is not opened for write\n");
        return 0;
    }

    inode = sys_ofile[user[user_id].u_ofile[fd_num]].f_inode;
    temp_buf = buf;
    off = sys_ofile[user[user_id].u_ofile[fd_num]].f_off;

    bytes_left = size;
    while (bytes_left > 0)
    {
        block = (int)(off / BLOCKSIZ);
        block_off = (int)(off % BLOCKSIZ);

        physical_block = get_block(inode, block);
        if (physical_block == 0)
        {
            physical_block = balloc();
            if (physical_block == DISKFULL)
            {
                printf("\nDisk full during write\n");
                break;
            }
            if (!set_block(inode, block, physical_block))
            {
                printf("\nFile too large (indirect index limit)\n");
                bfree(physical_block);
                break;
            }
        }

        to_write = BLOCKSIZ - block_off;
        if (to_write > bytes_left)
            to_write = bytes_left;

        fseek(fd, DATASTART + physical_block * BLOCKSIZ + block_off, SEEK_SET);
        fwrite(temp_buf, 1, to_write, fd);

        temp_buf += to_write;
        off += to_write;
        bytes_left -= to_write;
    }

    if (off > inode->di_size)
        inode->di_size = off;

    inode->i_flag |= IUPDATE;
    sys_ofile[user[user_id].u_ofile[fd_num]].f_off = off;
    return size - bytes_left;
}
