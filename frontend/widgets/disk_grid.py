from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QColor, QPainter, QPen
from PyQt6.QtWidgets import QWidget


# 前端网格显示的是数据区相对块号，不是 disk.img 的绝对物理块号。
# 数据区 0/1/2 在格式化时被根目录、/etc 和 /etc/password 固定占用。
SYSTEM_BLOCK_DESCRIPTIONS = {
    0: "系统保留（根目录 / 的目录数据块）",
    1: "系统保留（/etc 目录的数据块）",
    2: "系统保留（/etc/password 用户口令表）",
}


def describe_block_state(index, occupied=False):
    if index in SYSTEM_BLOCK_DESCRIPTIONS:
        return SYSTEM_BLOCK_DESCRIPTIONS[index]
    return "已占用" if occupied else "空闲"


class DiskGridWidget(QWidget):
    blockSelected = pyqtSignal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.blocks = [0] * 512
        self.selected = -1
        self.setMinimumSize(560, 300)
        self.setMouseTracking(True)

    def set_blocks(self, blocks):
        self.blocks = list(blocks)[:512]
        if len(self.blocks) < 512:
            self.blocks.extend([0] * (512 - len(self.blocks)))
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, False)

        margin = 10
        gap = 2
        cols = 32
        rows = 16
        cell_w = (self.width() - margin * 2 - gap * (cols - 1)) / cols
        cell_h = (self.height() - margin * 2 - gap * (rows - 1)) / rows
        cell = max(4, min(cell_w, cell_h))
        x0 = (self.width() - (cell * cols + gap * (cols - 1))) / 2
        y0 = (self.height() - (cell * rows + gap * (rows - 1))) / 2

        for index in range(512):
            row = index // cols
            col = index % cols
            x = x0 + col * (cell + gap)
            y = y0 + row * (cell + gap)
            if index < 3:
                color = QColor("#f59e0b")
            elif self.blocks[index]:
                color = QColor("#22c55e")
            else:
                color = QColor("#263244")
            painter.fillRect(int(x), int(y), int(cell), int(cell), color)
            if index == self.selected:
                painter.setPen(QPen(QColor("#e5e7eb"), 2))
                painter.drawRect(int(x), int(y), int(cell), int(cell))

    def mousePressEvent(self, event):
        if event.button() != Qt.MouseButton.LeftButton:
            return
        index = self._index_at(event.position().x(), event.position().y())
        if index is not None:
            self.selected = index
            self.blockSelected.emit(index)
            self.update()

    def mouseMoveEvent(self, event):
        index = self._index_at(event.position().x(), event.position().y())
        if index is None:
            self.setToolTip("")
            return
        state = describe_block_state(index, self.blocks[index])
        self.setToolTip(f"Block {index}: {state}")

    def _index_at(self, px, py):
        margin = 10
        gap = 2
        cols = 32
        rows = 16
        cell_w = (self.width() - margin * 2 - gap * (cols - 1)) / cols
        cell_h = (self.height() - margin * 2 - gap * (rows - 1)) / rows
        cell = max(4, min(cell_w, cell_h))
        x0 = (self.width() - (cell * cols + gap * (cols - 1))) / 2
        y0 = (self.height() - (cell * rows + gap * (rows - 1))) / 2
        col = int((px - x0) // (cell + gap))
        row = int((py - y0) // (cell + gap))
        if col < 0 or col >= cols or row < 0 or row >= rows:
            return None
        x = x0 + col * (cell + gap)
        y = y0 + row * (cell + gap)
        if px > x + cell or py > y + cell:
            return None
        return row * cols + col
