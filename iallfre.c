/* iallfre.c: i 节点分配与回收 */
#include <stdio.h>
#include "filesys.h"

static struct dinode block_buf[BLOCKSIZ / DINODESIZ];

struct inode *ialloc(void)
{
    struct inode *temp_inode;
    uint16_t cur_di;
    int i, count;
    int block_end_flag;
    uint16_t max_dinode;

    max_dinode = DINODEBLK * BLOCKSIZ / DINODESIZ;

    if (filsys.s_pinode == 0)
    {
        /* 超级块缓存的空闲 inode 栈为空，从磁盘 inode 区扫描 DIEMPTY 项补充。 */
        count = 0;
        block_end_flag = 1;
        cur_di = filsys.s_rinode;

        while (count < NICINOD && cur_di < max_dinode)
        {
            if (block_end_flag)
            {
                /* inode 区按块读取，一次读 512 字节，可检查 16 个 dinode。 */
                fseek(fd, DINODESTART + cur_di * DINODESIZ, SEEK_SET);
                fread(block_buf, 1, BLOCKSIZ, fd);
                block_end_flag = 0;
                i = 0;
            }

            if (block_buf[i].di_mode == DIEMPTY)
            {
                filsys.s_inode[count] = cur_di;
                count++;
            }

            cur_di++;
            i++;
            if (i >= BLOCKSIZ / DINODESIZ)
                block_end_flag = 1;
        }

        /* 记录下次继续扫描的位置，避免每次都从 inode 0 开始。 */
        filsys.s_rinode = cur_di;
        filsys.s_pinode = count; /* 栈中可用数量 */
    }

    if (filsys.s_pinode == 0)
    {
        printf("\nialloc: no free inode\n");
        return NULL;
    }

    /* 从超级块空闲 inode 栈中弹出一个 inode 号。 */
    filsys.s_pinode--;
    temp_inode = iget(filsys.s_inode[filsys.s_pinode]);
    /* 标记为刚分配，若后续初始化失败，iput() 仍会把它归还到空闲 inode 表。 */
    temp_inode->i_flag |= IUPDATE;

    /* 将分配结果同步到磁盘 inode 区；真正的 mode/uid/size 由调用者随后填写。 */
    fseek(fd, DINODESTART + filsys.s_inode[filsys.s_pinode] * DINODESIZ, SEEK_SET);
    fwrite(&(temp_inode->di_number), 1, sizeof(struct dinode), fd);

    filsys.s_ninode--;
    filsys.s_fmod = SUPDATE;
    return temp_inode;
}

void ifree(uint16_t dinodeid)
{
    uint16_t max_dinode;
    int i;

    max_dinode = DINODEBLK * BLOCKSIZ / DINODESIZ;
    if (dinodeid >= max_dinode)
        return;

    /* 防止重复释放导致空闲 inode 计数超过 inode 总数。 */
    if (filsys.s_ninode >= max_dinode)
        return;

    /* 当前缓存栈中已有该 inode 时直接返回，避免同一个 inode 重复入栈。 */
    for (i = 0; i < filsys.s_pinode && i < NICINOD; i++)
    {
        if (filsys.s_inode[i] == dinodeid)
            return;
    }

    filsys.s_ninode++;
    if (filsys.s_pinode < NICINOD)
    {
        /* 缓存栈未满，直接压入超级块的空闲 inode 栈。 */
        filsys.s_inode[filsys.s_pinode] = dinodeid;
        filsys.s_pinode++;
    }
    else if (dinodeid < filsys.s_rinode)
    {
        /* 缓存栈满时不越界写数组，只更新下次扫描起点。 */
        filsys.s_rinode = dinodeid;
    }
    filsys.s_fmod = SUPDATE;
}
