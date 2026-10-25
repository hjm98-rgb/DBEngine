#!/usr/bin/env python3
"""
通用 Python 服务器 (doubao-universal-server 核心脚本)

一个不限定用途、任意游戏 / 普通应用都能连接的通用服务器。
基于 Python 标准库 (socket + asyncio)，无需第三方依赖。

用法:
  python3 server.py start  <port> [--proto tcp|udp] [--mode echo|relay|http] [--name NAME] [--host HOST]
  python3 server.py list
  python3 server.py status [name|port|all]
  python3 server.py info   <name|port>
  python3 server.py stop   <name|port|all>
  python3 server.py run    <name>              # 前台运行单个实例(一般不用手动调)

示例:
  python3 server.py start 25565                        # 开 TCP 25565，echo 模式(测试连通用)
  python3 server.py start 8080 --mode http             # 开一个简单网页服务器
  python3 server.py start 9999 --mode relay --name lobby  # 多人中继(广播聊天/简单联机大厅)
  python3 server.py start 6000 --proto udp              # 开一个 UDP 端口(很多游戏用 UDP)
  python3 server.py list
  python3 server.py stop all

说明:
  - 每个实例独立占一个端口，互不冲突，可同时开多个("好几台")。
  - 连接地址 = <IP>:<port>，IP 用 info/list 里的地址分享给别人。
  - 模式:
      echo  : 收到什么回什么(适合测试能否连通)
      relay : 所有已连接客户端互相广播消息(聊天室/简单联机大厅)
      http  : 返回一个简单网页(适合当网页/应用入口)
"""
import argparse
import json
import os
import socket
import subprocess
import sys
import time

try:
    import asyncio
except ImportError:  # 极老 Python 兜底
    asyncio = None

# ---------- 状态目录 ----------
STATE_DIR = os.path.join(os.path.expanduser("~"), ".userver")
STATE_FILE = os.path.join(STATE_DIR, "state.json")
LOG_DIR = os.path.join(STATE_DIR, "logs")


def _ensure_state_dir():
    os.makedirs(STATE_DIR, exist_ok=True)
    os.makedirs(LOG_DIR, exist_ok=True)


def _load_state():
    _ensure_state_dir()
    if not os.path.exists(STATE_FILE):
        return {}
    try:
        with open(STATE_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def _save_state(state):
    _ensure_state_dir()
    tmp = STATE_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(state, f, ensure_ascii=False, indent=2)
    os.replace(tmp, STATE_FILE)


def _lan_ip():
    """探测本机局域网 IP；优先返回非环回、非链路本地(169.254)的可路由地址。"""
    candidates = []
    # 方法一：hostname -I (Linux/macOS/Unix 最准)
    try:
        out = subprocess.run(["hostname", "-I"], capture_output=True,
                             text=True, timeout=3).stdout
        candidates.extend(out.split())
    except Exception:
        pass
    # 方法二：路由选择法(兜底)
    if not candidates:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("8.8.8.8", 80))
            candidates.append(s.getsockname()[0])
        except Exception:
            pass
        finally:
            s.close()
    for ip in candidates:
        if not ip.startswith("127.") and not ip.startswith("169.254."):
            return ip
    return candidates[0] if candidates else "127.0.0.1"


def _is_port_open(port, proto="tcp", host="0.0.0.0"):
    try:
        if proto == "udp":
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind((host, int(port)))
            s.close()
            return True
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, int(port)))
        s.close()
        return True
    except OSError:
        return False


def _pid_alive(pid):
    if not pid:
        return False
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


# =========================================================
# 服务实现 (在子进程/前台进程里跑)
# =========================================================
async def _handle_tcp(reader, writer, mode, name):
    addr = writer.get_extra_info("peername")
    try:
        if mode == "http":
            # 简单网页服务器
            body = ("<html><head><meta charset='utf-8'></head>"
                    "<body><h1>doubao-universal-server</h1>"
                    "<p>通用服务器在线: <b>%s</b></p></body></html>") % (name,)
            resp = ("HTTP/1.1 200 OK\r\nContent-Type: text/html; "
                    "charset=utf-8\r\nContent-Length: %d\r\nConnection: "
                    "close\r\n\r\n%s") % (len(body.encode("utf-8")), body)
            writer.write(resp.encode("utf-8"))
            await writer.drain()
            return
        if mode == "relay":
            # 加入广播房间
            await _relay_loop(reader, writer, name)
            return
        # echo 模式
        while True:
            data = await reader.read(4096)
            if not data:
                break
            writer.write(data)
            await writer.drain()
    except Exception:
        pass
    finally:
        try:
            writer.close()
        except Exception:
            pass


