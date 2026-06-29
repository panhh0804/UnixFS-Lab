/* lseek.c: 文件指针定位 */
#include <stdio.h>
#include "filesys.h"

int lseek_file(uint16_t uid, uint16_t cfd, uint32_t offset)
{
    uint16_t sys_fd;

    if (cfd >= NOFILE)
    {
        printf("\nInvalid file descriptor\n");
        return -1;
    }
    sys_fd = user[uid].u_ofile[cfd];
    if (sys_fd >= SYSOPENFILE)
    {
        printf("\nInvalid file descriptor\n");
        return -1;
    }

    if (offset > sys_ofile[sys_fd].f_inode->di_size)
        offset = sys_ofile[sys_fd].f_inode->di_size;

    sys_ofile[sys_fd].f_off = offset;
    return 0;
}
