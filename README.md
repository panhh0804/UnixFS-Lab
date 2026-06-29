# UnixFS-Lab (OS 文件系统课程设计 v5.0)

这是一个类 UNIX 的模拟文件系统课程设计项目。后端使用 C 语言实现文件系统核心逻辑，前端使用 PyQt6 实现可视化监控界面。项目支持文件和目录管理、inode 管理、数据块分配、权限控制、root 用户、硬链接/软链接、大文件一级间接索引、自动测试和逐条测试。

项目路径：

```text
UnixFS-Lab
```

---

## 功能概览

| 类型 | 功能 |
|---|---|
| 基础文件系统 | 格式化、安装、保存退出、超级块管理 |
| 文件操作 | 创建、打开、关闭、读、写、追加写、删除、复制、移动/重命名 |
| 目录操作 | 创建目录、删除空目录、切换目录、显示当前路径、目录树 |
| inode 管理 | inode 分配、释放、内存 hash 缓存、引用计数、链接数 |
| 数据块管理 | 成组链接法管理空闲数据块 |
| 文件索引 | 9 个直接索引 + 1 个一级间接索引 |
| 权限系统 | `uid/gid/mode`，owner/group/others 权限判断 |
| root 用户 | `uid=0`，可绕过普通权限检查 |
| 链接 | 硬链接 `ln`，软链接 `symlink` |
| 工具命令 | `stat`、`wc`、`find`、`df`、`chmod`、`chown`、`lseek` |
| 前端可视化 | 数据块占用图、inode 列表、当前目录、目录树、系统统计、命令终端 |
| 测试 | 前端自动测试、逐条测试、命令行综合测试脚本 |

---

## 项目结构

```text
.
├── Makefile                     # C 后端编译脚本
├── filesys.h                    # 核心常量、结构体、函数声明
├── main.c                       # 命令解析、daemon、前端内部查询命令
├── format.c                     # 格式化，初始化根目录、/etc、/etc/password
├── install.c                    # 加载 disk.img，读入超级块、密码表、根目录
├── halt.c                       # 保存并退出
├── superblock.c / superblock.h  # 超级块读写与初始化
├── igetput.c                    # 内存 inode 获取、写回、释放
├── iallfre.c                    # inode 分配与回收
├── ballfre.c                    # 数据块分配与回收，成组链接法
├── rdwt.c                       # 文件读写，直接索引 + 一级间接索引
├── creat.c / open.c / close.c   # 文件创建、打开、关闭
├── delete.c                     # 删除普通文件/链接
├── dir.c / name.c               # 目录操作、目录项查找
├── access.c                     # 权限检查，包含 root 特权判断
├── chmod.c / chown.c            # 修改权限和所有者
├── link.c                       # 硬链接、软链接
├── mv.c                         # 重命名/移动
├── stat.c / wc.c / find.c       # 工具命令
├── lseek.c                      # 文件偏移定位
├── log.c                        # 用户登录、注销
├── run.sh                       # 一键启动前端
├── test.sh                      # 命令行综合测试脚本
├── disk.img                     # 虚拟磁盘镜像，运行时生成/更新
├── frontend/
│   ├── main.py                  # PyQt6 主界面
│   ├── fs_client.py             # 前端与后端 daemon 通信
│   ├── requirements.txt         # 前端依赖
│   ├── widgets/
│   │   └── disk_grid.py         # 数据块占用状态网格
│   └── assets/
│       └── style.qss            # 前端样式
└── README.md
```

---

## 编译与运行

### 1. 安装前端依赖

```bash
cd UnixFS-Lab
python3 -m pip install -r frontend/requirements.txt
```

如果只缺 PyQt6，也可以直接安装：

```bash
python3 -m pip install PyQt6
```

### 2. 编译后端

```bash
cd UnixFS-Lab
make
```

编译成功后会生成：

```text
./fs
```

### 3. 启动可视化前端

推荐使用：

```bash
cd UnixFS-Lab
./run.sh
```

`run.sh` 会做这些事：

