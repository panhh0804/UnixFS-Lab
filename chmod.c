/* chmod.c: 修改文件权限 */
#include <stdio.h>
#include "filesys.h"

void chmod(uint16_t uid, const char *filename, uint16_t mode)
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

    /* 只有文件所有者或 root 才能修改权限 */
    if (user[uid].u_uid != 0 && inode->di_uid != user[uid].u_uid)
    {
        printf(C_BRED "\nPermission denied (only owner can chmod)\n" C_RESET);
        iput(inode);
        return;
    }

    /* 保留文件类型位，只修改低9位权限 */
    inode->di_mode = (inode->di_mode & ~0777) | (mode & 0777);
    inode->i_flag |= IUPDATE;
    printf(C_BGREEN "Permissions of '%s' changed to %04o\n" C_RESET, filename, inode->di_mode & 0777);
    iput(inode);
}
