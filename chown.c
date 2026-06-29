/* chown.c: 修改文件所有者 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

void chown_file(uint16_t uid, const char *filename, uint16_t new_uid)
{
    uint16_t dinodeid;
    struct inode *inode;

    dinodeid = namei(filename);
    if (dinodeid >= DIRNUM)
    {
        printf(C_BRED "\n%s does not exist\n" C_RESET, filename);
        return;
    }

    inode = iget(dir.direct[dinodeid].d_ino);

    /* 只有 root (uid==0) 或文件所有者才能修改所有者 */
    if (user[uid].u_uid != 0 && user[uid].u_uid != inode->di_uid)
    {
        printf(C_BRED "\nPermission denied (only owner or root can chown)\n" C_RESET);
        iput(inode);
        return;
    }

    inode->di_uid = new_uid;
    inode->i_flag |= IUPDATE;
    iput(inode);
    printf(C_BGREEN "Owner of '%s' changed to %d.\n" C_RESET, filename, new_uid);
}
