#!/bin/bash
# 文件系统全场景综合测试脚本（含边界用例）
set -e

FS_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$FS_DIR"

echo "========================================"
echo "  文件系统课设 - 全场景测试脚本"
echo "========================================"

echo ""
echo "[1/5] 编译项目..."
make clean >/dev/null 2>&1
make

# 删除旧磁盘，确保从干净状态开始
rm -f disk.img disk.img.bak

echo ""
echo "[2/5] 初始化虚拟盘（格式化）..."
printf 'yes\nhalt\n' | ./fs >/dev/null 2>&1

echo ""
echo "[3/5] 生成测试输入..."

python3 -c '
import sys

lines = []

# 第一行回答格式化询问
lines += ["no"]

# ============ 第一回合：全面功能测试 ============
lines += ["login 2118 abcd"]

# 目录操作基础
lines += [
    "mkdir workdir",
    "cd workdir",
    "pwd",
    "mkdir subdir",
    "cd subdir",
    "pwd",
    "cd ..",
    "pwd",
    "cd ..",
    "pwd",
]

# cd 失败：路径不应被错误更新
lines += [
    "cd nonexist",
    "pwd",
]

# 文件创建与基础读写 + cat/wc/stat
lines += [
    "create doc1",
    "write 0 HelloWorld",
    "close 0",
    "cat doc1",
    "wc doc1",
    "stat doc1",
    "open doc1 read",
    "read 0 20",
    "close 0",
]

# create 带权限参数
lines += [
    "create doc2 0644",
    "dir",
    "close 0",
]

# create 覆盖已有文件
lines += [
    "create doc1",
    "write 0 Overwritten",
    "close 0",
    "open doc1 read",
    "read 0 20",
    "close 0",
]

# create 禁止覆盖目录
lines += [
    "create workdir",
]

# append 模式追加写入
lines += [
    "create appendfile",
    "write 0 First",
    "close 0",
    "open appendfile append",
    "write 0 Second",
    "close 0",
    "open appendfile read",
    "read 0 30",
    "close 0",
]

# 禁止打开目录
lines += [
    "open workdir read",
]

# cp 文件复制
lines += [
    "cp doc1 doc1copy",
    "open doc1copy read",
    "read 0 20",
    "close 0",
]

# 禁止复制目录
lines += [
    "cp workdir copybad",
]

# mv 移动/重命名 + lseek 定位
lines += [
    "mv doc1copy doc1moved",
    "stat doc1moved",
    "cat doc1moved",
    "open doc1moved read",
    "lseek 0 5",
    "read 0 10",
    "close 0",
]

# chmod 权限修改与验证
lines += [
    "chmod doc2 0755",
    "dir",
    "chmod doc2 0644",
    "dir",
    "chmod doc2 0000",
    "dir",
]

# ============ 权限边界测试（新增） ============
lines += [
    "create permtest PermissionBoundary",
    "close 0",
    # 1. chmod 0000 后 owner 无法打开
    "chmod permtest 0000",
    "open permtest read",
    # 2. root 用户可以绕过 0000 权限读取
    "logout",
    "login 0 root",
    "open permtest read",
    "read 0 30",
    "close 0",
    "logout",
    "login 2118 abcd",
    # 3. chmod 0050 后 owner 仍无法打开（group 权限不 fallback）
    "chmod permtest 0050",
    "open permtest read",
    "logout",
    # 4. 同 group 用户 (2119, gid=04) 可以打开
    "login 2119 cccc",
    "open permtest read",
    "read 0 30",
    "close 0",
    "logout",
    # 5. other 用户 (2117, gid=03) 无法打开
    "login 2117 bbbb",
    "open permtest read",
    "logout",
    # 6. chmod 0004 后 other 可以打开
    "login 2118 abcd",
    "chmod permtest 0004",
    "logout",
    "login 2117 bbbb",
    "open permtest read",
    "read 0 30",
    "close 0",
    "logout",
    # 恢复并清理
    "login 2118 abcd",
    "chmod permtest 0777",
    "delete permtest",
]