async def _relay_loop(reader, writer, name):
    """relay 模式：把某个客户端收到的消息广播给该实例所有其它连接。"""
    global RELAY_ROOMS
    room = RELAY_ROOMS.setdefault(name, set())
    room.add(writer)
    try:
        while True:
            data = await reader.read(4096)
            if not data:
                break
            dead = []
            for w in list(room):
                if w is writer:
                    continue
                try:
                    w.write(data)
                    await w.drain()
                except Exception:
                    dead.append(w)
            for w in dead:
                room.discard(w)
    except Exception:
        pass
    finally:
        room.discard(writer)
        try:
            writer.close()
        except Exception:
            pass


RELAY_ROOMS = {}


async def _udp_serve(port, host, mode, name):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    sock.setblocking(False)
    loop = asyncio.get_event_loop()
    while True:
        try:
            data, addr = await loop.sock_recvfrom(sock, 4096)
        except Exception:
            continue
        if mode == "http":
            pass  # UDP 不提供 http
        elif mode == "relay":
            pass  # UDP 中继留待扩展
        else:
            try:
                await loop.sock_sendto(sock, data, addr)
            except Exception:
                pass


async def _run_instance(info):
    host, port, proto, mode, name = (
        info["host"], int(info["port"]), info["proto"], info["mode"], info["name"])
    # 绑定成功后立刻上报状态(pid/端口/地址)，便于 list/info 使用
    state = _load_state()
    lan = _lan_ip()
    state[name] = {
        "pid": os.getpid(),
        "port": port,
        "proto": proto,
        "mode": mode,
        "host": host,
        "started_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "lan_ip": lan,
        "public": host == "0.0.0.0",
    }
    _save_state(state)

    if proto == "udp":
        await _udp_serve(port, host, mode, name)
    else:
        server = await asyncio.start_server(
            lambda r, w: _handle_tcp(r, w, mode, name),
            host, port)
        async with server:
            await server.serve_forever()


def _cmd_run(args):
    # run 直接接收完整配置，不依赖 state 预先写入
    info = {
        "name": args.name,
        "host": args.host,
        "port": args.port,
        "proto": args.proto,
        "mode": args.mode,
    }
    if asyncio is None:
        print("ERROR: 需要 Python3 的 asyncio")
        sys.exit(1)
    try:
        asyncio.run(_run_instance(info))
    except KeyboardInterrupt:
        pass


def _cmd_start(args):
    port = int(args.port)
    if port < 1 or port > 65535:
        print("ERROR: 端口必须 1-65535")
        sys.exit(1)
    host = args.host
    proto = args.proto
    mode = args.mode
    name = args.name or ("%s-%s" % (proto, port))

    # 端口占用检查(仅本机绑定视角，提示用)
    if not _is_port_open(port, proto):
        print("ERROR: 端口 %s/%s 已被占用或不可用" % (port, proto))
        sys.exit(1)

    state = _load_state()
    if name in state and _pid_alive(state[name].get("pid")):
        print("WARN: 实例 %s 已在运行(pid=%s)。如需换端口请 stop 后再 start。" % (
            name, state[name].get("pid")))
        sys.exit(1)

    _ensure_state_dir()
    logfile = os.path.join(LOG_DIR, name + ".log")
    script = os.path.abspath(__file__)
    env = dict(os.environ)
    proc = subprocess.Popen(
        [sys.executable, script, "run", name,
         "--host", host, "--port", str(port),
         "--proto", proto, "--mode", mode],
        stdout=open(logfile, "a"), stderr=subprocess.STDOUT,
        start_new_session=True, env=env)

    # 等待实例写入 state(最多 5 秒)
    info = None
    for _ in range(50):
        time.sleep(0.1)
        st = _load_state()
        if name in st:
            info = st[name]
            break

    if info is None:
        print("ERROR: 实例启动失败，见日志: %s" % logfile)
        sys.exit(1)

    lan_ip = info["lan_ip"]
    print("✅ 服务器已启动: %s (pid=%s)" % (name, info["pid"]))
    print("   模式 : %s / %s" % (proto, mode))
    print("   局域网内别人连接:  %s:%s" % (lan_ip, port))
    print("   本机自测连接:      127.0.0.1:%s" % port)
    print("   公网连接:          需端口映射或内网穿透，见技能 references/public-access.md")
    return info


