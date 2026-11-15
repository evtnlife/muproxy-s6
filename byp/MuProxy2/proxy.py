#!/usr/bin/env python3
"""
MuProxy2 - TCP proxy simples para interceptar pacotes MU/PS (GameGuard).

Modo de uso:
    python proxy.py --listen-port 44405 --remote 54.36.205.234:44405

O proxy escuta localmente e encaminha tudo para o servidor remoto,
logando cada pacote em hex com direção e timestamp.
"""

import argparse
import socket
import threading
import time
from datetime import datetime


def hexdump(data: bytes, max_len: int = 128) -> str:
    truncated = data[:max_len]
    hex_part = truncated.hex(" ")
    ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in truncated)
    if len(data) > max_len:
        hex_part += " ..."
    return f"{hex_part:<48} | {ascii_part}"


def parse_mu_opcode(data: bytes) -> str:
    """Tenta extrair opcode de pacotes MU C1/C2/C3/C4."""
    if len(data) < 2:
        return "-"
    head = data[0]
    if head in (0xC1, 0xC3):
        if len(data) >= 3:
            return f"0x{data[2]:02X}"
    elif head in (0xC2, 0xC4):
        if len(data) >= 4:
            return f"0x{data[3]:02X}"
    return f"head=0x{head:02X}"


def log_packet(direction: str, data: bytes, conn_id: int):
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    op = parse_mu_opcode(data)
    print(f"[{ts}] [{conn_id:04d}] {direction} op={op} len={len(data)} {hexdump(data)}")


def relay(source: socket.socket, dest: socket.socket, direction: str, conn_id: int, stop_event: threading.Event):
    try:
        while not stop_event.is_set():
            data = source.recv(4096)
            if not data:
                break
            log_packet(direction, data, conn_id)
            dest.sendall(data)
    except Exception as e:
        print(f"[!] [{conn_id:04d}] {direction} relay error: {e}")
    finally:
        stop_event.set()
        try:
            source.close()
        except Exception:
            pass
        try:
            dest.close()
        except Exception:
            pass


def handle_client(client_sock: socket.socket, remote_host: str, remote_port: int, conn_id: int):
    try:
        remote_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        remote_sock.connect((remote_host, remote_port))
    except Exception as e:
        print(f"[!] [{conn_id:04d}] cannot connect to remote {remote_host}:{remote_port}: {e}")
        client_sock.close()
        return

    stop = threading.Event()
    t1 = threading.Thread(target=relay, args=(client_sock, remote_sock, "C->S", conn_id, stop), daemon=True)
    t2 = threading.Thread(target=relay, args=(remote_sock, client_sock, "S->C", conn_id, stop), daemon=True)
    t1.start()
    t2.start()
    t1.join()
    t2.join()
    print(f"[*] [{conn_id:04d}] connection closed")


def main():
    parser = argparse.ArgumentParser(description="MuProxy2 - simple TCP proxy for MU/PS packet logging")
    parser.add_argument("--listen-host", default="0.0.0.0", help="Local bind address")
    parser.add_argument("--listen-port", type=int, required=True, help="Local bind port")
    parser.add_argument("--remote", required=True, help="Remote host:port")
    args = parser.parse_args()

    remote_host, remote_port = args.remote.rsplit(":", 1)
    remote_port = int(remote_port)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.listen_host, args.listen_port))
    server.listen(100)

    print(f"[*] MuProxy2 listening on {args.listen_host}:{args.listen_port} -> {remote_host}:{remote_port}")
    print("[*] Press Ctrl+C to stop")

    conn_id = 0
    try:
        while True:
            client_sock, addr = server.accept()
            conn_id += 1
            print(f"[*] [{conn_id:04d}] connection from {addr}")
            threading.Thread(
                target=handle_client,
                args=(client_sock, remote_host, remote_port, conn_id),
                daemon=True,
            ).start()
    except KeyboardInterrupt:
        print("\n[*] shutting down")
    finally:
        server.close()


if __name__ == "__main__":
    main()
