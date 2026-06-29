# 文件系统可视化前端

前端通过 TCP 连接后端，**端口动态分配**（启动时自动寻找本地空闲端口）。

## 依赖

```bash
python3 -m pip install -r requirements.txt
```

## 启动

```bash
# 方式一：使用项目根目录的启动脚本（推荐，会自动清理残留 daemon）
cd UnixFS-Lab
./run.sh

# 方式二：手动启动
cd UnixFS-Lab/frontend
python3 main.py
```

前端启动后会自动运行 `./fs --daemon --tcp --host 127.0.0.1 --port <随机空闲端口>`，关闭窗口时自动结束后端进程。

## Windows

Windows 原生运行时，先安装 Python、PyQt6 和 MinGW/MSYS2 的 gcc/make，然后在 PowerShell 中：

```powershell
cd path\to\UnixFS-Lab
make
python -m pip install -r frontend\requirements.txt
python frontend\main.py
```
