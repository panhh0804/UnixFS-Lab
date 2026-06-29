# Windows 运行配置指南

本文档记录了在 Windows 系统上成功运行文件系统可视化项目的完整过程和解决方案。

---

## 遇到的问题

### 问题 1：WinError 216 - 版本不兼容
**症状：** 启动前端时报错：`无法启动 fs daemon: [WinError 216] 该版本的 %1 与你运行的 Windows 版本不兼容`

**原因：** 已编译的 `fs` 可执行文件与 Windows 版本不兼容，可能是因为：
- 编译环境的差异
- 链接库不匹配

**解决方案：** 在 Windows 上重新编译项目

---

### 问题 2：符号冲突 - 函数名重定义
**症状：** 编译时出现多个 "conflicting types" 错误：
```
error: conflicting types for 'access'
error: conflicting types for 'mkdir'
error: conflicting types for 'creat'
error: conflicting types for 'read'
error: conflicting types for 'write'
error: conflicting types for 'close'
error: conflicting types for 'rmdir'
error: conflicting types for 'chmod'
```

**原因：** Windows 的 `<io.h>` 头文件已经定义了这些函数，与项目的 `filesys.h` 中的定义冲突

**解决方案：** 修改 `main.c` 的头文件引入顺序
1. 先引入项目的 `filesys.h`
2. 后引入 Windows 的系统头文件 `<winsock2.h>` 等
3. 使用 `#undef` 指令清除冲突的符号

---

## 完整解决步骤

### 1. 确认编译工具
```powershell
gcc --version  # 验证 MinGW 的 GCC 已安装
```

**要求：** GCC 6.3.0 或更高版本

---

### 2. 修改 main.c

将文件顶部的头文件包含部分从：

```c
/* main.c: 文件系统主程序 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#ifndef _WIN32
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#endif
#endif
#include "filesys.h"
```

修改为：

```c
/* main.c: 文件系统主程序 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
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
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#endif
#endif
```

**关键改动：**
- `filesys.h` 提前到 Windows 系统头文件之前
- 在 `#ifdef _WIN32` 块中添加 `#undef` 指令，清除与 `<io.h>` 的冲突

---

### 3. 清理旧的编译产物
```powershell
cd f:\course\3-2\OS_project\filesystem
Remove-Item -Path "fs", "fs.exe", "*.o" -ErrorAction SilentlyContinue
```

---

### 4. 重新编译
```powershell
gcc -Wall -g -std=c99 -o fs.exe main.c igetput.c iallfre.c ballfre.c rdwt.c creat.c open.c close.c delete.c dir.c name.c access.c log.c install.c format.c halt.c chmod.c link.c mv.c stat.c wc.c find.c chown.c lseek.c superblock.c -lws2_32
```

**编译选项说明：**
- `-Wall` - 显示所有警告
- `-g` - 包含调试符号
- `-std=c99` - 使用 C99 标准
- `-o fs.exe` - 输出文件名
- `-lws2_32` - 链接 Windows Socket 库

**预期输出：** 4 个警告（可忽略），0 个错误

---

### 5. 安装 Python 依赖
```powershell
python -m pip install -r frontend\requirements.txt
```

**需要的包：**
- PyQt6
- PyQt6-sip

---

### 6. 启动前端
```powershell
python frontend/main.py
```

**启动流程：**
1. Python 启动 PyQt6 应用
2. 前端自动启动后端 daemon：`fs.exe --daemon --tcp --host 127.0.0.1 --port 9090`
3. PyQt6 窗口出现，显示文件系统可视化界面
4. 关闭窗口时自动关闭后端进程

---

## 最终验证

✅ `fs.exe` 可执行文件已生成
✅ 编译无错误
✅ PyQt6 前端窗口正常显示
✅ 后端 daemon 正常启动
✅ TCP 连接 127.0.0.1:9090 成功

---

## 故障排除

| 问题 | 解决方案 |
|------|---------|
| `系统找不到指定的文件` | 检查 `fs.exe` 是否存在，重新编译 |
| `WinError 216 版本不兼容` | 修改 `main.c` 头文件顺序，重新编译 |
| `conflicting types` 编译错误 | 确认 `#undef` 指令已添加到 `main.c` |
| PyQt6 找不到后端 | 检查当前目录是否在项目根目录 |

---

## 快速参考

### 一键编译脚本
```powershell
cd f:\course\3-2\OS_project\filesystem
Remove-Item -Path "fs", "fs.exe", "*.o" -ErrorAction SilentlyContinue
gcc -Wall -g -std=c99 -o fs.exe main.c igetput.c iallfre.c ballfre.c rdwt.c creat.c open.c close.c delete.c dir.c name.c access.c log.c install.c format.c halt.c chmod.c link.c mv.c stat.c wc.c find.c chown.c lseek.c superblock.c -lws2_32
```

### 启动应用
```powershell
python frontend/main.py
```

---

## 系统环境信息

- **操作系统：** Windows
- **编译器：** MinGW GCC 6.3.0
- **Python 版本：** 3.10.8
- **GUI 框架：** PyQt6
- **编译日期：** 2026年6月2日

