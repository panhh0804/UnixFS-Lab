/* main.c: 文件系统主程序 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
extern int _dup(int oldfd);
extern int _dup2(int oldfd, int newfd);
extern int _close(int fd);
extern int _fileno(FILE *stream);
#undef access
#undef mkdir
#undef creat
#undef read
#undef write
#undef close
#undef rmdir
#undef chmod
#else
#ifndef _WIN32
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#endif
#endif

struct hinode hinode[NHINO];
struct dir dir;
struct file sys_ofile[SYSOPENFILE];
struct filsys filsys;
struct pwd pwd[PWDNUM];
struct user user[USERNUM];
FILE *fd;
struct inode *cur_path_inode;
int user_id = -1;
char current_path[256] = "/";
static int daemon_mode = 0;

#ifdef _WIN32
#define fs_dup _dup
#define fs_dup2 _dup2
#define fs_fileno _fileno
typedef SOCKET fs_socket_t;
#define FS_INVALID_SOCKET INVALID_SOCKET
static void fs_socket_close(fs_socket_t s) { closesocket(s); }
#else
extern int dup(int oldfd);
extern int dup2(int oldfd, int newfd);
extern int fileno(FILE *stream);
extern int unlink(const char *pathname);
extern long syscall(long number, ...);
#define fs_dup dup
#define fs_dup2 dup2
#define fs_fileno fileno
typedef int fs_socket_t;
#define FS_INVALID_SOCKET (-1)
static void fs_socket_close(fs_socket_t s) { syscall(SYS_close, s); }
#endif

static void show_help(void)
{
    printf(C_BOLD "\nCommands:\n" C_RESET);
    printf("  " C_BGREEN "login <uid> <password>" C_RESET "   User login\n");
    printf("  " C_BGREEN "logout" C_RESET "                   Logout current user\n");
    printf("  " C_BGREEN "dir" C_RESET "                      List current directory\n");
    printf("  " C_BGREEN "mkdir <dirname>" C_RESET "          Create directory\n");
    printf("  " C_BGREEN "cd <dirname>" C_RESET "             Change directory\n");
    printf("  " C_BGREEN "pwd" C_RESET "                      Print current directory\n");
    printf("  " C_BGREEN "rmdir <dirname>" C_RESET "          Remove empty directory\n");
    printf("  " C_BGREEN "create <filename> [mode] [content]" C_RESET "  Create file (default mode 0777)\n");
    printf("  " C_BGREEN "chmod <filename> <mode>" C_RESET "  Change file permissions\n");
    printf("  " C_BGREEN "ln <src> <dst>" C_RESET "           Create hard link\n");
    printf("  " C_BGREEN "symlink <target> <name>" C_RESET "  Create symbolic link\n");
    printf("  " C_BGREEN "open <filename> [mode]" C_RESET "   Open file (mode: read/write/append)\n");
    printf("  " C_BGREEN "close <fd>" C_RESET "               Close file descriptor\n");
    printf("  " C_BGREEN "read <fd> <size>" C_RESET "         Read bytes from file\n");
    printf("  " C_BGREEN "write <fd> <text>" C_RESET "        Write text to file\n");
    printf("  " C_BGREEN "delete <filename>" C_RESET "        Delete file\n");
    printf("  " C_BGREEN "cp <src> <dst>" C_RESET "           Copy file\n");
    printf("  " C_BGREEN "mv <src> <dst>" C_RESET "           Rename/move file\n");
    printf("  " C_BGREEN "cat <filename>" C_RESET "           Display file content\n");
    printf("  " C_BGREEN "lseek <fd> <offset>" C_RESET "      Seek to position in file\n");
    printf("  " C_BGREEN "stat <filename>" C_RESET "          Show file inode metadata\n");
    printf("  " C_BGREEN "wc <filename>" C_RESET "            Count lines/words/bytes\n");
    printf("  " C_BGREEN "find <path> [pattern]" C_RESET "    Recursively find files\n");
    printf("  " C_BGREEN "chown <uid> <filename>" C_RESET "   Change file owner\n");
    printf("  " C_BGREEN "df" C_RESET "                       Show disk free space\n");
    printf("  " C_BGREEN "format" C_RESET "                   Format file system\n");
    printf("  " C_BGREEN "exit / halt" C_RESET "              Save and exit\n");
    printf("  " C_BGREEN "help" C_RESET "                     Show this help\n\n");
}

static int parse_mode(const char *mode_str)
{
    if (strcmp(mode_str, "read") == 0)
        return FREAD;
    if (strcmp(mode_str, "write") == 0)
        return FWRITE;
    if (strcmp(mode_str, "append") == 0)
        return FWRITE | FAPPEND;
    return (int)strtol(mode_str, NULL, 8);
}

static void cmd_login(char *args)
{
    char uid_str[32], passwd[32];
    if (sscanf(args, "%31s %31s", uid_str, passwd) != 2)
    {
        printf(C_BYELLOW "Usage: login <uid> <password>\n" C_RESET);
        return;
    }
    if (login((uint16_t)atoi(uid_str), passwd))
    {
        printf(C_BGREEN "Login successful.\n" C_RESET);
    }
    else
    {
        printf(C_BRED "Login failed.\n" C_RESET);
    }
}

static void cmd_logout(void)
{
    if (user_id < 0)
    {
        printf(C_BYELLOW "No user logged in.\n" C_RESET);
        return;
    }
    logout(user[user_id].u_uid);
    user_id = -1;
    strcpy(current_path, "/");
    printf(C_BGREEN "Logged out.\n" C_RESET);
}

static void cmd_create(char *args)
{
    char filename[256];
    char mode_str[16] = "0777";
    char *content = NULL;
    uint16_t fd_num;
    int n;

    if (args == NULL || *args == '\0')
    {
        printf(C_BYELLOW "Usage: create <filename> [mode] [content]\n" C_RESET);
        return;
    }

    /* 提取文件名 */
    n = sscanf(args, "%255s", filename);
    if (n != 1)
    {
        printf("Usage: create <filename> [mode] [content]\n");
        return;
    }

    /* 查找第二个 token */
    content = strchr(args, ' ');
    if (content)
    {
        char *p = content + 1;
        while (*p == ' ') p++;
        /* 如果以数字开头，认为是模式 */
        if (*p >= '0' && *p <= '9')
        {
            int len = 0;
            while (p[len] && p[len] != ' ') len++;
            if (len > 0 && len < 16)
            {
                strncpy(mode_str, p, len);
                mode_str[len] = '\0';
                p += len;
                while (*p == ' ') p++;
            }
            content = (*p) ? p : NULL;
        }
        else
        {
            content = p;
        }
    }

    fd_num = creat(user_id, filename, (uint16_t)strtol(mode_str, NULL, 8));
    if (fd_num >= NOFILE)
    {
        printf(C_BRED "Create failed.\n" C_RESET);
        return;
    }

    if (content && *content != '\0')
    {
        write(fd_num, content, (uint16_t)strlen(content));
        printf(C_BGREEN "Created '%s' with content (%zu bytes), fd=%d\n" C_RESET, filename, strlen(content), fd_num);
    }
    else
    {
        printf(C_BGREEN "Created and opened '%s', fd=%d\n" C_RESET, filename, fd_num);
    }
}