```text
1. 清理可能残留的后端 daemon
2. 如果 fs 不存在则自动 make
3. 确保 fs 有执行权限
4. 启动 PyQt6 前端
```

也可以手动启动：

```bash
cd UnixFS-Lab
make
cd frontend
python3 main.py
```

### 4. 命令行模式

不启动前端时，也可以直接运行后端：

```bash
cd UnixFS-Lab
make
./fs
```

首次使用或需要重置文件系统时，输入：

```text
yes
```

表示格式化磁盘。

---

## 虚拟磁盘布局

本项目使用文件 `disk.img` 模拟磁盘。块大小固定为：

```text
512 字节
```

代码中的关键定义：

```c
#define BLOCKSIZ    512
#define DINODEBLK   32
#define FILEBLK     512
#define DINODESIZ   32
#define DINODESTART 2*BLOCKSIZ
#define DATASTART   (2+DINODEBLK)*BLOCKSIZ
```

物理布局：

```text
物理块 0        保留块
物理块 1        超级块 superblock
物理块 2~33     inode 区，共 32 个块
物理块 34 以后  数据区，共 512 个数据块
```

inode 总数：

```text
DINODEBLK * BLOCKSIZ / DINODESIZ
= 32 * 512 / 32
= 512 个 inode
```

格式化后，前 4 个 inode 预分配：

```text
inode 0  保留空 inode
inode 1  根目录 /
inode 2  /etc 目录
inode 3  /etc/password 文件
```

因此干净文件系统中通常显示：

```text
Total inodes: 512
Used: 4
Free: 508
```

---

## 前端磁盘块可视化说明

前端的“磁盘块占用状态”显示的是：

```text
数据区的 512 个数据块
```

不是整个 `disk.img` 的所有物理块。因此它不会显示超级块和 inode 区。inode 区通过右侧的“Inode 列表”展示。

数据块网格中的前三块是数据区相对块号：

```text
数据块 0：根目录 / 的目录数据块
数据块 1：/etc 目录的数据块
数据块 2：/etc/password 用户口令表
```

注意：

```text
前端 Block 0 不是 disk.img 的物理块 0。
前端 Block 0 实际对应物理块 34。
```

换算关系：

```text
物理块号 = 34 + 数据区块号
```

---

## inode 与数据块关系

文件系统采用 inode 管理文件。目录项只保存：

```text
文件名 -> inode 编号
```

inode 保存：

```text
文件类型
权限 mode
所有者 uid
所属组 gid
文件大小
硬链接计数
数据块索引 di_addr[]
```

核心结构：

```c
struct dinode {
    uint16_t di_number;
    uint16_t di_mode;
    uint16_t di_uid;
    uint16_t di_gid;
    uint32_t di_size;
    uint16_t di_addr[NADDR];
};
```

文件内容不放在 inode 里，而是放在数据块区。inode 通过 `di_addr[]` 指向这些数据块。

关系可以理解为：

```text
目录项 doc1
    ↓
inode 51
    ↓
di_addr[0] = 508
    ↓
数据块 508
    ↓
文件内容 HelloWorld
```

---

## 文件索引方式

本项目使用混合索引分配：

```text
di_addr[0..8]  直接索引
di_addr[9]     一级间接索引
```

也就是：

```text
前 9 个数据块由 inode 直接保存块号；
超过 9 个数据块后，di_addr[9] 指向一个间接索引块；
间接索引块内部继续保存数据块号。
```

每个间接索引块大小为 512 字节，每个块号是 `uint16_t`，所以一个间接索引块最多保存：

```text
512 / 2 = 256 个数据块号
```

单个文件最大可索引数据块数：

```text
9 + 256 = 265 块
```

单个文件最大容量：

```text
265 * 512 = 135680 字节
```

---

## 空闲数据块管理：成组链接法

空闲数据块由超级块管理，采用成组链接法。

相关字段：

```c
uint16_t s_nfree;
uint16_t s_pfree;
uint16_t s_free[NICFREE];
```

含义：

| 字段 | 含义 |
|---|---|
| `s_nfree` | 空闲数据块总数 |
| `s_pfree` | 当前空闲块栈指针 |
| `s_free[50]` | 超级块中缓存的一组空闲块号 |

