#!/bin/bash
# 启动文件系统可视化前端
# 先清理可能残留的后端 daemon 进程
pkill -f './fs --daemon'
sleep 0.5

# 进入项目根目录
cd "$(dirname "$0")" || exit 1

# 如果 fs 不存在，先编译
if [ ! -f "./fs" ]; then
    echo "编译后端..."
    make || exit 1
fi

# 确保 fs 有执行权限
chmod +x ./fs

# 启动前端
cd frontend && python3 main.py