static void cmd_chmod(char *args)
{
    char filename[256];
    char mode_str[16];
    if (sscanf(args, "%255s %15s", filename, mode_str) != 2)
    {
        printf(C_BYELLOW "Usage: chmod <filename> <mode>\n" C_RESET);
        return;
    }
    chmod(user_id, filename, (uint16_t)strtol(mode_str, NULL, 8));
}

static void cmd_ln(char *args)
{
    char src[256], dst[256];
    if (sscanf(args, "%255s %255s", src, dst) != 2)
    {
        printf(C_BYELLOW "Usage: ln <src> <dst>\n" C_RESET);
        return;
    }
    hlink(src, dst);
}

static void cmd_symlink(char *args)
{
    char target[256], name[256];
    if (sscanf(args, "%255s %255s", target, name) != 2)
    {
        printf(C_BYELLOW "Usage: symlink <target> <name>\n" C_RESET);
        return;
    }
    slink(target, name);
}

static void cmd_lseek(char *args)
{
    int fd_num;
    uint32_t offset;
    if (sscanf(args, "%d %u", &fd_num, &offset) != 2)
    {
        printf(C_BYELLOW "Usage: lseek <fd> <offset>\n" C_RESET);
        return;
    }
    if (lseek_file(user_id, (uint16_t)fd_num, offset) == 0)
        printf(C_BGREEN "Seeked fd=%d to offset=%u.\n" C_RESET, fd_num, offset);
}

static void cmd_cat(char *args)
{
    char filename[256];
    uint16_t ffd;
    char buf[BLOCKSIZ + 1];
    int n;
    if (sscanf(args, "%255s", filename) != 1)
    {
        printf(C_BYELLOW "Usage: cat <filename>\n" C_RESET);
        return;
    }
    ffd = aopen(user_id, filename, FREAD);
    if (ffd >= NOFILE)
    {
        printf(C_BRED "Cannot open '%s'.\n" C_RESET, filename);
        return;
    }
    while ((n = read(ffd, buf, BLOCKSIZ)) > 0)
    {
        buf[n] = '\0';
        printf("%s", buf);
    }
    printf("\n");
    printf(C_DIM "─────────────────────────────────────\n" C_RESET);
    close(user_id, ffd);
}

static void cmd_mv(char *args)
{
    char src[256], dst[256];
    if (sscanf(args, "%255s %255s", src, dst) != 2)
    {
        printf(C_BYELLOW "Usage: mv <src> <dst>\n" C_RESET);
        return;
    }
    mv(src, dst);
}

static uint32_t inode_total_count(void)
{
    return (uint32_t)(DINODEBLK * BLOCKSIZ / DINODESIZ);
}