# ============ 目录 execute 权限测试（新增） ============
lines += [
    "mkdir testdir",
    "chmod testdir 0000",
    "cd testdir",
    "chmod testdir 0777",
    "rmdir testdir",
]

# df 磁盘使用
lines += [
    "df",
]

# chown 修改所有者
lines += [
    "create chowntest ChownTestData",
    "close 0",
    "stat chowntest",
    "chown 99 chowntest",
    "stat chowntest",
    "logout",
    "login 0 root",
    "chown 2118 chowntest",
    "stat chowntest",
    "logout",
    "login 2118 abcd",
]

# ============ 多级索引：大文件 >5KB（增强） ============
lines += [
    "create bigfile",
]
for i in range(12):
    letter = chr(ord("A") + i)
    lines += [f"write 0 {letter * 512}"]
lines += [
    "close 0",
    "dir",
    "open bigfile read",
    # 读取前两块（直接索引）
    "read 0 20",
    "read 0 20",
    # 跳到第 10 块（间接索引第 1 项，对应字母 K）
    "lseek 0 4608",
    "read 0 20",
    "close 0",
]

# 硬链接与软链接
lines += [
    "create linksrc LinkTestData",
    "close 0",
    "ln linksrc hlink",
    "symlink linksrc slink",
    "dir",
    "open hlink read",
    "read 0 30",
    "close 0",
    "open slink read",
    "read 0 30",
    "close 0",
    "delete linksrc",
    "dir",
    "open hlink read",
    "read 0 30",
    "close 0",
    "open slink read",
]

# find 查找文件
lines += [
    "find / doc1moved",
    "find / nonexist",
]

# ============ 打开文件表满测试（新增） ============
lines += [
    "create t0", "create t1", "create t2", "create t3", "create t4",
    "create t5", "create t6", "create t7", "create t8", "create t9",
    "create t10", "create t11", "create t12", "create t13", "create t14",
    "create t15", "create t16", "create t17", "create t18", "create t19",
    # 第 21 个应该失败
    "create t20",
]
# 关闭前 20 个
for i in range(20):
    lines += [f"close {i}"]
# 清理
for i in range(20):
    lines += [f"delete t{i}"]

# ============ rmdir 非空目录拒绝（新增） ============
lines += [
    "mkdir nonempty",
    "cd nonempty",
    "create dummy",
    "close 0",
    "cd ..",
    "rmdir nonempty",
    # 清理
    "cd nonempty",
    "delete dummy",
    "cd ..",
    "rmdir nonempty",
]

# 删除与清理（先恢复 doc2 权限，否则 owner 也无法删除）
lines += [
    "chmod doc2 0777",
    "delete doc1",
    "delete doc1moved",
    "delete doc2",
    "delete appendfile",
    "delete bigfile",
    "delete hlink",
    "delete slink",
    "delete chowntest",
    "cd workdir",
    "rmdir subdir",
    "cd ..",
    "rmdir workdir",
]

# 多用户切换
lines += [
    "logout",
    "login 2117 bbbb",
    "mkdir user2117dir",
    "cd user2117dir",
    "pwd",
    "create file2117 User2117Data",
    "close 0",
    "logout",
]

# 持久化：halt 保存
lines += [
    "halt",
]

sys.stdout.write("\n".join(lines) + "\n")
' > test_input.txt

echo ""
echo "[4/5] 执行全场景测试..."
echo ""
./fs < test_input.txt

echo ""
echo "[5/5] 验证重启持久化与格式化..."
echo ""
printf 'no\nlogin 2117 bbbb\ncd user2117dir\npwd\nopen file2117 read\nread 0 30\nclose 0\nlogout\nhalt\n' | ./fs
printf 'no\nformat\nyes\nlogin 2118 abcd\ndir\nlogout\nhalt\n' | ./fs

echo ""
echo "========================================"
echo "  全场景测试执行完毕"
echo "========================================"