`NICFREE = 50`，表示超级块一次缓存 50 个空闲块号。

成组链接法的核心思想：

```text
超级块只保存当前一组空闲块号；
下一组空闲块号保存在磁盘上的某个“组长块”里；
当前组用完时，再读取组长块加载下一组。
```

分配时：

```text
balloc()
1. 从超级块 s_free[] 中取一个空闲块号
2. 如果当前组没用完，直接返回
3. 如果当前组用完，把取出的组长块读入内存
4. 从组长块中加载下一组空闲块号
```

回收时：

```text
bfree()
1. 如果超级块当前组没满，直接把块号压回 s_free[]
2. 如果当前组满了，把当前组写入被回收的块
3. 被回收的块成为新的组长块
```

---

## 用户、权限与 root

格式化时初始化用户表 `/etc/password`：

| uid | gid | password | 说明 |
|---:|---:|---|---|
| 0 | 0 | `root` | root 特权用户 |
| 2116 | 3 | `dddd` | 普通用户 |
| 2117 | 3 | `bbbb` | 普通用户 |
| 2118 | 4 | `abcd` | 普通用户，默认演示用户 |
| 2119 | 4 | `cccc` | 普通用户 |
| 2220 | 5 | `eeee` | 普通用户 |

登录示例：

```text
login 2118 abcd
login 0 root
```

权限使用 UNIX 风格的八进制表示：

```text
0777
0644
0755
0000
```

后三位分别表示：

```text
owner
group
others
```

每一位由以下值相加：

```text
4 = read
2 = write
1 = execute
```

例如：

```text
0777 = rwxrwxrwx
0644 = rw-r--r--
0755 = rwxr-xr-x
0000 = ---------
```

权限判断顺序：

```text
1. root: uid == 0，直接通过权限检查
2. owner: 当前用户 uid == 文件 di_uid
3. group: 当前用户 gid == 文件 di_gid
4. others: 其他用户权限
```

root 用户说明：

```text
root 可以绕过普通 rwx 权限检查。
但 root 不能绕过结构性错误，例如文件不存在、目录非空、磁盘满等。
```

root 演示命令：

```text
format yes
login 2118 abcd
create rootdemo RootOnlyData
close 0
chmod rootdemo 0000
open rootdemo read
logout
login 0 root
open rootdemo read
read 0 30
close 0
```

预期现象：

```text
普通用户被 0000 权限拒绝；
root 用户可以成功读取 RootOnlyData。
```

---

## 支持命令

| 命令 | 说明 |
|---|---|
| `login <uid> <password>` | 用户登录 |
| `logout` | 注销当前用户 |
| `dir` | 列出当前目录 |
| `mkdir <dirname>` | 创建目录 |
| `cd <dirname>` | 切换目录 |
| `pwd` | 显示当前路径 |
| `rmdir <dirname>` | 删除空目录 |
| `create <filename> [mode] [content]` | 创建文件，可指定权限和初始内容 |
| `open <filename> [mode]` | 打开文件，mode 支持 `read/write/append` |
| `close <fd>` | 关闭文件描述符 |
| `read <fd> <size>` | 从文件描述符读取数据 |
| `write <fd> <text>` | 写入文本 |
| `cat <filename>` | 显示文件内容 |
| `delete <filename>` | 删除普通文件或软链接 |
| `cp <src> <dst>` | 复制文件 |
| `mv <src> <dst>` | 重命名或移动 |
| `ln <src> <dst>` | 创建硬链接 |
| `symlink <target> <name>` | 创建软链接 |
| `chmod <filename> <mode>` | 修改权限 |
| `chown <uid> <filename>` | 修改文件所有者 |
| `stat <filename>` | 显示 inode 元数据 |
| `wc <filename>` | 统计行数、词数、字节数 |
| `find <path> [pattern]` | 递归查找文件 |
| `lseek <fd> <offset>` | 修改文件偏移 |
| `df` | 显示磁盘块和 inode 使用情况 |
| `format yes` | 格式化并重新安装文件系统 |
| `help` | 显示命令帮助 |
| `exit` / `halt` | 保存并退出 |