static uint32_t scan_free_inode_count(void)
{
    uint32_t total_inodes = inode_total_count();
    uint32_t free_inodes = 0;
    uint32_t i;
    long old_pos;
    struct dinode dinode;

    /* 兜底扫描只读磁盘 dinode，不调用 iget/iput，避免刷新界面时改变 inode 空闲表。 */
    old_pos = ftell(fd);
    for (i = 0; i < total_inodes; i++)
    {
        fseek(fd, DINODESTART + (long)i * DINODESIZ, SEEK_SET);
        if (fread(&dinode, 1, sizeof(dinode), fd) != sizeof(dinode))
            break;
        if (dinode.di_number == 0 && dinode.di_mode == DIEMPTY)
            free_inodes++;
    }
    if (old_pos >= 0)
        fseek(fd, old_pos, SEEK_SET);

    return free_inodes;
}

static uint32_t free_inode_count_for_stats(uint32_t total_inodes)
{
    /* 正常情况下使用超级块计数；若旧镜像曾被污染导致计数越界，则扫描 inode 区兜底显示。 */
    if (filsys.s_ninode <= total_inodes)
        return filsys.s_ninode;
    return scan_free_inode_count();
}

static void cmd_df(void)
{
    uint32_t total_blocks = filsys.s_fsize;
    uint32_t free_blocks = filsys.s_nfree;
    uint32_t total_inodes = inode_total_count();
    uint32_t free_inodes = free_inode_count_for_stats(total_inodes);
    uint32_t used_blocks = total_blocks - free_blocks;
    uint32_t used_inodes = total_inodes - free_inodes;

    printf(C_BOLD "Disk usage:\n" C_RESET);
    printf("  Total blocks: %u  Used: " C_BRED "%u" C_RESET "  Free: " C_BGREEN "%u\n" C_RESET, total_blocks, used_blocks, free_blocks);
    printf("  Total inodes: %u  Used: " C_BRED "%u" C_RESET "  Free: " C_BGREEN "%u\n" C_RESET, total_inodes, used_inodes, free_inodes);
    printf("  Block usage: " C_BYELLOW "%.1f%%\n" C_RESET, total_blocks > 0 ? (used_blocks * 100.0 / total_blocks) : 0);
    printf("  Inode usage: " C_BYELLOW "%.1f%%\n" C_RESET, total_inodes > 0 ? (used_inodes * 100.0 / total_inodes) : 0);
}

static void cmd_stat(char *args)
{
    char filename[256];
    if (sscanf(args, "%255s", filename) != 1)
    {
        printf(C_BYELLOW "Usage: stat <filename>\n" C_RESET);
        return;
    }
    file_stat(filename);
}

static void cmd_wc(char *args)
{
    char filename[256];
    if (sscanf(args, "%255s", filename) != 1)
    {
        printf(C_BYELLOW "Usage: wc <filename>\n" C_RESET);
        return;
    }
    file_wc(filename);
}

static void cmd_find(char *args)
{
    char path[256];
    char pattern[256] = "";
    if (sscanf(args, "%255s %255s", path, pattern) < 1)
    {
        printf(C_BYELLOW "Usage: find <path> [pattern]\n" C_RESET);
        return;
    }
    file_find(path, pattern[0] ? pattern : NULL);
}

static void cmd_chown(char *args)
{
    char filename[256];
    char uid_str[16];
    if (sscanf(args, "%15s %255s", uid_str, filename) != 2)
    {
        printf(C_BYELLOW "Usage: chown <uid> <filename>\n" C_RESET);
        return;
    }
    chown_file(user_id, filename, (uint16_t)atoi(uid_str));
}

static void cmd_open(char *args)
{
    char filename[256];
    char mode_str[16] = "read";
    uint16_t fd_num;
    uint16_t mode;
    if (sscanf(args, "%255s %15s", filename, mode_str) < 1)
    {
        printf(C_BYELLOW "Usage: open <filename> [mode(read/write/append)]\n" C_RESET);
        return;
    }
    mode = (uint16_t)parse_mode(mode_str);
    if (mode == 0)
        mode = FREAD;
    fd_num = aopen(user_id, filename, mode);
    if (fd_num < NOFILE)
        printf(C_BGREEN "Opened '%s', fd=%d\n" C_RESET, filename, fd_num);
    else
        printf(C_BRED "Open failed.\n" C_RESET);
}

static void cmd_close(char *args)
{
    int fd_num;
    if (sscanf(args, "%d", &fd_num) != 1)
    {
        printf(C_BYELLOW "Usage: close <fd>\n" C_RESET);
        return;
    }
    close(user_id, (uint16_t)fd_num);
    printf(C_BGREEN "Closed fd=%d\n" C_RESET, fd_num);
}

static void cmd_read(char *args)
{
    int fd_num, size;
    char *buf;
    int n;
    if (sscanf(args, "%d %d", &fd_num, &size) != 2)
    {
        printf(C_BYELLOW "Usage: read <fd> <size>\n" C_RESET);
        return;
    }
    if (size <= 0 || size > 4096)
    {
        printf(C_BRED "Invalid size.\n" C_RESET);
        return;
    }
    buf = (char *)malloc(size + 1);
    n = read((uint16_t)fd_num, buf, (uint16_t)size);
    buf[n] = '\0';
    printf(C_BGREEN "Read %d bytes: " C_RESET "%s\n", n, buf);
    free(buf);
}

