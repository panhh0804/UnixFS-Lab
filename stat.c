/* stat.c: 显示文件 inode 元数据 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

static void mode_to_str(uint16_t mode, char *buf)
{
    buf[0] = (mode & DIDIR) ? 'd' : ((mode & DILINK) ? 'l' : '-');
    buf[1] = (mode & UDIREAD) ? 'r' : '-';
    buf[2] = (mode & UDIWRITE) ? 'w' : '-';
    buf[3] = (mode & UDIEXECUTE) ? 'x' : '-';
    buf[4] = (mode & GDIREAD) ? 'r' : '-';
    buf[5] = (mode & GDIWRITE) ? 'w' : '-';
    buf[6] = (mode & GDIEXECUTE) ? 'x' : '-';
    buf[7] = (mode & ODIREAD) ? 'r' : '-';
    buf[8] = (mode & ODIWRITE) ? 'w' : '-';
    buf[9] = (mode & ODIEXECUTE) ? 'x' : '-';
    buf[10] = '\0';
}

void file_stat(const char *filename)
{
    uint16_t dinodeid;
    struct inode *inode;
    char mode_str[16];
    int i, bcount;

    dinodeid = namei(filename);
    if (dinodeid >= DIRNUM)
    {
        printf(C_BRED "\n%s does not exist\n" C_RESET, filename);
        return;
    }

    inode = iget(dir.direct[dinodeid].d_ino);
    mode_to_str(inode->di_mode, mode_str);
    bcount = (int)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);

    printf("  " C_BOLD "File:" C_RESET " %s\n", filename);
    printf("  " C_BOLD "Inode:" C_RESET " %d\n", inode->i_ino);
    printf("  " C_BOLD "Links:" C_RESET " %d\n", inode->di_number);
    printf("  " C_BOLD "Access:" C_RESET " (%s)\n", mode_str);
    printf("  " C_BOLD "Uid:" C_RESET " %d   " C_BOLD "Gid:" C_RESET " %d\n", inode->di_uid, inode->di_gid);
    printf("  " C_BOLD "Size:" C_RESET " %lu bytes\n", (unsigned long)inode->di_size);
    printf("  " C_BOLD "Blocks:" C_RESET " %d\n", bcount);
    printf("  " C_BOLD "Block chain:" C_RESET);
    for (i = 0; i < bcount && i < NADDR - 1; i++)
        printf(" %d", inode->di_addr[i]);
    if (bcount > NADDR - 1 && inode->di_addr[NADDR - 1] != 0)
    {
        uint16_t ibuf[BLOCKSIZ / sizeof(uint16_t)];
        fseek(fd, DATASTART + inode->di_addr[NADDR - 1] * BLOCKSIZ, SEEK_SET);
        fread(ibuf, 1, BLOCKSIZ, fd);
        for (i = 0; i < bcount - (NADDR - 1); i++)
            printf(" %d", ibuf[i]);
        printf(" " C_DIM "(indirect: %d)" C_RESET, inode->di_addr[NADDR - 1]);
    }
    printf("\n");

    iput(inode);
}
