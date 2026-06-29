/* access.c: 访问权限检查（标准 UNIX 权限模型） */
#include <stdio.h>
#include "filesys.h"

int access(uint16_t uid, struct inode *inode, uint16_t mode)
{
    uint16_t perm_bits;

    /* root 特权：uid==0 时绕过普通 rwx 权限检查。
     * 这里只跳过权限检查，不跳过“文件不存在、目录非空、磁盘满”等结构性错误。
     */
    if (user[uid].u_uid == 0)
        return 1;

    /* 1. owner 匹配：只看 owner 权限位，不再 fallback 到 group/other。 */
    if (user[uid].u_uid == inode->di_uid)
    {
        switch (mode)
        {
        case READ:    perm_bits = UDIREAD;    break;
        case WRITE:   perm_bits = UDIWRITE;   break;
        case EXECUTE: perm_bits = UDIEXECUTE; break;
        default:      return 0;
        }
        return (inode->di_mode & perm_bits) ? 1 : 0;
    }

    /* 2. group 匹配：当前用户不是 owner，但 gid 与文件 gid 相同。 */
    if (user[uid].u_gid == inode->di_gid)
    {
        switch (mode)
        {
        case READ:    perm_bits = GDIREAD;    break;
        case WRITE:   perm_bits = GDIWRITE;   break;
        case EXECUTE: perm_bits = GDIEXECUTE; break;
        default:      return 0;
        }
        return (inode->di_mode & perm_bits) ? 1 : 0;
    }

    /* 3. other：既不是 owner，也不同组，最后使用 other 权限位。 */
    switch (mode)
    {
    case READ:    perm_bits = ODIREAD;    break;
    case WRITE:   perm_bits = ODIWRITE;   break;
    case EXECUTE: perm_bits = ODIEXECUTE; break;
    default:      return 0;
    }
    return (inode->di_mode & perm_bits) ? 1 : 0;
}