static void cmd_write(char *args)
{
    int fd_num;
    char *text;
    int n;
    if (args == NULL || *args == '\0')
    {
        printf(C_BYELLOW "Usage: write <fd> <text>\n" C_RESET);
        return;
    }
    fd_num = atoi(args);
    text = strchr(args, ' ');
    if (!text)
    {
        printf("Usage: write <fd> <text>\n");
        return;
    }
    text++; /* skip space */
    n = write((uint16_t)fd_num, text, (uint16_t)strlen(text));
    printf(C_BGREEN "Wrote %d bytes.\n" C_RESET, n);
}

static void cmd_delete(char *args)
{
    char filename[256];
    if (sscanf(args, "%255s", filename) != 1)
    {
        printf(C_BYELLOW "Usage: delete <filename>\n" C_RESET);
        return;
    }
    delete(filename);
}

static void cmd_cp(char *args)
{
    char src[256], dst[256];
    uint16_t sfd, dfd;
    char buf[BLOCKSIZ];
    int n;
    struct inode *inode;

    if (sscanf(args, "%255s %255s", src, dst) != 2)
    {
        printf(C_BYELLOW "Usage: cp <src> <dst>\n" C_RESET);
        return;
    }

    sfd = aopen(user_id, src, FREAD);
    if (sfd >= NOFILE)
    {
        printf(C_BRED "Cannot open source file.\n" C_RESET);
        return;
    }

    inode = sys_ofile[user[user_id].u_ofile[sfd]].f_inode;
    dfd = creat(user_id, dst, inode->di_mode);
    if (dfd >= NOFILE)
    {
        printf(C_BRED "Cannot create destination file.\n" C_RESET);
        close(user_id, sfd);
        return;
    }

    while ((n = read(sfd, buf, BLOCKSIZ)) > 0)
    {
        write(dfd, buf, (uint16_t)n);
    }

    close(user_id, sfd);
    close(user_id, dfd);
    printf(C_BGREEN "Copied '%s' to '%s'.\n" C_RESET, src, dst);
}

static void cmd_format(char *args)
{
    char confirm[8];
    if (daemon_mode && strcmp(args, "yes") == 0)
    {
        format();
        install();
        user_id = -1;
        strcpy(current_path, "/");
        printf(C_BGREEN "Formatted and reinstalled.\n" C_RESET);
        return;
    }

    printf(C_BYELLOW "Format will erase all data! Are you sure? (yes/no): " C_RESET);
    fflush(stdout);
    if (fgets(confirm, sizeof(confirm), stdin))
    {
        if (strncmp(confirm, "yes", 3) == 0)
        {
            format();
            install();
            user_id = -1;
            strcpy(current_path, "/");
            printf(C_BGREEN "Formatted and reinstalled.\n" C_RESET);
        }
        else
        {
            printf(C_BYELLOW "Format cancelled.\n" C_RESET);
        }
    }
}

static int require_login(void)
{
    if (user_id < 0)
    {
        printf(C_BYELLOW "Please login first.\n" C_RESET);
        return 0;
    }
    return 1;
}

static int push_current_path(const char *dirname)
{
    char next_path[sizeof(current_path)];

    if (strlen(current_path) > 1)
    {
        if (snprintf(next_path, sizeof(next_path), "%s/%s", current_path, dirname) >= (int)sizeof(next_path))
            return 0;
    }
    else
    {
        if (snprintf(next_path, sizeof(next_path), "/%s", dirname) >= (int)sizeof(next_path))
            return 0;
    }

    strcpy(current_path, next_path);
    return 1;
}

static int current_path_can_push(const char *dirname)
{
    size_t need = strlen(current_path) + strlen(dirname) + 1;
    if (strlen(current_path) > 1)
        need++;
    return need < sizeof(current_path);
}

static int parse_single_name_arg(const char *args, char *name, size_t size, const char *usage)
{
    char extra[2];

    if (sscanf(args, "%255s %1s", name, extra) != 1)
    {
        printf("%s\n", usage);
        return 0;
    }
    if (strlen(name) >= size)
    {
        printf(C_BRED "Argument too long.\n" C_RESET);
        return 0;
    }
    return 1;
}

static const char *inode_type_name(uint16_t mode)
{
    if (mode & DIDIR)
        return "dir";
    if (mode & DILINK)
        return "link";
    if (mode & DIFILE)
        return "file";
    return "unknown";
}

static void print_inode_blocks(struct inode *inode)
{
    uint16_t nblocks;
    uint16_t ibuf[BLOCKSIZ / sizeof(uint16_t)];
    int i;
    int printed = 0;

    nblocks = (uint16_t)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
    for (i = 0; i < NADDR - 1 && i < nblocks; i++)
    {
        if (inode->di_addr[i] != 0 || inode->i_ino == 1)
        {
            if (printed)
                printf(",");
            printf("%u", inode->di_addr[i]);
            printed = 1;
        }
    }

    if (inode->di_addr[NADDR - 1] != 0)
    {
        if (printed)
            printf(",");
        printf("%u(I)", inode->di_addr[NADDR - 1]);
        printed = 1;

        fseek(fd, DATASTART + inode->di_addr[NADDR - 1] * BLOCKSIZ, SEEK_SET);
        fread(ibuf, 1, BLOCKSIZ, fd);
        for (i = 0; i < BLOCKSIZ / (int)sizeof(uint16_t) && i + NADDR - 1 < nblocks; i++)
        {
            if (ibuf[i] != 0)
                printf(",%u", ibuf[i]);
        }
    }
}

