/* igetput.c: 内存 i 节点的获取与释放 */
#include <stdio.h>
#include <stdlib.h>
#include "filesys.h"

struct inode *iget(uint16_t dinodeid)
{
    int inodeid;
    long addr;
    struct inode *temp, *newinode;

    /* 内存 inode 使用 hash 链表缓存，避免同一个 inode 被重复从磁盘读入。 */
    inodeid = dinodeid % NHINO;

    /* 在 Hash 链表中查找 */
    temp = hinode[inodeid].i_forw;
    while (temp)
    {
        if (temp->i_ino == dinodeid)
        {
            /* 命中缓存时只增加引用计数，调用者最后通过 iput() 归还引用。 */
            temp->i_count++;
            return temp;
        }
        temp = temp->i_forw;
    }

    /* 未找到，从磁盘 inode 区读入对应 dinode，并包装成内存 inode。 */
    addr = DINODESTART + dinodeid * DINODESIZ;
    newinode = (struct inode *)malloc(sizeof(struct inode));
    if (!newinode)
    {
        printf("\niget: malloc failed\n");
        return NULL;
    }

    fseek(fd, addr, SEEK_SET);
    fread(&(newinode->di_number), DINODESIZ, 1, fd);

    /* 插入 Hash 链表头部，后续 iget 同一编号可直接复用。 */
    newinode->i_forw = hinode[inodeid].i_forw;
    newinode->i_back = NULL; /* i_back 仅作标记，实际删除时遍历链表 */
    hinode[inodeid].i_forw = newinode;

    newinode->i_count = 1;
    newinode->i_flag = 0;
    newinode->i_ino = dinodeid;
    return newinode;
}

void iput(struct inode *pinode)
{
    long addr;
    int inodeid;
    struct inode *temp, *prev;

    if (!pinode)
        return;

    if (pinode->i_count > 1)
    {
        pinode->i_count--;
        return;
    }

    /* i_count == 1，当前引用即将释放：要么写回，要么真正回收 inode。 */
    if (pinode->di_number != 0)
    {
        /* 还有硬链接引用，说明文件仍然有效，只需把 inode 元数据写回磁盘。 */
        addr = DINODESTART + pinode->i_ino * DINODESIZ;
        fseek(fd, addr, SEEK_SET);
        fwrite(&(pinode->di_number), DINODESIZ, 1, fd);
    }
    else if (pinode->di_mode != DIEMPTY || (pinode->i_flag & IUPDATE))
    {
        /* 有效文件链接数降为 0，或刚分配但初始化失败：释放数据块并归还 inode。 */
        free_all_blocks(pinode);
        pinode->di_mode = DIEMPTY;
        pinode->di_size = 0;
        addr = DINODESTART + pinode->i_ino * DINODESIZ;
        fseek(fd, addr, SEEK_SET);
        fwrite(&(pinode->di_number), DINODESIZ, 1, fd);
        ifree(pinode->i_ino);
    }

    /* 从 Hash 链表中摘除该内存 inode，防止后续访问悬空对象。 */
    inodeid = pinode->i_ino % NHINO;
    temp = hinode[inodeid].i_forw;
    prev = NULL;
    while (temp)
    {
        if (temp == pinode)
        {
            if (prev)
                prev->i_forw = temp->i_forw;
            else
                hinode[inodeid].i_forw = temp->i_forw;
            break;
        }
        prev = temp;
        temp = temp->i_forw;
    }

    free(pinode);
}
