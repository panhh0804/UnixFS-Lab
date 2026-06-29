import socket
import subprocess
import time
import sys
from pathlib import Path


class FsClient:
    def __init__(self, project_root: Path, host: str = "127.0.0.1", port: int | None = None):
        self.project_root = Path(project_root)
        self.host = host
        self.port = port
        self.process = None

    def start_daemon(self):
        exe_name = "fs.exe" if sys.platform.startswith("win") else "./fs"
        if self.port is None:
            self.port = self._find_free_port()

        self.process = subprocess.Popen(
            [exe_name, "--daemon", "--tcp", "--host", self.host, "--port", str(self.port)],
            cwd=self.project_root,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        deadline = time.time() + 5
        while time.time() < deadline:
            if self._can_connect():
                return
            if self.process.poll() is not None:
                raise RuntimeError("fs daemon exited during startup")
            time.sleep(0.05)
        raise TimeoutError("fs daemon TCP port was not opened")

    def stop_daemon(self):
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()

    def _can_connect(self):
        try:
            with socket.create_connection((self.host, self.port), timeout=0.2):
                return True
        except OSError:
            return False

    def _find_free_port(self) -> int:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.bind((self.host, 0))
            return sock.getsockname()[1]

    def send_command(self, cmd: str) -> str:
        data = b""
        with socket.create_connection((self.host, self.port), timeout=3) as sock:
            sock.sendall((cmd.rstrip("\n") + "\n").encode("utf-8"))
            while b"__END__\n" not in data:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                data += chunk
        return data.replace(b"__END__\n", b"").decode("utf-8", errors="replace")

    def refresh(self):
        sections = self._parse_snapshot(self.send_command("__snapshot__"))
        return {
            "disk": self._parse_disk(sections.get("disk", "")),
            "inodes": self._parse_table(sections.get("inodes", "")),
            "dir": self._parse_table(sections.get("dir", "")),
            "tree": sections.get("tree", ""),
            "stats": self._parse_stats(sections.get("stats", "")),
            "blocks": self._parse_block_info(sections.get("blocks", "")),
        }

    @staticmethod
    def _parse_block_info(text: str):
        lines = [line for line in text.splitlines() if line.strip()]
        if not lines:
            return {}, []
        headers = lines[0].split("|")
        rows = [line.split("|") for line in lines[1:]]
        mapping = {}
        for row in rows:
            if len(row) >= 4:
                try:
                    blk = int(row[0])
                    mapping.setdefault(blk, []).append({
                        "ino": row[1],
                        "role": row[2],
                        "index": row[3],
                    })
                except ValueError:
                    pass
        return mapping, headers

    @staticmethod
    def _parse_snapshot(text: str):
        sections = {}
        current = None
        lines = []

        for line in text.splitlines():
            if line.startswith("__SECTION__ "):
                if current is not None:
                    sections[current] = "\n".join(lines)
                current = line.split(" ", 1)[1].strip()
                lines = []
            elif current is not None:
                lines.append(line)
        if current is not None:
            sections[current] = "\n".join(lines)
        return sections

    @staticmethod
    def _parse_disk(text: str):
        values = []
        for item in text.split():
            try:
                values.append(int(item))
            except ValueError:
                pass
        return values[:512]

    @staticmethod
    def _parse_table(text: str):
        lines = [line for line in text.splitlines() if line.strip()]
        if not lines:
            return [], []
        headers = lines[0].split("|")
        rows = [line.split("|") for line in lines[1:]]
        return headers, rows

    @staticmethod
    def _parse_stats(text: str):
        result = {}
        for line in text.splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                result[key] = value
        return result