static void cmd_internal_disk_map(void)
{
    int occupied[FILEBLK] = {0};
    uint16_t ibuf[BLOCKSIZ / sizeof(uint16_t)];
    int max_inode = DINODEBLK * BLOCKSIZ / DINODESIZ;
    int i, j, nblocks;
    struct inode *inode;

    /* 这里的 block 0/1/2 是数据区相对块号，分别保存 /、/etc 和 /etc/password。 */
    occupied[0] = occupied[1] = occupied[2] = 1;
    for (i = 0; i < max_inode; i++)
    {
        inode = iget((uint16_t)i);
        if (inode && inode->di_mode != DIEMPTY)
        {
            nblocks = (int)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
            for (j = 0; j < NADDR - 1 && j < nblocks; j++)
            {
                if (inode->di_addr[j] < FILEBLK)
                    occupied[inode->di_addr[j]] = 1;
            }
            if (inode->di_addr[NADDR - 1] != 0 && inode->di_addr[NADDR - 1] < FILEBLK)
            {
                occupied[inode->di_addr[NADDR - 1]] = 1;
                fseek(fd, DATASTART + inode->di_addr[NADDR - 1] * BLOCKSIZ, SEEK_SET);
                fread(ibuf, 1, BLOCKSIZ, fd);
                for (j = 0; j < BLOCKSIZ / (int)sizeof(uint16_t) && j + NADDR - 1 < nblocks; j++)
                {
                    if (ibuf[j] < FILEBLK)
                        occupied[ibuf[j]] = 1;
                }
            }
        }
        iput(inode);
    }

    for (i = 0; i < FILEBLK; i++)
        printf("%d%c", occupied[i], i == FILEBLK - 1 ? '\n' : ' ');
}

static void cmd_internal_inode_list(void)
{
    int max_inode = DINODEBLK * BLOCKSIZ / DINODESIZ;
    int i;
    struct inode *inode;

    printf("ino|type|size|mode|uid|gid|links|blocks\n");
    for (i = 0; i < max_inode; i++)
    {
        inode = iget((uint16_t)i);
        if (inode && inode->di_mode != DIEMPTY)
        {
            printf("%u|%s|%lu|%04o|%u|%u|%u|",
                   inode->i_ino,
                   inode_type_name(inode->di_mode),
                   (unsigned long)inode->di_size,
                   inode->di_mode & 0777,
                   inode->di_uid,
                   inode->di_gid,
                   inode->di_number);
            print_inode_blocks(inode);
            printf("\n");
        }
        iput(inode);
    }
}

static void cmd_internal_dir_list(void)
{
    int i;
    struct inode *inode;

    printf("name|ino|type|size|mode\n");
    for (i = 0; i < dir.size; i++)
    {
        if (dir.direct[i].d_ino == 0)
            continue;
        inode = iget(dir.direct[i].d_ino);
        if (inode)
        {
            printf("%s|%u|%s|%lu|%04o\n",
                   dir.direct[i].d_name,
                   dir.direct[i].d_ino,
                   inode_type_name(inode->di_mode),
                   (unsigned long)inode->di_size,
                   inode->di_mode & 0777);
        }
        iput(inode);
    }
}

static void print_tree_rec(uint16_t ino, int depth, const char *name)
{
    struct inode *inode;
    struct direct entries[DIRNUM];
    int nentries, nblocks;
    int i, j, e;

    inode = iget(ino);
    if (!inode)
        return;

    for (i = 0; i < depth; i++)
        printf("  ");
    printf("%s%s\n", name, (inode->di_mode & DIDIR) ? "/" : "");

    if (!(inode->di_mode & DIDIR) || depth >= 8)
    {
        iput(inode);
        return;
    }

    memset(entries, 0, sizeof(entries));
    nentries = inode->di_size / sizeof(struct direct);
    if (nentries > DIRNUM)
        nentries = DIRNUM;
    nblocks = (int)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
    e = 0;
    for (i = 0; i < nblocks && i < NADDR && e < DIRNUM; i++)
    {
        fseek(fd, DATASTART + inode->di_addr[i] * BLOCKSIZ, SEEK_SET);
        fread(&entries[e], 1, BLOCKSIZ, fd);
        e += BLOCKSIZ / sizeof(struct direct);
    }

    for (j = 0; j < nentries; j++)
    {
        if (entries[j].d_ino == 0 ||
            strcmp(entries[j].d_name, ".") == 0 ||
            strcmp(entries[j].d_name, "..") == 0)
            continue;
        print_tree_rec(entries[j].d_ino, depth + 1, entries[j].d_name);
    }

    iput(inode);
}

static void cmd_internal_tree(void)
{
    print_tree_rec(1, 0, "/");
}