def _cmd_list(args):
    state = _load_state()
    if not state:
        print("(没有正在运行的实例。用: python3 server.py start <port>)")
        return
    print("%-16s %-6s %-5s %-6s %-8s %s" % (
        "NAME", "PORT", "PROTO", "MODE", "PID", "LAN_IP"))
    for name, info in sorted(state.items()):
        alive = "运行" if _pid_alive(info.get("pid")) else "已退出"
        print("%-16s %-6s %-5s %-6s %-8s %s [%s]" % (
            name, info["port"], info["proto"], info["mode"],
            info.get("pid"), info.get("lan_ip"), alive))


def _cmd_status(args):
    state = _load_state()
    key = args.target if args.target != "all" else None
    items = [state[k] for k in state] if key is None else (
        [state[k] for k in state if k == key or str(state[k]["port"]) == key])
    if not items:
        print("没有匹配的实例。")
        return
    for info in items:
        alive = _pid_alive(info.get("pid"))
        print("%s:%s (%s/%s) pid=%s → %s" % (
            info["lan_ip"], info["port"], info["proto"], info["mode"],
            info.get("pid"), "运行中" if alive else "已退出"))


def _cmd_info(args):
    state = _load_state()
    for name, info in state.items():
        if name == args.target or str(info["port"]) == args.target:
            print("实例    : %s" % name)
            print("连接地址: %s:%s" % (info["lan_ip"], info["port"]))
            print("协议    : %s  模式: %s" % (info["proto"], info["mode"]))
            print("本机自测: 127.0.0.1:%s" % info["port"])
            return
    print("没有找到实例: %s" % args.target)


def _cmd_stop(args):
    state = _load_state()
    key = args.target
    targets = list(state.keys()) if key == "all" else [
        k for k in state if k == key or str(state[k]["port"]) == key]
    if not targets:
        print("没有匹配的实例，无需停止。")
        return
    for name in targets:
        info = state[name]
        if _pid_alive(info.get("pid")):
            try:
                os.kill(info["pid"], 15)
            except OSError:
                pass
        state.pop(name, None)
        print("已停止: %s (%s:%s)" % (name, info["lan_ip"], info["port"]))
    _save_state(state)


def main():
    ap = argparse.ArgumentParser(prog="server.py", description="通用 Python 多端口服务器")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("start", help="启动一个新实例")
    p.add_argument("port")
    p.add_argument("--proto", choices=["tcp", "udp"], default="tcp")
    p.add_argument("--mode", choices=["echo", "relay", "http"], default="echo")
    p.add_argument("--name", default=None)
    p.add_argument("--host", default="0.0.0.0")

    p = sub.add_parser("list", help="列出所有实例")
    p = sub.add_parser("status", help="查看状态")
    p.add_argument("target", nargs="?", default="all")
    p = sub.add_parser("info", help="查看连接信息")
    p.add_argument("target")
    p = sub.add_parser("stop", help="停止实例")
    p.add_argument("target", help="实例名 / 端口 / all")
    p = sub.add_parser("run", help="前台运行单个实例(内部用)")
    p.add_argument("name")
    p.add_argument("--port", default="0")
    p.add_argument("--proto", choices=["tcp", "udp"], default="tcp")
    p.add_argument("--mode", choices=["echo", "relay", "http"], default="echo")
    p.add_argument("--host", default="0.0.0.0")

    args = ap.parse_args()
    {"start": _cmd_start, "list": _cmd_list, "status": _cmd_status,
     "info": _cmd_info, "stop": _cmd_stop, "run": _cmd_run}[args.cmd](args)


if __name__ == "__main__":
    main()
