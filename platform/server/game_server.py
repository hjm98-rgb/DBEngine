#!/usr/bin/env python3
"""
game_server.py — 你自己的 Python 多人实时联机模块（可 import 到你的游戏里）

只依赖 Python 标准库。分服务端(GameServer)和客户端(GameClient)两部分。
消息协议：每条消息 = 4字节大端长度 + JSON(UTF-8)。安全、易懂、无第三方依赖。

用法（在你的游戏里）:

  服务端（一台设备跑，负责接收所有玩家）:
    from game_server import GameServer
    srv = GameServer(host="0.0.0.0", port=25565)

    def on_msg(cid, data):             # data 是 dict/str/数字等
        srv.broadcast(data)             # 把消息发给所有玩家

    srv.on_message = on_msg
    srv.on_join   = lambda cid: srv.broadcast({"type":"join","id":cid})
    srv.start_tick(0.1, lambda s: s.broadcast({"type":"tick","t":s.tick()}))
    srv.run()

  客户端（每个玩家的游戏里连进来）:
    from game_server import GameClient
    cli = GameClient(host="192.168.0.196", port=25565)

    def on_msg(data):
        print("收到:", data)

    cli.on_message = on_msg
    cli.connect()
    cli.send({"type":"move","x":1,"y":2})   # 发自己的操作/位置
    cli.run()
"""
import json
import select
import socket
import struct
import threading
import time

__all__ = ["GameServer", "GameClient", "encode", "decode"]


def encode(payload):
    """把任意可 JSON 序列化的数据打包成帧(bytes)。"""
    if isinstance(payload, (bytes, bytearray)):
        body = bytes(payload)
    else:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    return struct.pack(">I", len(body)) + body


def decode_frame(data):
    """从 data 中解析出一条帧；不足一条返回 (None, data)。"""
    if len(data) < 4:
        return None, data
    n = struct.unpack(">I", data[:4])[0]
    if len(data) < 4 + n:
        return None, data
    body = data[4:4 + n]
    try:
        obj = json.loads(body.decode("utf-8"))
    except Exception:
        obj = body
    return obj, data[4 + n:]


def _read_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def _send_framed(sock, payload):
    sock.sendall(encode(payload))


# =========================================================
# 服务端
# =========================================================
class GameServer:
    def __init__(self, host="0.0.0.0", port=25565):
        self.host = host
        self.port = port
        self.on_message = None   # fn(cid, data)
        self.on_join = None      # fn(cid)
        self.on_leave = None     # fn(cid)
        self._clients = {}       # cid -> (socket, reader_thread)
        self._lock = threading.Lock()
        self._next_id = 1
        self._tick = 0
        self._running = False
        self._sock = None

    # ---- 连接管理 ----
    def _accept_loop(self):
        while self._running:
            try:
                readable, _, _ = select.select([self._sock], [], [], 0.5)
            except Exception:
                break
            if not readable:
                continue
            try:
                conn, _ = self._sock.accept()
            except Exception:
                continue
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            with self._lock:
                cid = self._next_id
                self._next_id += 1
                t = threading.Thread(target=self._reader, args=(cid, conn),
                                     daemon=True)
                self._clients[cid] = (conn, t)
            t.start()
            if self.on_join:
                try:
                    self.on_join(cid)
                except Exception:
                    pass

    def _reader(self, cid, conn):
        buf = b""
        try:
            while self._running:
                frame, buf = decode_frame(buf)
                if frame is None:
                    chunk = conn.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                    frame, buf = decode_frame(buf)
                    if frame is None:
                        continue
                if self.on_message:
                    try:
                        self.on_message(cid, frame)
                    except Exception:
                        pass
        except Exception:
            pass
        finally:
            self._remove(cid, conn)

    def _remove(self, cid, conn):
        with self._lock:
            self._clients.pop(cid, None)
        try:
            conn.close()
        except Exception:
            pass
        if self.on_leave:
            try:
                self.on_leave(cid)
            except Exception:
                pass

    # ---- 对外接口 ----
    def player_count(self):
        with self._lock:
            return len(self._clients)

    def client_ids(self):
        with self._lock:
            return list(self._clients.keys())

    def send(self, cid, data):
        with self._lock:
            entry = self._clients.get(cid)
        if entry:
            try:
                _send_framed(entry[0], data)
            except Exception:
                pass

    def broadcast(self, data, exclude=None):
        exclude = set(exclude or [])
        with self._lock:
            items = [(c, s) for c, (s, _) in self._clients.items()
                     if c not in exclude]
        for cid, sock in items:
            try:
                _send_framed(sock, data)
            except Exception:
                pass

    def tick(self):
        return self._tick

    def start_tick(self, interval, callback):
        """按 interval 秒周期调用 callback(self)，用于实时状态同步。
        可安全地在 run() 之前或之后调用。"""
        def loop():
            while not self._running:   # 等服务端真正启动
                time.sleep(0.05)
            while self._running:
                self._tick += 1
                try:
                    callback(self)
                except Exception:
                    pass
                time.sleep(interval)
        t = threading.Thread(target=loop, daemon=True)
        t.start()

    def run(self):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((self.host, self.port))
        self._sock.listen(32)
        self._running = True
        print("[game_server] 服务端已启动 %s:%s (等待玩家连接...)" %
              (self.host, self.port))
        try:
            self._accept_loop()
        finally:
            self.stop()

    def stop(self):
        self._running = False
        with self._lock:
            socks = [s for s, _ in self._clients.values()]
            self._clients.clear()
        for s in socks:
            try:
                s.close()
            except Exception:
                pass
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass


# =========================================================
# 客户端
# =========================================================
class GameClient:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.on_open = None      # fn()
        self.on_message = None   # fn(data)
        self.on_close = None     # fn()
        self._sock = None
        self._running = False

    def connect(self):
        self._sock = socket.create_connection((self.host, self.port), timeout=5)
        self._sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self._running = True
        if self.on_open:
            try:
                self.on_open()
            except Exception:
                pass
        threading.Thread(target=self._reader, daemon=True).start()
        print("[game_client] 已连接 %s:%s" % (self.host, self.port))

    def _reader(self):
        buf = b""
        try:
            while self._running:
                frame, buf = decode_frame(buf)
                if frame is None:
                    chunk = self._sock.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                    frame, buf = decode_frame(buf)
                    if frame is None:
                        continue
                if self.on_message:
                    try:
                        self.on_message(frame)
                    except Exception:
                        pass
        except Exception:
            pass
        finally:
            self._running = False
            try:
                self._sock.close()
            except Exception:
                pass
            if self.on_close:
                try:
                    self.on_close()
                except Exception:
                    pass

    def send(self, data):
        if self._sock:
            try:
                _send_framed(self._sock, data)
            except Exception:
                pass

    def run(self):
        try:
            while self._running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            pass

    def stop(self):
        self._running = False
        try:
            self._sock.close()
        except Exception:
            pass


if __name__ == "__main__":
    # 自测：standalone 跑服务端
    import sys
    host = sys.argv[1] if len(sys.argv) > 1 else "0.0.0.0"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 25565
    srv = GameServer(host=host, port=port)

    def on_msg(cid, data):
        srv.broadcast({"type": "echo", "from": cid, "data": data})

    srv.on_message = on_msg

    srv.start_tick(1.0, lambda s: s.broadcast({"type": "tick", "t": s.tick()}))
    srv.run()