static void cmd_internal_stats(void)
{
    uint32_t total_blocks = filsys.s_fsize;
    uint32_t free_blocks = filsys.s_nfree;
    uint32_t total_inodes = inode_total_count();
    uint32_t free_inodes = free_inode_count_for_stats(total_inodes);

    printf("total_blocks=%u\n", total_blocks);
    printf("used_blocks=%u\n", total_blocks - free_blocks);
    printf("free_blocks=%u\n", free_blocks);
    printf("total_inodes=%u\n", total_inodes);
    printf("used_inodes=%u\n", total_inodes - free_inodes);
    printf("free_inodes=%u\n", free_inodes);
    printf("user=%d\n", user_id >= 0 ? user[user_id].u_uid : 0);
    printf("path=%s\n", current_path);
}

static void cmd_internal_block_info(void)
{
    uint16_t ibuf[BLOCKSIZ / sizeof(uint16_t)];
    int max_inode = DINODEBLK * BLOCKSIZ / DINODESIZ;
    int i, j, nblocks;
    struct inode *inode;

    /* 供前端点击磁盘块、查看块链使用：输出 block -> inode/角色/逻辑序号。 */
    printf("block|ino|role|index\n");
    for (i = 0; i < 3; i++)
        printf("%d|0|system|%d\n", i, i);

    for (i = 0; i < max_inode; i++)
    {
        inode = iget((uint16_t)i);
        if (inode && inode->di_mode != DIEMPTY)
        {
            nblocks = (int)((inode->di_size + BLOCKSIZ - 1) / BLOCKSIZ);
            for (j = 0; j < NADDR - 1 && j < nblocks; j++)
            {
                if (inode->di_addr[j] >= 3 && inode->di_addr[j] < FILEBLK)
                    printf("%d|%d|data|%d\n", inode->di_addr[j], i, j);
            }
            if (inode->di_addr[NADDR - 1] != 0 && inode->di_addr[NADDR - 1] < FILEBLK)
            {
                printf("%d|%d|indirect|%d\n", inode->di_addr[NADDR - 1], i, NADDR - 1);
                fseek(fd, DATASTART + inode->di_addr[NADDR - 1] * BLOCKSIZ, SEEK_SET);
                fread(ibuf, 1, BLOCKSIZ, fd);
                for (j = 0; j < BLOCKSIZ / (int)sizeof(uint16_t) && j + NADDR - 1 < nblocks; j++)
                {
                    if (ibuf[j] >= 3 && ibuf[j] < FILEBLK)
                        printf("%d|%d|indirect_data|%d\n", ibuf[j], i, j + NADDR - 1);
                }
            }
        }
        iput(inode);
    }
}

static void cmd_internal_snapshot(void)
{
    /* 前端每次刷新只发一次 __snapshot__，后端分段输出所有可视化需要的状态。 */
    printf("__SECTION__ disk\n");
    cmd_internal_disk_map();
    printf("__SECTION__ inodes\n");
    cmd_internal_inode_list();
    printf("__SECTION__ dir\n");
    cmd_internal_dir_list();
    printf("__SECTION__ tree\n");
    cmd_internal_tree();
    printf("__SECTION__ stats\n");
    cmd_internal_stats();
    printf("__SECTION__ blocks\n");
    cmd_internal_block_info();
}

