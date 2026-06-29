/* delete.c: 删除文件 */
#include <stdio.h>
#include <string.h>
#include "filesys.h"

void delete(const char *filename)
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
    if (!access(user_id, inode, WRITE))
    {
        printf(C_BRED "\nPermission denied\n" C_RESET);
        iput(inode);
        return;
    }

    if (inode->di_mode & DIDIR)
    {
        printf(C_BRED "\n%s is a directory, use rmdir instead\n" C_RESET, filename);
        iput(inode);
        return;
    }

    inode->di_number--;
    iput(inode);

    /* 从目录中移除 */
    dir_entry_remove(dinodeid);
}
