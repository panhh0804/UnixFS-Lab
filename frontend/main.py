import re
import sys
import time
from pathlib import Path

from PyQt6.QtCore import QEvent, Qt
from PyQt6.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QDialog,
    QDialogButtonBox,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMenu,
    QMessageBox,
    QPushButton,
    QPlainTextEdit,
    QSlider,
    QSplitter,
    QTableWidget,
    QTableWidgetItem,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from fs_client import FsClient
from widgets.disk_grid import DiskGridWidget, describe_block_state


ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


class FileEditorDialog(QDialog):
    """文件内容编辑器弹窗"""
    def __init__(self, filename: str, content: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"编辑文件: {filename}")
        self.setMinimumSize(600, 400)
        self.filename = filename
        layout = QVBoxLayout(self)
        self.text = QTextEdit()
        self.text.setPlainText(content)
        layout.addWidget(self.text)
        btns = QDialogButtonBox(QDialogButtonBox.StandardButton.Save | QDialogButtonBox.StandardButton.Cancel)
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        layout.addWidget(btns)

    def get_content(self):
        return self.text.toPlainText()


class MainWindow(QMainWindow):
    def __init__(self, client: FsClient):
        super().__init__()
        self.client = client
        self.setWindowTitle("OS 文件系统可视化监控")
        self.resize(1280, 840)

        self.disk = DiskGridWidget()
        self.disk.blockSelected.connect(self.show_block)
        self.block_label = QLabel("Block: -")
        self.inodes = QTableWidget()
        self.dir_table = QTableWidget()
        self.dir_table.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.dir_table.customContextMenuRequested.connect(self.on_dir_context_menu)
        self.dir_table.itemDoubleClicked.connect(self.on_dir_double_click)
        self.tree = QPlainTextEdit()
        self.tree.setReadOnly(True)
        self.stats = QLabel()
        self.stats.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.terminal = QPlainTextEdit()
        self.terminal.setReadOnly(True)
        self.tree.setMinimumHeight(70)
        self.dir_table.setMinimumHeight(110)
        self.inodes.setMinimumHeight(150)
        self.stats.setMinimumHeight(100)
        self.terminal.setMinimumHeight(150)
        self.command = QLineEdit()
        self.command.setPlaceholderText("login 2118 abcd")
        self.command.returnPressed.connect(self.execute_command)
        self.command.installEventFilter(self)
        self.command_history = []
        self.history_index = 0
        self.command_buttons = []

        self.configure_table(self.inodes)
        self.configure_table(self.dir_table)

        # 逐条测试状态
        self._test_cmds = []
        self._test_idx = 0

        # 块归属映射 block -> list of entries
        self._block_map = {}

        # 历史状态回放
        self._snapshots = []
        self._snapshot_index = -1
        self._live_mode = True

        # 构建各个面板
        legend = QLabel(
            '<span style="color:#f59e0b">■</span> 系统保留（0:根目录，1:/etc，2:口令表）&nbsp;&nbsp;&nbsp;'
            '<span style="color:#22c55e">■</span> 已占用&nbsp;&nbsp;&nbsp;'
            '<span style="color:#263244">■</span> 空闲'
        )
        legend.setObjectName("diskLegend")
        disk_box = self.box("磁盘块占用状态", self.disk, legend, self.block_label)
        inode_box = self.box("Inode 列表", self.inodes)
        tree_box = self.box("目录树", self.tree)
        dir_box = self.box("当前目录", self.dir_table)
        stats_box = self.box("系统统计", self.stats)
        term_box = self.terminal_box()
        term_box.setMinimumHeight(260)

        # 左侧区域：磁盘块在上，目录树和当前目录左右并排在下。
        browse_splitter = QSplitter(Qt.Orientation.Horizontal)
        browse_splitter.addWidget(tree_box)
        browse_splitter.addWidget(dir_box)
        browse_splitter.setStretchFactor(0, 1)
        browse_splitter.setStretchFactor(1, 2)

        left_splitter = QSplitter(Qt.Orientation.Vertical)
        left_splitter.addWidget(disk_box)
        left_splitter.addWidget(browse_splitter)
        left_splitter.setStretchFactor(0, 3)
        left_splitter.setStretchFactor(1, 2)

        # 右侧垂直分割：Inode 列表 | 系统统计
        right_splitter = QSplitter(Qt.Orientation.Vertical)
        right_splitter.addWidget(inode_box)
        right_splitter.addWidget(stats_box)
        right_splitter.setStretchFactor(0, 3)
        right_splitter.setStretchFactor(1, 2)

        # 上半部分水平分割：左 | 右
        top_splitter = QSplitter(Qt.Orientation.Horizontal)
        top_splitter.addWidget(left_splitter)
        top_splitter.addWidget(right_splitter)
        top_splitter.setStretchFactor(0, 3)
        top_splitter.setStretchFactor(1, 2)

        # 顶层垂直分割：上半部分 | 终端
        main_splitter = QSplitter(Qt.Orientation.Vertical)
        main_splitter.addWidget(top_splitter)
        main_splitter.addWidget(term_box)
        main_splitter.setStretchFactor(0, 3)
        main_splitter.setStretchFactor(1, 2)

        # 加宽 splitter 手柄并禁用折叠，让终端区更容易上下拖动。
        for splitter in (browse_splitter, left_splitter, right_splitter, top_splitter, main_splitter):
            splitter.setHandleWidth(16)
            splitter.setChildrenCollapsible(False)
            splitter.setOpaqueResize(True)

        browse_splitter.setSizes([320, 520])
        left_splitter.setSizes([360, 220])
        right_splitter.setSizes([420, 220])
        top_splitter.setSizes([760, 520])
        main_splitter.setSizes([520, 320])

        self.setCentralWidget(main_splitter)

        self.refresh_all()

    def box(self, title, *widgets):
        group = QGroupBox(title)
        layout = QVBoxLayout(group)
        for widget in widgets:
            layout.addWidget(widget)
        return group

    def terminal_box(self):
        group = QGroupBox("命令终端")
        layout = QVBoxLayout(group)

        # 历史回放控制条
        replay_row = QHBoxLayout()
        self.replay_label = QLabel("实时")
        self.replay_slider = QSlider(Qt.Orientation.Horizontal)
        self.replay_slider.setEnabled(False)
        self.replay_slider.valueChanged.connect(self.on_replay_slider)
        self.replay_live_btn = QPushButton("返回实时")
        self.replay_live_btn.setEnabled(False)
        self.replay_live_btn.clicked.connect(self.go_live)
        self.replay_prev_btn = QPushButton("◀")
        self.replay_prev_btn.setEnabled(False)
        self.replay_prev_btn.clicked.connect(self.prev_snapshot)
        self.replay_next_btn = QPushButton("▶")
        self.replay_next_btn.setEnabled(False)
        self.replay_next_btn.clicked.connect(self.next_snapshot)
        replay_row.addWidget(QLabel("状态回放:"))
        replay_row.addWidget(self.replay_label, 1)
        replay_row.addWidget(self.replay_slider)
        replay_row.addWidget(self.replay_prev_btn)
        replay_row.addWidget(self.replay_next_btn)
        replay_row.addWidget(self.replay_live_btn)
        layout.addLayout(replay_row)

        row = QHBoxLayout()
        run_btn = QPushButton("执行")
        run_btn.clicked.connect(self.execute_command)
        refresh_btn = QPushButton("刷新")
        refresh_btn.clicked.connect(self.refresh_all)
        format_btn = QPushButton("格式化")
        format_btn.clicked.connect(self.format_disk)
        auto_test_btn = QPushButton("自动测试")
        auto_test_btn.clicked.connect(self.run_auto_tests)
        step_test_btn = QPushButton("逐条测试")
        step_test_btn.clicked.connect(self.run_step_test)
        row.addWidget(self.command, 1)
        row.addWidget(run_btn)
        row.addWidget(refresh_btn)
        row.addWidget(format_btn)
        row.addWidget(auto_test_btn)
        row.addWidget(step_test_btn)
        self.command_buttons.extend([run_btn, refresh_btn, format_btn, auto_test_btn, step_test_btn])
        layout.addLayout(row)

        layout.addWidget(self.terminal, 1)
        return group

    def execute_command(self):
        cmd = self.command.text().strip()
        if not cmd:
            return
        if not self.command_history or self.command_history[-1] != cmd:
            self.command_history.append(cmd)
        self.history_index = len(self.command_history)
        self.run_text(cmd)
        self.command.clear()

    def run_text(self, cmd):
        try:
            output = self.client.send_command(cmd)
        except Exception as exc:
            QMessageBox.critical(self, "命令失败", str(exc))
            return
        clean = ANSI_RE.sub("", output).strip()
        self.terminal.appendPlainText(f"$ {cmd}")
        if clean:
            self.terminal.appendPlainText(clean)
        self.terminal.appendPlainText("")
        self.refresh_all()

    def format_disk(self):
        reply = QMessageBox.question(self, "确认格式化", "格式化会清空 disk.img，是否继续？")
        if reply == QMessageBox.StandardButton.Yes:
            self.run_text("format yes")

    def set_command_busy(self, busy):
        self.command.setEnabled(not busy)
        for button in self.command_buttons:
            button.setEnabled(not busy)
        QApplication.processEvents()

    def _build_test_cmds(self):
        """构建测试命令列表"""
        cmds = [
            # 先登录
            "login 2118 abcd",
            # 目录操作
            "mkdir workdir", "cd workdir", "pwd", "mkdir subdir", "cd subdir", "pwd",
            "cd ..", "pwd", "cd ..", "pwd",
            "cd nonexist", "pwd",
            # 文件创建与读写
            "create doc1", "write 0 HelloWorld", "close 0",
            "cat doc1", "wc doc1", "stat doc1",
            "open doc1 read", "read 0 20", "close 0",
            # 权限与覆盖
            "create doc2 0644", "dir", "close 0",
            "create doc1", "write 0 Overwritten", "close 0",
            "open doc1 read", "read 0 20", "close 0",
            # append 模式
            "create appendfile", "write 0 First", "close 0",
            "open appendfile append", "write 0 Second", "close 0",
            "cat appendfile",
            # cp / mv / lseek
            "cp doc1 doc1copy",
            "mv doc1copy doc1moved", "stat doc1moved", "cat doc1moved",
            "open doc1moved read", "lseek 0 5", "read 0 10", "close 0",
            # chmod
            "chmod doc2 0755", "dir",
            "chmod doc2 0644", "dir",
            "chmod doc2 0000", "dir",
            "chmod doc2 0777",
            # root 权限演示：普通用户被 0000 拒绝，root 仍可访问
            "create rootdemo RootOnlyData", "close 0",
            "chmod rootdemo 0000",
            "open rootdemo read",
            "logout",
            "login 0 root",
            "open rootdemo read", "read 0 30", "close 0",
            "chmod rootdemo 0777",
            "logout", "login 2118 abcd",
            # df / chown
            "df",
            "create chowntest ChownTestData", "close 0",
            "stat chowntest", "chown 99 chowntest", "stat chowntest",
            "logout", "login 0 root", "chown 2118 chowntest", "stat chowntest",
            "logout", "login 2118 abcd",
            # 大文件多级索引 >5KB
            "create bigfile", "open bigfile write",
        ]
        for i in range(12):
            letter = chr(ord('A') + i)
            cmds.append(f"write 0 {letter * 512}")
        cmds.extend([
            "close 0", "dir",
            "open bigfile read", "read 0 20", "lseek 0 4608", "read 0 20", "close 0",
            # 链接
            "ln doc1moved hlink", "symlink doc1moved slink", "dir",
            "cat hlink", "cat slink",
            # find
            "find / doc1moved",
            # rmdir 非空拒绝
            "mkdir nonempty", "cd nonempty", "create dummy", "close 0",
            "cd ..", "rmdir nonempty",
            "cd nonempty", "delete dummy", "cd ..", "rmdir nonempty",
            # 清理
            "delete doc1", "delete doc1moved", "delete doc2",
            "delete appendfile", "delete bigfile", "delete hlink", "delete slink",
            "delete rootdemo", "delete chowntest",
            "cd workdir", "rmdir subdir", "cd ..", "rmdir workdir",
        ])
        return cmds

    def run_auto_tests(self):
        """自动顺序执行全部测试命令"""
        self.terminal.appendPlainText("\n========== 开始自动测试 ==========\n")
        test_cmds = self._build_test_cmds()
        try:
            for cmd in test_cmds:
                try:
                    resp = self.client.send_command(cmd)
                except Exception as exc:
                    self.terminal.appendPlainText(f"$ {cmd}")
                    self.terminal.appendPlainText(f"[ERROR] {exc}\n")
                    continue
                clean = ANSI_RE.sub("", resp).strip()
                self.terminal.appendPlainText(f"$ {cmd}")
                if clean:
                    self.terminal.appendPlainText(clean)
                self.terminal.appendPlainText("")
                QApplication.processEvents()
                time.sleep(1.0)
                self.refresh_all()
                QApplication.processEvents()
                time.sleep(0.5)
            self.terminal.appendPlainText("========== 自动测试完成 ==========\n")
        except Exception as exc:
            self.terminal.appendPlainText(f"[ERROR] 测试失败: {exc}\n")
        self.refresh_all()

    def run_step_test(self):
        """逐条测试：每点一下执行一条命令"""
        if not self._test_cmds:
            self._test_cmds = self._build_test_cmds()
            self._test_idx = 0
            self.terminal.appendPlainText("\n========== 开始逐条测试 ==========\n")

        if self._test_idx >= len(self._test_cmds):
            self.terminal.appendPlainText("========== 逐条测试已完成 ==========\n")
            self._test_cmds = []
            self._test_idx = 0
            return

        cmd = self._test_cmds[self._test_idx]
        try:
            resp = self.client.send_command(cmd)
        except Exception as exc:
            self.terminal.appendPlainText(f"$ {cmd}")
            self.terminal.appendPlainText(f"[ERROR] {exc}\n")
            self._test_idx += 1
            self.refresh_all()
            return

        clean = ANSI_RE.sub("", resp).strip()
        self.terminal.appendPlainText(f"$ {cmd}")
        if clean:
            self.terminal.appendPlainText(clean)
        self.terminal.appendPlainText("")
        self._test_idx += 1
        self.refresh_all()

    # ========== 目录表右键菜单与双击 ==========
    def on_dir_context_menu(self, pos):
        menu = QMenu(self)
        menu.addAction("新建文件", self.action_new_file)
        menu.addAction("新建目录", self.action_new_dir)
        menu.addSeparator()
        idx = self.dir_table.indexAt(pos)
        if idx.isValid():
            menu.addAction("重命名", self.action_rename)
            menu.addAction("删除", self.action_delete)
            ftype = self.dir_table.item(idx.row(), 2).text() if self.dir_table.columnCount() > 2 else ""
            if ftype == "file":
                menu.addAction("查看块链", self.action_view_blocks)
        menu.addSeparator()
        menu.addAction("刷新", self.refresh_all)
        menu.exec(self.dir_table.viewport().mapToGlobal(pos))

    def action_new_file(self):
        name, ok = QInputDialog.getText(self, "新建文件", "文件名:")
        if not ok or not name:
            return
        content, ok2 = QInputDialog.getMultiLineText(self, "新建文件", "内容:")
        if not ok2:
            return
        self.run_text(f"create {name}")
        if content:
            self.run_text("write 0 " + content)
            self.run_text("close 0")

    def action_new_dir(self):
        name, ok = QInputDialog.getText(self, "新建目录", "目录名:")
        if ok and name:
            self.run_text(f"mkdir {name}")

    def action_rename(self):
        row = self.dir_table.currentRow()
        if row < 0:
            return
        old_name = self.dir_table.item(row, 0).text()
        new_name, ok = QInputDialog.getText(self, "重命名", "新名称:", text=old_name)
        if ok and new_name:
            self.run_text(f"mv {old_name} {new_name}")

    def action_delete(self):
        row = self.dir_table.currentRow()
        if row < 0:
            return
        name = self.dir_table.item(row, 0).text()
        reply = QMessageBox.question(self, "确认删除", f"确定删除 '{name}' 吗？")
        if reply == QMessageBox.StandardButton.Yes:
            self.run_text(f"delete {name}")

    def action_view_blocks(self):
        row = self.dir_table.currentRow()
        if row < 0:
            return
        name = self.dir_table.item(row, 0).text()
        ino = self.dir_table.item(row, 1).text() if self.dir_table.columnCount() > 1 else ""
        lines = [f"文件: {name}  (inode={ino})", "", "占用的磁盘块:"]
        for blk, entries in sorted(self._block_map.items()):
            for e in entries:
                if str(e.get("ino")) == str(ino):
                    role = e.get("role", "?")
                    idx = e.get("index", "?")
                    if role == "data":
                        lines.append(f"  块 {blk}  → 直接块[{idx}]")
                    elif role == "indirect":
                        lines.append(f"  块 {blk}  → 间接索引块")
                    elif role == "indirect_data":
                        lines.append(f"  块 {blk}  → 间接数据块[{idx}]")
        if len(lines) <= 3:
            lines.append("  (无数据块或块信息未加载)")
        dlg = QDialog(self)
        dlg.setWindowTitle(f"块链: {name}")
        dlg.setMinimumSize(400, 300)
        layout = QVBoxLayout(dlg)
        text = QPlainTextEdit()
        text.setReadOnly(True)
        text.setPlainText("\n".join(lines))
        layout.addWidget(text)
        btns = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok)
        btns.accepted.connect(dlg.accept)
        layout.addWidget(btns)
        dlg.exec()

    def _save_snapshot(self, data):
        import copy
        snap = {
            "disk": list(data.get("disk", [])),
            "inodes": data.get("inodes", ([], [])),
            "dir": data.get("dir", ([], [])),
            "tree": data.get("tree", ""),
            "stats": dict(data.get("stats", {})),
            "blocks": dict(data.get("blocks", ({}, []))[0]),
        }
        self._snapshots.append(snap)
        # 最多保留 200 个快照
        if len(self._snapshots) > 200:
            self._snapshots.pop(0)
        self._snapshot_index = len(self._snapshots) - 1
        self.replay_slider.setMaximum(len(self._snapshots) - 1)
        self.replay_slider.setValue(self._snapshot_index)

    def _restore_snapshot(self, index):
        if not (0 <= index < len(self._snapshots)):
            return
        snap = self._snapshots[index]
        self.disk.set_blocks(snap["disk"])
        self.fill_table(self.inodes, *snap["inodes"])
        self.fill_table(self.dir_table, *snap["dir"])
        self.tree.setPlainText(snap["tree"])
        stats = snap["stats"]
        self.stats.setText(
            "\n".join(
                [
                    f"用户: {stats.get('user', '0')}",
                    f"路径: {stats.get('path', '/')}",
                    f"总块: {stats.get('total_blocks', '-')}",
                    f"已用块: {stats.get('used_blocks', '-')}",
                    f"空闲块: {stats.get('free_blocks', '-')}",
                    f"总 inode: {stats.get('total_inodes', '-')}",
                    f"已用 inode: {stats.get('used_inodes', '-')}",
                    f"空闲 inode: {stats.get('free_inodes', '-')}",
                ]
            )
        )
        self._block_map = snap["blocks"]
        self.replay_label.setText(f"快照 {index + 1}/{len(self._snapshots)}")

    def go_live(self):
        self._live_mode = True
        self.replay_slider.setEnabled(False)
        self.replay_live_btn.setEnabled(False)
        self.replay_prev_btn.setEnabled(False)
        self.replay_next_btn.setEnabled(False)
        self.replay_label.setText("实时")
        self.refresh_all()

    def prev_snapshot(self):
        if self._snapshot_index > 0:
            self._snapshot_index -= 1
            self.replay_slider.setValue(self._snapshot_index)
            self._restore_snapshot(self._snapshot_index)

    def next_snapshot(self):
        if self._snapshot_index < len(self._snapshots) - 1:
            self._snapshot_index += 1
            self.replay_slider.setValue(self._snapshot_index)
            self._restore_snapshot(self._snapshot_index)

    def on_replay_slider(self, value):
        if not self._snapshots:
            return
        self._live_mode = False
        self._snapshot_index = value
        self.replay_slider.setEnabled(True)
        self.replay_live_btn.setEnabled(True)
        self.replay_prev_btn.setEnabled(True)
        self.replay_next_btn.setEnabled(True)
        self._restore_snapshot(value)

    def on_dir_double_click(self, item):
        row = item.row()
        name = self.dir_table.item(row, 0).text()
        ftype = self.dir_table.item(row, 2).text() if self.dir_table.columnCount() > 2 else ""
        if ftype == "dir":
            self.run_text(f"cd {name}")
        else:
            try:
                raw = self.client.send_command(f"cat {name}")
                clean = ANSI_RE.sub("", raw)
                parts = clean.split("─────────────────────────────────────")
                content = parts[0].rstrip("\n") if parts else clean
                dlg = FileEditorDialog(name, content, self)
                if dlg.exec() == QDialog.DialogCode.Accepted:
                    new_content = dlg.get_content()
                    self.run_text(f"delete {name}")
                    self.run_text(f"create {name} 0777 {new_content}")
            except Exception as exc:
                QMessageBox.critical(self, "错误", str(exc))

    def refresh_all(self):
        try:
            data = self.client.refresh()
        except Exception as exc:
            self.terminal.appendPlainText(f"刷新失败: {exc}")
            return

        self.disk.set_blocks(data["disk"])
        self.fill_table(self.inodes, *data["inodes"])
        self.fill_table(self.dir_table, *data["dir"])
        self.tree.setPlainText(data["tree"])
        stats = data["stats"]
        self.stats.setText(
            "\n".join(
                [
                    f"用户: {stats.get('user', '0')}",
                    f"路径: {stats.get('path', '/')}",
                    f"总块: {stats.get('total_blocks', '-')}",
                    f"已用块: {stats.get('used_blocks', '-')}",
                    f"空闲块: {stats.get('free_blocks', '-')}",
                    f"总 inode: {stats.get('total_inodes', '-')}",
                    f"已用 inode: {stats.get('used_inodes', '-')}",
                    f"空闲 inode: {stats.get('free_inodes', '-')}",
                ]
            )
        )
        # 保存块归属映射 block -> list of {ino, role, index}
        self._block_map = data.get("blocks", ({}, []))[0]
        if self._live_mode:
            self._save_snapshot(data)

    def fill_table(self, table, headers, rows):
        table.setSortingEnabled(False)
        table.clear()
        table.setColumnCount(len(headers))
        table.setHorizontalHeaderLabels(headers)
        table.setRowCount(len(rows))
        for row_index, row in enumerate(rows):
            for col_index, value in enumerate(row):
                table.setItem(row_index, col_index, QTableWidgetItem(value))
        table.resizeColumnsToContents()
        table.setSortingEnabled(True)

    def configure_table(self, table):
        table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        table.setAlternatingRowColors(True)
        table.setSortingEnabled(True)

    def show_block(self, index):
        state = describe_block_state(index, self.disk.blocks[index])
        info = f"Block: {index}  {state}"
        entries = self._block_map.get(index, [])
        if entries:
            # 查找 inode 对应的文件名
            ino_to_name = {}
            for row in range(self.dir_table.rowCount()):
                ino_item = self.dir_table.item(row, 1)
                name_item = self.dir_table.item(row, 0)
                if ino_item and name_item:
                    ino_to_name[ino_item.text()] = name_item.text()
            parts = []
            for e in entries:
                ino = e.get("ino", "?")
                role = e.get("role", "?")
                idx = e.get("index", "?")
                name = ino_to_name.get(str(ino), f"ino={ino}")
                if role == "system":
                    continue
                elif role == "indirect":
                    parts.append(f"{name} 的间接索引块")
                elif role == "indirect_data":
                    parts.append(f"{name} 的间接数据块[{idx}]")
                else:
                    parts.append(f"{name} 的直接块[{idx}]")
            if parts:
                info += "  |  " + "; ".join(parts)
        self.block_label.setText(info)

    def eventFilter(self, obj, event):
        if obj is self.command and event.type() == QEvent.Type.KeyPress:
            if event.key() == Qt.Key.Key_Up and self.command_history:
                self.history_index = max(0, self.history_index - 1)
                self.command.setText(self.command_history[self.history_index])
                return True
            if event.key() == Qt.Key.Key_Down and self.command_history:
                self.history_index = min(len(self.command_history), self.history_index + 1)
                if self.history_index == len(self.command_history):
                    self.command.clear()
                else:
                    self.command.setText(self.command_history[self.history_index])
                return True
        return super().eventFilter(obj, event)

    def closeEvent(self, event):
        self.client.stop_daemon()
        super().closeEvent(event)


def load_style(app):
    style_path = Path(__file__).resolve().parent / "assets" / "style.qss"
    if style_path.exists():
        app.setStyleSheet(style_path.read_text(encoding="utf-8"))


def main():
    project_root = Path(__file__).resolve().parents[1]
    app = QApplication(sys.argv)
    load_style(app)

    client = FsClient(project_root)
    try:
        client.start_daemon()
    except Exception as exc:
        QMessageBox.critical(None, "启动失败", f"无法启动 fs daemon:\n{exc}")
        return 1

    window = MainWindow(client)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