---

## 前端界面说明

前端使用 PyQt6，启动后会自动启动 C 后端 daemon，并通过 TCP socket 通信。

界面主要区域：

| 区域 | 说明 |
|---|---|
| 数据块占用状态 | 32×16 网格，显示 512 个数据区块的占用情况 |
| 目录树 | 显示当前文件系统目录层次 |
| 当前目录 | 显示当前目录下的目录项，支持右键菜单和双击 |
| Inode 列表 | 显示已使用 inode 的元数据 |
| 系统统计 | 显示用户、路径、块数、inode 数 |
| 命令终端 | 输入命令、查看输出、执行自动测试/逐条测试 |

颜色说明：

| 颜色 | 含义 |
|---|---|
| 橙色 | 系统保留数据块 |
| 绿色 | 已占用数据块 |
| 深色 | 空闲数据块 |

交互：

```text
点击数据块       显示该块属于哪个 inode/文件
悬浮数据块       显示块号和具体作用
右键当前目录文件 查看块链
双击目录         进入目录
双击文件         打开编辑器
```

---

## 测试方式

### 1. 前端逐条测试

启动前端后点击：

```text
逐条测试
```

每点击一次执行一条测试命令，适合观察每条命令对 inode、数据块、目录树的影响。

### 2. 前端自动测试

启动前端后点击：

```text
自动测试
```

前端会按顺序执行完整测试流程。

### 3. 命令行综合测试

```bash
cd UnixFS-Lab
./test.sh
```

该脚本会：

```text
1. make clean && make
2. 删除旧 disk.img
3. 初始化虚拟磁盘
4. 生成 test_input.txt
5. 执行完整命令测试
6. 验证重启持久化
7. 验证格式化
```

---

## 典型验收流程

建议演示顺序：

```text
format yes
login 2118 abcd
mkdir workdir
cd workdir
create doc1
write 0 HelloWorld
close 0
stat doc1
dir
chmod doc1 0000
open doc1 read
logout
login 0 root
open doc1 read
read 0 20
close 0
logout
login 2118 abcd
create bigfile
连续写入 12 个 512 字节块
stat bigfile
lseek + read 验证一级间接索引
```

可以重点说明：

```text
1. inode 保存文件元数据和数据块索引
2. 数据块通过成组链接法管理空闲空间
3. 文件使用 9 个直接索引 + 1 个一级间接索引
4. 权限采用 owner/group/others 模型
5. root 用户 uid=0，可以绕过普通权限检查
6. 前端数据块网格显示的是数据区，不是完整物理磁盘布局
```

---

## 常见问题

### 1. 为什么磁盘块可视化看不到 inode 区？

因为前端网格显示的是数据区 512 个数据块，不是整个 `disk.img` 的绝对物理块。inode 区位于物理块 2~33，通过右侧“Inode 列表”展示。

### 2. 为什么 root 登录失败？

root 用户是格式化时写入 `/etc/password` 的。如果你使用的是旧 `disk.img`，需要先执行：

```text
format yes
```

然后再登录：

```text
login 0 root
```

### 3. 为什么 `df` 里 inode 总数是 512？

因为 inode 区有 32 块，每块 512 字节，每个 inode 32 字节：

```text
32 * 512 / 32 = 512
```

### 4. 为什么 `doc2` 大小为 0 时没有 block chain？

空文件没有内容，因此不需要分配数据块。只有写入内容后才会分配数据块。

### 5. 硬链接和软链接区别是什么？

硬链接：

```text
多个目录项指向同一个 inode，链接数 Links 增加。
```

软链接：

```text
创建一个新的链接文件，文件内容保存目标路径。
```

---

## 构建检查

后端编译：

```bash
make
```

前端语法检查：

```bash
python3 -m compileall frontend
```

---

## 课程设计答辩关键词

```text
superblock
inode
dinode
directory entry
direct block
single indirect block
grouped free block linking
uid/gid/mode
root privilege
hard link
symbolic link
file descriptor
open file table
disk block visualization
```
