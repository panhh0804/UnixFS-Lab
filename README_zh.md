# UnixFS-Lab

[English](README.md) | [简体中文](README_zh.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C](https://img.shields.io/badge/Language-C-blue.svg)](https://gcc.gnu.org/)
[![Language: Python](https://img.shields.io/badge/Language-Python-green.svg)](https://www.python.org/)

`UnixFS-Lab` 是一个交互式的类 UNIX 模拟文件系统（设计灵感源自经典的 UNIX V6 文件系统）。项目采用混合架构：使用 **C 语言** 编写底层核心文件系统引擎并以本地守护进程（Daemon）形式运行，使用 **Python & PyQt6** 构建现代化、动态的可视化监控面板。前后端通过本地 **TCP Socket** 进行实时进程间通信（IPC）。

该项目非常适合作为操作系统（OS）学习的沙盒，帮助理解 superblock 同步、inode 映射、磁盘块分配、权限模型及目录遍历等底层概念。

---

## 📸 可视化界面功能

PyQt6 监控面板提供了对虚拟磁盘状态的实时观测：
- **磁盘块占用网格**：以 32×16 网格直观展示全部 512 个数据块的状态（系统保留、已占用、空闲）。
- **块链（Block Chaining）展示**：可视化呈现指定文件/Inode 占用的磁盘块链条。
- **活动 Inode 监控**：实时展示内存活动 Inode 和磁盘 Inode 的元数据状态。
- **目录树**：动态渲染文件目录的层级结构。
- **交互式终端**：内置模拟终端，支持输入类 UNIX 命令与文件系统直接交互。

---

## 🌟 核心特性

* **前后端分离架构**：C 语言编写的内核 Daemon 负责高效的块级操作与逻辑控制，Python PyQt6 前端通过 Socket 响应式渲染状态变化。
* **经典 UNIX 存储设计**：
  * **Inode 管理**：双层 Inode 设计（内存 `struct inode` 和精确 32 字节的磁盘 `struct dinode`）。
  * **混合索引机制**：9 个直接索引项 + 1 个一级间接索引项。单个文件最大支持 265 块（约 132 KB）。
  * **成组链接法（Grouped Free Block Linking）**：超级块通过缓存栈与成组链接法动态管理空闲数据块（每组 50 个块）。
* **多用户与权限安全**：
  * 模拟基于 `/etc/password` 文件的多用户登录会话管理。
  * 标准 UNIX 八进制权限模型（对 owner/group/others 的 `rwx` 权限进行判断）。
  * 管理员特权机制：`root` 用户（uid = 0）可绕过常规 rwx 权限检查。
* **丰富的文件系统操作**：
  * 支持硬链接（`ln`）与符号链接/软链接（`symlink`）。
  * 提供完善的系统工具：`find`、`wc`、`stat`、`df`、`lseek`、`chmod`、`chown` 及打开文件表管理。
* **状态持久化**：所有的修改均实时同步至单一的虚拟磁盘文件（`disk.img`），支持跨重启状态持久化。

---

## 📐 虚拟磁盘布局

虚拟磁盘模拟为二进制文件 `disk.img`，共包含 546 个磁盘块（每块大小 512 字节）：

| 块范围 | 组件名称 | 详细说明 |
|---|---|---|
| **第 0 块** | 系统保留块 | 用于系统保留/引导区空间 |
| **第 1 块** | 超级块 (Superblock) | 管理空闲数据块、空闲 inode 栈及文件系统全局元数据 |
| **第 2 - 33 块** | Inode 区 | 预分配的 Inode 表，容纳 512 个磁盘 inode（每个 32 字节） |
| **第 34 - 545 块**| 数据区 | 512 个物理数据块，用于存储目录项内容和文件实际数据 |

---

## 🛠️ 构建与快速启动

### 📋 前提条件

运行本项目需要您的系统安装有 C 编译器（`gcc`）、`make` 编译工具以及 Python 3。

### 1. 安装前端依赖

安装 GUI 界面所需的 `PyQt6`：

```bash
python3 -m pip install PyQt6
```

### 2. 编译后端 Daemon

在项目根目录下通过 `Makefile` 编译 C 引擎：

```bash
make
```

### 3. 运行可视化前端

执行一键启动脚本（该脚本会自动清理残留的后台进程并运行前端）：

```bash
./run.sh
```

或者手动启动前端面板：
```bash
python3 frontend/main.py
```

### 4. 命令行交互模式

如果您希望不启动 GUI，直接在终端中以命令行模式操作文件系统：

```bash
./fs
```
*注意：首次运行需要输入 `yes` 格式化虚拟盘以初始化文件系统结构。*

---

## 🖥️ 支持的终端指令

内置的命令行支持大部分标准的 UNIX 文件操作：

| 指令 | 详细说明 |
|---|---|
| `login <uid> <password>` | 用户登录，切换用户上下文 |
| `logout` | 注销当前用户会话 |
| `dir` | 列出当前目录下的文件及元数据 |
| `mkdir <dirname>` | 创建新目录 |
| `cd <dirname>` | 切换当前工作目录 |
| `pwd` | 显示当前工作路径 |
| `rmdir <dirname>` | 删除空目录 |
| `create <filename> [mode] [content]` | 创建文件，支持指定权限与初始内容 |
| `open <filename> <mode>` | 打开文件，模式支持 `read` / `write` / `append` |
| `close <fd>` | 关闭指定的文件描述符 |
| `read <fd> <size>` | 从文件描述符中读取指定大小的内容 |
| `write <fd> <text>` | 向已打开的文件描述符写入文本 |
| `cat <filename>` | 读取并显示文件的全部内容 |
| `delete <filename>` | 删除普通文件或软链接 |
| `cp <src> <dst>` | 复制文件 |
| `mv <src> <dst>` | 移动文件或重命名 |
| `ln <src> <dst>` | 创建硬链接，使新目录项指向已有 Inode |
| `symlink <target> <name>` | 创建软链接/符号链接，内容保存目标路径 |
| `chmod <filename> <mode>` | 修改文件权限（使用八进制，如 `0755`） |
| `chown <uid> <filename>` | 转移文件所有权 |
| `stat <filename>` | 查看文件的 Inode 元数据及磁盘块链 |
| `wc <filename>` | 统计文件的行数、词数和字节数 |
| `find <path> [pattern]` | 在指定路径下递归检索匹配的文件名 |
| `lseek <fd> <offset>` | 调整文件描述符的读写偏移量 |
| `df` | 查看磁盘块和 Inode 的空间使用量 |
| `format yes` | 格式化虚拟磁盘，重置文件系统结构 |
| `halt` / `exit` | 安全保存虚拟盘状态并退出系统 |

---

## 🧪 自动化测试

`UnixFS-Lab` 提供了一个全场景的集成测试脚本 `test.sh`：

```bash
./test.sh
```

该脚本将自动执行以下测试流程：
1. 编译并清理后端。
2. 格式化并建立全新的虚拟磁盘镜像。
3. 自动模拟一系列目录与文件操作。
4. 验证各项边界情况（如最大打开文件数限制、越权操作拦截、一级间接索引大文件写入）。
5. 重启系统以验证数据持久性。

---

## 📄 开源许可证

本项目基于 MIT 许可证开源。详情请参阅 [LICENSE](LICENSE) 文件。
