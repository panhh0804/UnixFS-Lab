#ifndef FILESYS_H
#define FILESYS_H

#include <stdio.h>
#include <stdint.h>

/* ANSI 颜色宏 */
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[2m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_CYAN    "\033[36m"
#define C_WHITE   "\033[37m"
#define C_BRED    "\033[1;31m"
#define C_BGREEN  "\033[1;32m"
#define C_BYELLOW "\033[1;33m"
#define C_BBLUE   "\033[1;34m"
#define C_BCYAN   "\033[1;36m"
#define C_BWHITE  "\033[1;37m"

/* 虚拟磁盘布局：
 * block 0        : 保留块
 * block 1        : 超级块 struct filsys
 * block 2~33     : inode 区，共 DINODEBLK 个块
 * block 34 以后  : 数据区，共 FILEBLK 个数据块
 */
#define BLOCKSIZ    512  /* 每个磁盘块 512 字节 */
#define SYSOPENFILE 40   /* 系统级打开文件表容量 */
#define DIRNUM      128  /* 单个目录最多缓存的目录项数 */
#define DIRSIZ      14   /* 目录项中文件名最大长度 */
#define PWDSIZ      12   /* 密码最大长度 */
#define PWDNUM      32   /* /etc/password 中最多用户数 */
#define NOFILE      20   /* 单个用户最多打开文件数 */
#define NADDR       10   /* inode 地址项数量：9 个直接索引 + 1 个一级间接索引 */
#define NHINO       128  /* 内存 inode hash 桶数量 */
#define USERNUM     10   /* 同时登录用户槽数量 */
#define DINODESIZ   32   /* 磁盘 inode 固定 32 字节 */
#define DINODEBLK   32   /* inode 区固定占 32 个块 */
#define FILEBLK     512  /* 数据区固定 512 个数据块 */
#define NICFREE     50   /* 成组链接法中超级块一次缓存 50 个空闲块号 */
#define NICINOD     50   /* 超级块一次缓存 50 个空闲 inode 号 */
#define DINODESTART 2*BLOCKSIZ
#define DATASTART   (2+DINODEBLK)*BLOCKSIZ
#define DIEMPTY     00000
#define DIFILE      01000
#define DIDIR       02000
#define DILINK      04000
#define UDIREAD     00400
#define UDIWRITE    00200
#define UDIEXECUTE  00100
#define GDIREAD     00040
#define GDIWRITE    00020
#define GDIEXECUTE  00010
#define ODIREAD     00004
#define ODIWRITE    00002
#define ODIEXECUTE  00001
#define READ        1
#define WRITE       2
#define EXECUTE     3
#define DEFAULTMODE 00777
#define IUPDATE     00002
#define SUPDATE     00001
#define FREAD       00001
#define FWRITE      00002
#define FAPPEND     00004
#define DISKFULL    65535

/* 所有结构体使用紧凑布局，避免编译器填充导致磁盘映射错误 */
#pragma pack(push, 1)

/* 内存中的 i 节点。
 * 前半部分 i_forw/i_count/i_flag/i_ino 是内存管理字段；
 * 后半部分 di_* 与磁盘 inode 保持一致，用于保存文件元数据和数据块索引。
 */
struct inode{
    struct inode *i_forw;      /* hash 链表后继 */
    struct inode *i_back;      /* 兼容字段，当前删除时通过遍历找前驱 */
    char i_flag;               /* 内存 inode 脏标记，例如 IUPDATE */
    uint16_t i_ino;            /* inode 编号 */
    uint16_t i_count;          /* 内存引用计数 */
    uint16_t di_number;        /* 硬链接计数，降为 0 时释放文件 */
    uint16_t di_mode;          /* 文件类型位 + 低 9 位 rwx 权限 */
    uint16_t di_uid;           /* 文件所有者 uid */
    uint16_t di_gid;           /* 文件所属组 gid */
    uint32_t di_size;          /* 文件大小，单位字节 */
    uint16_t di_addr[NADDR];   /* 数据块索引：0~8 直接索引，9 一级间接索引 */
};

/* 磁盘上的 i 节点，必须精确 32 字节。
 * inode 区总 inode 数 = DINODEBLK * BLOCKSIZ / DINODESIZ = 512。
 */
struct dinode{
    uint16_t di_number;
    uint16_t di_mode;
    uint16_t di_uid;
    uint16_t di_gid;
    uint32_t di_size;
    uint16_t di_addr[NADDR];
};

/* 目录项，必须精确 16 字节。
 * 目录文件的内容就是 direct 数组，完成“文件名 -> inode 编号”的映射。
 */
struct direct{
    char d_name[DIRSIZ];
    uint16_t d_ino;
};

/* 超级块，存于物理 block 1。
 * s_free/s_pfree/s_nfree 管理数据块空闲空间，使用成组链接法；
 * s_inode/s_pinode/s_ninode 管理 inode 空闲空间，使用空闲 inode 缓存栈。
 */
struct filsys{
    uint16_t s_isize;          /* inode 区块数 */
    uint32_t s_fsize;          /* 数据区块数 */
    uint16_t s_nfree;          /* 空闲数据块总数 */
    uint16_t s_pfree;          /* s_free 当前栈顶指针 */
    uint16_t s_free[NICFREE];  /* 当前一组空闲数据块号 */
    uint16_t s_ninode;         /* 空闲 inode 总数 */
    uint16_t s_pinode;         /* s_inode 当前栈中数量 */
    uint16_t s_inode[NICINOD]; /* 当前缓存的一组空闲 inode 号 */
    uint16_t s_rinode;         /* 下次扫描空闲 inode 的起点 */
    char s_fmod;               /* 超级块脏标记 */
};

struct pwd{
    uint16_t p_uid;
    uint16_t p_gid;
    char password[PWDSIZ];
};

struct file{
    char f_flag;               /* FREAD/FWRITE/FAPPEND */
    uint16_t f_count;          /* 系统打开文件表引用计数 */
    struct inode *f_inode;     /* 指向已打开文件的内存 inode */
    uint32_t f_off;            /* 当前读写偏移 */
};

struct user{
    uint16_t u_active;         /* 用户槽是否被占用；必须独立于 uid，支持 root(uid=0) */
    uint16_t u_default_mode;   /* 新建文件/目录默认权限 */
    uint16_t u_uid;            /* 当前登录用户 uid */
    uint16_t u_gid;            /* 当前登录用户 gid */
    uint16_t u_ofile[NOFILE];  /* 用户打开文件表，保存系统打开文件表下标 */
};

#pragma pack(pop)

struct dir{
    struct direct direct[DIRNUM];
    int size;
};

struct hinode{
    struct inode *i_forw;
};

/* 全局变量 */
extern struct hinode hinode[NHINO];
extern struct dir dir;
extern struct file sys_ofile[SYSOPENFILE];
extern struct filsys filsys;
extern struct pwd pwd[PWDNUM];
extern struct user user[USERNUM];
extern FILE *fd;
extern struct inode *cur_path_inode;
extern int user_id;

/* 函数声明 */
extern struct inode *iget(uint16_t dinodeid);
extern void iput(struct inode *pinode);
extern uint16_t balloc(void);
extern void bfree(uint16_t block_num);
extern struct inode *ialloc(void);
extern void ifree(uint16_t dinodeid);
extern uint16_t namei(const char *name);
extern uint16_t iname(const char *name);
extern int valid_dir_name(const char *name);
extern void dir_entry_occupy(uint16_t index);
extern void dir_entry_remove(uint16_t index);
extern int access(uint16_t uid, struct inode *inode, uint16_t mode);
extern void _dir(void);
extern void mkdir(const char *dirname);
extern int chdir(const char *dirname);
extern uint16_t aopen(uint16_t uid, const char *filename, uint16_t openmode);
extern uint16_t creat(uint16_t uid, const char *filename, uint16_t mode);
extern uint16_t read(uint16_t fd, char *buf, uint16_t size);
extern uint16_t write(uint16_t fd, char *buf, uint16_t size);
extern int login(uint16_t uid, const char *passwd);
extern int logout(uint16_t uid);
extern void install(void);
extern void format(void);
extern void close(uint16_t uid, uint16_t cfd);
extern void halt(void);
extern void delete(const char *filename);
extern void rmdir(const char *dirname);
extern void cp(uint16_t user_id, const char *src, const char *dst);

/* 多级索引辅助函数 */
extern uint16_t get_block(struct inode *inode, uint16_t logical_block);
extern int set_block(struct inode *inode, uint16_t logical_block, uint16_t physical_block);
extern void free_all_blocks(struct inode *inode);

/* 新增拓展功能 */
extern void chmod(uint16_t uid, const char *filename, uint16_t mode);
extern void hlink(const char *src, const char *dst);
extern void slink(const char *target, const char *name);
extern int lseek_file(uint16_t uid, uint16_t cfd, uint32_t offset);
extern void mv(const char *src, const char *dst);
extern void file_stat(const char *filename);
extern void file_wc(const char *filename);
extern void file_find(const char *path, const char *pattern);
extern void chown_file(uint16_t uid, const char *filename, uint16_t new_uid);

#endif
