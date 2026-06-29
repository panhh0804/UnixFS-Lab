# UnixFS-Lab

[English](README.md) | [简体中文](README_zh.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C](https://img.shields.io/badge/Language-C-blue.svg)](https://gcc.gnu.org/)
[![Language: Python](https://img.shields.io/badge/Language-Python-green.svg)](https://www.python.org/)

`UnixFS-Lab` is an interactive simulator of a UNIX-like file system (modeled after classic UNIX V6). The project features a hybrid architecture: a core filesystem engine written in **C** running as a local daemon, and a modern GUI visualization dashboard built with **Python & PyQt6**. The two modules communicate in real-time via local **TCP Socket IPC**.

This project serves as an educational sandbox for exploring low-level Operating System concepts including superblock synchronization, inode mapping, disk block allocation, permissions, and directory traversal.

---

## 📸 Visualization Preview

The PyQt6 dashboard provides real-time visibility into the virtual disk:
- **Disk Block Grid**: A 32×16 grid visualizing the status of all 512 data blocks (reserved, allocated, or free).
- **Block Chaining**: Visual paths displaying which blocks belong to which files/inodes.
- **Active Inodes**: Real-time inspection of active memory and disk inodes.
- **Directory Tree**: Dynamic hierarchy of the file directory tree.
- **Interactive Terminal**: An integrated command shell supporting all UNIX-like filesystem commands.

---

## 🌟 Key Features

* **Client-Daemon Architecture**: Core filesystem operations are handled by the C daemon, ensuring high-performance block-level operations, while the PyQt6 frontend renders state changes reactively.
* **Classic UNIX Storage Design**:
  * **Inode Management**: Dual-layer inode system (memory `struct inode` and compact 32-byte disk `struct dinode`).
  * **Hybrid Block Indexing**: 9 direct block pointers + 1 single indirect block pointer. Supports files up to 265 blocks (~132 KB).
  * **Grouped Free Block Linking (成组链接法)**: Superblock manages disk space dynamically via groups (50 blocks per group) with leader-block caching.
* **Security & Permissions**:
  * Multi-user session management mapped to a simulated `/etc/password` registry.
  * Standard UNIX octal file permissions (`rwx` flags for owner/group/others).
  * Privilege escalation capabilities for administrative bypass (`root` user with uid = 0).
* **Advanced File Operations**:
  * Hard links (`ln`) and symbolic/soft links (`symlink`).
  * Robust system tools: `find`, `wc`, `stat`, `df`, `lseek`, `chmod`, `chown`, and file descriptors tracker.
* **Persistence & Reboot Integrity**: Synchronizes metadata to a single virtual disk file (`disk.img`), maintaining full state integrity across reboots.

---

## 📐 Virtual Disk Layout

The virtual disk is modeled as a binary file `disk.img` divided into 546 blocks (512-byte block size):

| Block Range | Component | Description |
|---|---|---|
| **Block 0** | Reserved Block | System reserve / boot space |
| **Block 1** | Superblock | Manages free disk blocks, free inodes, and filesystem metadata |
| **Block 2 - 33** | Inode Table | Pre-allocated space for 512 inodes (32 bytes each) |
| **Block 34 - 545** | Data Region | 512 physical data blocks for directory lists and file data |

---

## 🛠️ Build & Quick Start

### 📋 Prerequisites

Ensure you have a C compiler (`gcc`), `make`, and Python 3 installed.

### 1. Install Frontend Dependencies

Install `PyQt6` for the GUI interface:

```bash
python3 -m pip install PyQt6
```

### 2. Compile Backend Daemon

Compile the C engine using the provided `Makefile`:

```bash
make
```

### 3. Launch GUI Visualization Dashboard

Run the helper script to clean active sessions and launch the interface:

```bash
./run.sh
```

Alternatively, launch the dashboard manually:
```bash
python3 frontend/main.py
```

### 4. Interactive CLI Mode

If you prefer to interact with the file system directly via the command line:

```bash
./fs
```
*Note: Upon first startup, type `yes` to format the disk and set up the default structure.*

---

## 🖥️ Supported Commands

The interactive terminal supports a wide array of core UNIX operations:

| Command | Description |
|---|---|
| `login <uid> <password>` | Authenticate and switch user context |
| `logout` | Log out the current user session |
| `dir` | List files and metadata in the current directory |
| `mkdir <dirname>` | Create a directory |
| `cd <dirname>` | Change directory |
| `pwd` | Print current directory path |
| `rmdir <dirname>` | Remove an empty directory |
| `create <filename> [mode] [content]` | Create a new file with initial content and permissions |
| `open <filename> <mode>` | Open a file descriptor (`read`, `write`, or `append` mode) |
| `close <fd>` | Close an active file descriptor |
| `read <fd> <size>` | Read specified bytes from a file descriptor |
| `write <fd> <text>` | Write text data to an open file descriptor |
| `cat <filename>` | Read and display the contents of a file |
| `delete <filename>` | Delete a file or symbolic link |
| `cp <src> <dst>` | Copy a file |
| `mv <src> <dst>` | Move/rename a file or directory |
| `ln <src> <dst>` | Create a hard link pointing to the target inode |
| `symlink <target> <name>` | Create a soft link containing the target path |
| `chmod <filename> <mode>` | Modify octal permission mode (e.g., `0755`) |
| `chown <uid> <filename>` | Transfer file ownership |
| `stat <filename>` | Inspect metadata and block chain of an inode |
| `wc <filename>` | Output line, word, and byte counts of a file |
| `find <path> [pattern]` | Recursively search for matching filenames |
| `lseek <fd> <offset>` | Reposition file descriptor read/write offset |
| `df` | View free space and inode usage |
| `format yes` | Format virtual disk and reset structure |
| `halt` / `exit` | Save disk state and exit safely |

---

## 🧪 Testing Suite

`UnixFS-Lab` includes a comprehensive automated shell script, `test.sh`, which performs end-to-end functionality checks:

```bash
./test.sh
```

The test runner automatically:
1. Compiles the filesystem binary.
2. Formats a fresh virtual disk.
3. Simulates basic directory/file operations.
4. Validates boundary scenarios (e.g., maximum open file tables, multi-user privilege boundaries, indirect block indexing).
5. Verifies state persistence across reboots.

---

## 📄 License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