static void process_cmd(char *cmdline)
{
    char cmd[32];
    char *args;
    int n;

    /* trim newline */
    cmdline[strcspn(cmdline, "\n")] = '\0';

    n = sscanf(cmdline, "%31s", cmd);
    if (n != 1)
        return;

    args = strchr(cmdline, ' ');
    if (args)
        args++; /* skip leading space */
    else
        args = "";

    if (strcmp(cmd, "__disk_map__") == 0)
    {
        cmd_internal_disk_map();
    }
    else if (strcmp(cmd, "__inode_list__") == 0)
    {
        cmd_internal_inode_list();
    }
    else if (strcmp(cmd, "__dir_list__") == 0)
    {
        cmd_internal_dir_list();
    }
    else if (strcmp(cmd, "__tree__") == 0)
    {
        cmd_internal_tree();
    }
    else if (strcmp(cmd, "__stats__") == 0)
    {
        cmd_internal_stats();
    }
    else if (strcmp(cmd, "__block_info__") == 0)
    {
        cmd_internal_block_info();
    }
    else if (strcmp(cmd, "__snapshot__") == 0)
    {
        cmd_internal_snapshot();
    }
    else if (strcmp(cmd, "help") == 0)
    {
        show_help();
    }
    else if (strcmp(cmd, "login") == 0)
    {
        cmd_login(args);
    }
    else if (strcmp(cmd, "logout") == 0)
    {
        cmd_logout();
    }
    else if (strcmp(cmd, "dir") == 0)
    {
        if (!require_login()) return;
        _dir();
    }
    else if (strcmp(cmd, "mkdir") == 0)
    {
        char dirname[256];
        if (!require_login()) return;
        if (parse_single_name_arg(args, dirname, sizeof(dirname), "Usage: mkdir <dirname>"))
            mkdir(dirname);
    }
    else if (strcmp(cmd, "cd") == 0)
    {
        char dirname[256];
        if (!require_login()) return;
        if (!parse_single_name_arg(args, dirname, sizeof(dirname), "Usage: cd <dirname>"))
            return;
        if (strcmp(dirname, "..") != 0 && strcmp(dirname, ".") != 0 && !current_path_can_push(dirname))
        {
            printf(C_BRED "Path is too long.\n" C_RESET);
            return;
        }
        if (chdir(dirname))
        {
            /* 更新 current_path */
            if (strcmp(dirname, "..") == 0)
            {
                /* 返回上级：去掉最后一级目录 */
                char *last_slash = strrchr(current_path, '/');
                if (last_slash && last_slash != current_path)
                {
                    *last_slash = '\0';
                }
                else if (last_slash == current_path)
                {
                    current_path[1] = '\0';
                }
            }
            else if (strcmp(dirname, ".") != 0)
            {
                /* 进入子目录 */
                if (!push_current_path(dirname))
                    printf(C_BYELLOW "Path display is too long; current path was not updated.\n" C_RESET);
            }
        }
    }
    else if (strcmp(cmd, "pwd") == 0)
    {
        if (!require_login()) return;
        printf("%s\n", current_path);
    }
    else if (strcmp(cmd, "rmdir") == 0)
    {
        char dirname[256];
        if (!require_login()) return;
        if (parse_single_name_arg(args, dirname, sizeof(dirname), "Usage: rmdir <dirname>"))
            rmdir(dirname);
    }
    else if (strcmp(cmd, "create") == 0)
    {
        if (!require_login()) return;
        cmd_create(args);
    }
    else if (strcmp(cmd, "open") == 0)
    {
        if (!require_login()) return;
        cmd_open(args);
    }
    else if (strcmp(cmd, "close") == 0)
    {
        if (!require_login()) return;
        cmd_close(args);
    }
    else if (strcmp(cmd, "read") == 0)
    {
        if (!require_login()) return;
        cmd_read(args);
    }
    else if (strcmp(cmd, "write") == 0)
    {
        if (!require_login()) return;
        cmd_write(args);
    }
    else if (strcmp(cmd, "delete") == 0)
    {
        if (!require_login()) return;
        cmd_delete(args);
    }
    else if (strcmp(cmd, "cp") == 0)
    {
        if (!require_login()) return;
        cmd_cp(args);
    }
    else if (strcmp(cmd, "chmod") == 0)
    {
        if (!require_login()) return;
        cmd_chmod(args);
    }
    else if (strcmp(cmd, "ln") == 0)
    {
        if (!require_login()) return;
        cmd_ln(args);
    }
    else if (strcmp(cmd, "symlink") == 0)
    {
        if (!require_login()) return;
        cmd_symlink(args);
    }
    else if (strcmp(cmd, "lseek") == 0)
    {
        if (!require_login()) return;
        cmd_lseek(args);
    }
    else if (strcmp(cmd, "cat") == 0)
    {
        if (!require_login()) return;
        cmd_cat(args);
    }
    else if (strcmp(cmd, "mv") == 0)
    {
        if (!require_login()) return;
        cmd_mv(args);
    }
    else if (strcmp(cmd, "df") == 0)
    {
        if (!require_login()) return;
        cmd_df();
    }
    else if (strcmp(cmd, "stat") == 0)
    {
        if (!require_login()) return;
        cmd_stat(args);
    }
    else if (strcmp(cmd, "wc") == 0)
    {
        if (!require_login()) return;
        cmd_wc(args);
    }
    else if (strcmp(cmd, "find") == 0)
    {
        if (!require_login()) return;
        cmd_find(args);
    }
    else if (strcmp(cmd, "chown") == 0)
    {
        if (!require_login()) return;
        cmd_chown(args);
    }
    else if (strcmp(cmd, "format") == 0)
    {
        cmd_format(args);
    }
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "halt") == 0)
    {
        halt();
    }
    else
    {
        printf(C_BRED "Unknown command: %s. Type 'help' for help.\n" C_RESET, cmd);
    }
}

static void send_all(fs_socket_t client_fd, const char *buf, size_t len)
{
    size_t sent = 0;
    int n;

    while (sent < len)
    {
        n = send(client_fd, buf + sent, (int)(len - sent), 0);
        if (n <= 0)
            return;
        sent += (size_t)n;
    }
}

static void handle_daemon_command(fs_socket_t client_fd, char *cmdline)
{
    FILE *capture;
    int saved_stdout;
    char outbuf[4096];
    size_t nread;

    capture = tmpfile();
    if (!capture)
    {
        send_all(client_fd, "capture failed\n__END__\n", 23);
        return;
    }

    fflush(stdout);
    saved_stdout = fs_dup(fs_fileno(stdout));
    if (saved_stdout < 0)
    {
        fclose(capture);
        send_all(client_fd, "dup stdout failed\n__END__\n", 26);
        return;
    }

    fs_dup2(fs_fileno(capture), fs_fileno(stdout));
    process_cmd(cmdline);
    fflush(stdout);
    fs_dup2(saved_stdout, fs_fileno(stdout));
#ifdef _WIN32
    _close(saved_stdout);
#else
    syscall(SYS_close, saved_stdout);
#endif

    rewind(capture);
    while ((nread = fread(outbuf, 1, sizeof(outbuf), capture)) > 0)
        send_all(client_fd, outbuf, nread);
    fclose(capture);

    send_all(client_fd, "__END__\n", 8);
}

