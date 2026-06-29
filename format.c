/* format.c: 格式化文件系统 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys.h"
#include "superblock.h"

static void clear_inode_cache(void)
{
    int i;
    struct inode *node, *next;

    for (i = 0; i < NHINO; i++)
    {
        node = hinode[i].i_forw;
        while (node)
        {
            next = node->i_forw;
            free(node);
            node = next;
        }
        hinode[i].i_forw = NULL;
    }
    cur_path_inode = NULL;
}

void format(void)
{
    struct inode *inode;
    struct direct dir_buf[BLOCKSIZ / sizeof(struct direct)];
    struct pwd passwd[BLOCKSIZ / (PWDSIZ + 4)];
    char *buf;

    /* 关闭旧虚拟盘，释放内存中的 inode 缓存 */
    if (fd)
    {
        fclose(fd);
        fd = NULL;
    }
    clear_inode_cache();

    /* 创建虚拟盘文件 */
    fd = fopen("disk.img", "w+b");
    if (!fd)
    {
        printf("\nFile system file creation failed!\n");
        exit(1);
    }

    buf = (char *)malloc((DINODEBLK + FILEBLK + 2) * BLOCKSIZ);
    if (!buf)
    {
        printf("\nMemory allocation failed!\n");
        exit(1);
    }
    memset(buf, 0, (DINODEBLK + FILEBLK + 2) * BLOCKSIZ);
    fseek(fd, 0, SEEK_SET);
    fwrite(buf, 1, (DINODEBLK + FILEBLK + 2) * BLOCKSIZ, fd);
    free(buf);

    /* 初始化密码表。该表稍后写入 /etc/password 文件的数据块。
     * root 用户 uid/gid 均为 0；普通用户用于权限和多用户演示。
     */
    memset(passwd, 0, sizeof(passwd));
    passwd[0].p_uid = 0;    passwd[0].p_gid = 00; strcpy(passwd[0].password, "root");
    passwd[1].p_uid = 2116; passwd[1].p_gid = 03; strcpy(passwd[1].password, "dddd");
    passwd[2].p_uid = 2117; passwd[2].p_gid = 03; strcpy(passwd[2].password, "bbbb");
    passwd[3].p_uid = 2118; passwd[3].p_gid = 04; strcpy(passwd[3].password, "abcd");
    passwd[4].p_uid = 2119; passwd[4].p_gid = 04; strcpy(passwd[4].password, "cccc");
    passwd[5].p_uid = 2220; passwd[5].p_gid = 05; strcpy(passwd[5].password, "eeee");

    /* 0# inode: 保留空 inode。它不作为普通文件使用。 */
    inode = iget(0);
    inode->di_number = 1;
    inode->di_mode = DIEMPTY;
    iput(inode);

    /* 1# inode: 根目录 /。
     * 根目录的 . 和 .. 都指向自身；etc 目录项指向 inode 2。
     */
    inode = iget(1);
    inode->di_number = 1;
    inode->di_mode = DEFAULTMODE | DIDIR;
    inode->di_uid = 0;
    inode->di_gid = 0;
    inode->di_size = 3 * sizeof(struct direct);
    inode->di_addr[0] = 0; /* 数据块 0# 被根目录使用 */
    memset(dir_buf, 0, BLOCKSIZ);
    strcpy(dir_buf[0].d_name, "..");  dir_buf[0].d_ino = 1;
    strcpy(dir_buf[1].d_name, ".");   dir_buf[1].d_ino = 1;
    strcpy(dir_buf[2].d_name, "etc"); dir_buf[2].d_ino = 2;
    fseek(fd, DATASTART, SEEK_SET);
    fwrite(dir_buf, 1, 3 * sizeof(struct direct), fd);
    iput(inode);

    /* 2# inode: /etc 目录。
     * /etc 中的 password 目录项指向 inode 3。
     */
    inode = iget(2);
    inode->di_number = 1;
    inode->di_mode = DEFAULTMODE | DIDIR;
    inode->di_uid = 0;
    inode->di_gid = 0;
    inode->di_size = 3 * sizeof(struct direct);
    inode->di_addr[0] = 1; /* 数据块 1# 被 etc 使用 */
    memset(dir_buf, 0, BLOCKSIZ);
    strcpy(dir_buf[0].d_name, "..");      dir_buf[0].d_ino = 1;
    strcpy(dir_buf[1].d_name, ".");       dir_buf[1].d_ino = 2;
    strcpy(dir_buf[2].d_name, "password"); dir_buf[2].d_ino = 3;
    fseek(fd, DATASTART + BLOCKSIZ * 1, SEEK_SET);
    fwrite(dir_buf, 1, 3 * sizeof(struct direct), fd);
    iput(inode);

    /* 3# inode: /etc/password 文件。
     * 用户表写入数据块 2，后续 install() 从这里读入内存 pwd[]。
     */
    inode = iget(3);
    inode->di_number = 1;
    inode->di_mode = DEFAULTMODE | DIFILE;
    inode->di_uid = 0;
    inode->di_gid = 0;
    inode->di_size = BLOCKSIZ;
    inode->di_addr[0] = 2; /* 数据块 2# 存放密码 */
    memset(passwd + 6, 0, sizeof(struct pwd) * (PWDNUM - 6));
    fseek(fd, DATASTART + 2 * BLOCKSIZ, SEEK_SET);
    fwrite(passwd, 1, BLOCKSIZ, fd);
    iput(inode);

    /* 初始化超级块与成组链接法 */
    sb_init_format();

    /* 关闭文件，确保所有数据落盘 */
    fclose(fd);
    fd = NULL;
}
