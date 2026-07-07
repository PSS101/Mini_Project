#!/usr/bin/env python3
import socket
import sqlite3
import sys
import threading


def handle_client(client_sock: socket.socket, db_path: str) -> None:
    buffer = ""
    db = sqlite3.connect(db_path, timeout=30, check_same_thread=False)
    set_stmt = db.cursor()
    get_stmt = db.cursor()
    try:
        while True:
            data = client_sock.recv(4096)
            if not data:
                break
            buffer += data.decode("utf-8", errors="ignore")
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue
                parts = line.split()
                if not parts:
                    continue

                cmd = parts[0].upper()
                if cmd == "SET" and len(parts) == 3:
                    set_stmt.execute("INSERT INTO kv(k, v) VALUES(?, ?) ON CONFLICT(k) DO UPDATE SET v=excluded.v", (parts[1], parts[2]))
                    db.commit()
                    reply = "OK"
                elif cmd == "GET" and len(parts) == 2:
                    get_stmt.execute("SELECT v FROM kv WHERE k = ?", (parts[1],))
                    row = get_stmt.fetchone()
                    reply = row[0] if row else ""
                else:
                    reply = "ERR"
                client_sock.sendall(reply.encode("utf-8") + b"\r\nEND\n")
    finally:
        db.close()
        try:
            client_sock.close()
        except OSError:
            pass


def main() -> None:
    if len(sys.argv) < 3:
        raise SystemExit("usage: sqlite_network_server.py <port> <db_path>")

    port = int(sys.argv[1])
    db_path = sys.argv[2]
    db = sqlite3.connect(db_path, timeout=30, check_same_thread=False)
    db.execute("PRAGMA journal_mode = WAL")
    db.execute("PRAGMA synchronous = NORMAL")
    db.execute("PRAGMA cache_size = 10000")
    db.execute("CREATE TABLE IF NOT EXISTS kv (k TEXT PRIMARY KEY, v TEXT)")
    db.close()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", port))
        server.listen(50)

        while True:
            client, _ = server.accept()
            thread = threading.Thread(target=handle_client, args=(client, db_path), daemon=True)
            thread.start()


if __name__ == "__main__":
    main()