static void handle_client(fs_socket_t client_fd)
{
    char recvbuf[4096];
    char line[8192];
    size_t used = 0;
    int n;
    char *newline;
    size_t line_len;

    while ((n = recv(client_fd, recvbuf, (int)sizeof(recvbuf) - 1, 0)) > 0)
    {
        recvbuf[n] = '\0';
        if (used + (size_t)n >= sizeof(line))
            used = 0;
        memcpy(line + used, recvbuf, (size_t)n + 1);
        used += (size_t)n;

        while ((newline = memchr(line, '\n', used)) != NULL)
        {
            line_len = (size_t)(newline - line);
            line[line_len] = '\0';
            handle_daemon_command(client_fd, line);
            memmove(line, newline + 1, used - line_len - 1);
            used -= line_len + 1;
            line[used] = '\0';
        }
    }
}

static int ensure_disk_loaded(void)
{
    FILE *probe = fopen("disk.img", "rb");
    if (!probe)
        format();
    else
        fclose(probe);

    install();
    return 1;
}

static int run_tcp_daemon(const char *host, unsigned short port)
{
    fs_socket_t server_fd;
    fs_socket_t client_fd;
    struct sockaddr_in addr;
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    daemon_mode = 1;
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    ensure_disk_loaded();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == FS_INVALID_SOCKET)
    {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);
    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        fs_socket_close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0)
    {
        perror("listen");
        fs_socket_close(server_fd);
        return 1;
    }

    printf("fs daemon listening on %s:%u\n", host, port);
    fflush(stdout);

    while (1)
    {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == FS_INVALID_SOCKET)
            break;
        handle_client(client_fd);
        fs_socket_close(client_fd);
    }

    fs_socket_close(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

#ifndef _WIN32
static int run_unix_daemon(const char *sock_path)
{
    fs_socket_t server_fd;
    fs_socket_t client_fd;
    struct sockaddr_un addr;

    daemon_mode = 1;
    signal(SIGPIPE, SIG_IGN);
    ensure_disk_loaded();

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    unlink(sock_path);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        fs_socket_close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0)
    {
        perror("listen");
        fs_socket_close(server_fd);
        return 1;
    }

    printf("fs daemon listening on %s\n", sock_path);
    fflush(stdout);

    while (1)
    {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            perror("accept");
            break;
        }
        handle_client(client_fd);
        fs_socket_close(client_fd);
    }

    fs_socket_close(server_fd);
    unlink(sock_path);
    return 0;
}
#endif

int main(int argc, char *argv[])
{
    char cmdline[8192];
    char answer[8];
    const char *tcp_host = "127.0.0.1";
    unsigned short tcp_port = 9090;
    int use_tcp = 0;
    int i;
#ifndef _WIN32
    const char *sock_path = "/tmp/fs_daemon.sock";
#endif

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--tcp") == 0)
        {
            use_tcp = 1;
        }
        else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc)
        {
            tcp_host = argv[++i];
        }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            tcp_port = (unsigned short)atoi(argv[++i]);
            use_tcp = 1;
        }
        else if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc)
        {
#ifndef _WIN32
            sock_path = argv[++i];
#else
            i++;
#endif
            use_tcp = 0;
        }
        if (strcmp(argv[i], "--daemon") == 0)
        {
#ifdef _WIN32
            use_tcp = 1;
#else
            /* Linux defaults to Unix socket unless --tcp/--port is provided. */
#endif
            continue;
        }
    }

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--daemon") == 0)
        {
            if (use_tcp)
                return run_tcp_daemon(tcp_host, tcp_port);
#ifdef _WIN32
            return run_tcp_daemon(tcp_host, tcp_port);
#else
            return run_unix_daemon(sock_path);
#endif
        }
    }

    printf(C_BBLUE "=====================================\n" C_RESET);
    printf(C_BBLUE "  UNIX-like File System Simulator   \n" C_RESET);
    printf(C_BBLUE "=====================================\n\n" C_RESET);

    printf("Do you want to format the disk? (yes/no): ");
    fflush(stdout);
    if (fgets(answer, sizeof(answer), stdin) && strncmp(answer, "yes", 3) == 0)
    {
        format();
    }

    install();
    printf(C_BGREEN "File system loaded. Type 'help' for commands.\n\n" C_RESET);

    while (1)
    {
        if (user_id >= 0)
            printf(C_BBLUE "[%d:%s]$ " C_RESET, user[user_id].u_uid, current_path);
        else
            printf(C_BBLUE "[guest:%s]$ " C_RESET, current_path);
        fflush(stdout);

        if (!fgets(cmdline, sizeof(cmdline), stdin))
            halt();

        process_cmd(cmdline);
    }

    return 0;
}
