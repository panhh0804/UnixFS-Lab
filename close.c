/* close.c: 关闭文件 */
#include <stdio.h>
#include "filesys.h"

void close(uint16_t uid, uint16_t cfd)
{
    struct inode *inode;
    uint16_t sys_fd;

    if (cfd >= NOFILE)
        return;
    sys_fd = user[uid].u_ofile[cfd];
    if (sys_fd >= SYSOPENFILE)
        return;

    inode = sys_ofile[sys_fd].f_inode;
    if (sys_ofile[sys_fd].f_count > 0)
        sys_ofile[sys_fd].f_count--;

    if (sys_ofile[sys_fd].f_count == 0)
    {
        iput(inode);
        sys_ofile[sys_fd].f_inode = NULL;
        sys_ofile[sys_fd].f_flag = 0;
    }

    user[uid].u_ofile[cfd] = SYSOPENFILE + 1;
}
