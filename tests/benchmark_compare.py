#!/usr/bin/env python3
import os
import socket
import subprocess
import sys
import tempfile
import time
import statistics
from pathlib import Path
from typing import List, Tuple

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "miniredis-server"
CLIENT = ROOT / "miniredis-client"
RESULT_FILE = ROOT / "tests" / "benchmark_results.txt"


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_for_server(host: str, port: int, timeout: int = 10) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.1)
    raise RuntimeError(f"Server did not come up on {host}:{port}")


def start_server(port: int) -> subprocess.Popen:
    tmp = tempfile.NamedTemporaryFile("w", delete=False, suffix=".conf", dir=str(ROOT))
    tmp.write("port {port}\nstore-size 10000\naof-enabled no\nrdb-enabled no\nactive-expiry-hz 10\n".format(port=port))
    tmp.flush()
    tmp.close()
    cfg_path = tmp.name
    proc = subprocess.Popen([str(BIN), "--config", cfg_path], cwd=str(ROOT), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    wait_for_server("127.0.0.1", port)
    return proc, cfg_path


def stop_server(proc: subprocess.Popen, cfg_path: str) -> None:
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)
    if os.path.exists(cfg_path):
        os.remove(cfg_path)


def send_command(host: str, port: int, command: str) -> str:
    with socket.create_connection((host, port), timeout=2) as sock:
        sock.settimeout(2.0)
        sock.sendall(command.encode("utf-8"))
        data = b""
        while b"END\n" not in data:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
        return data.decode("utf-8", errors="ignore")


def bench_miniredis(host: str, port: int, operations: List[Tuple[str, str, str]], iterations: int) -> List[float]:
    latencies = []
    for _ in range(iterations):
        with socket.create_connection((host, port), timeout=2.0) as sock:
            sock.settimeout(2.0)
            start = time.perf_counter()
            for op in operations:
                cmd_name, key, value = op
                command = f"{cmd_name} {key} {value}" if value else f"{cmd_name} {key}"
                sock.sendall(command.encode("utf-8") + b"\n")
                data = b""
                while b"END\n" not in data:
                    chunk = sock.recv(4096)
                    if not chunk:
                        break
                    data += chunk
            elapsed = time.perf_counter() - start
            latencies.append(elapsed)
    return latencies


def start_sqlite_server(port: int) -> subprocess.Popen:
    script = ROOT / "tests" / "sqlite_network_server.py"
    proc = subprocess.Popen([sys.executable, str(script), str(port)], cwd=str(ROOT), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    wait_for_server("127.0.0.1", port)
    return proc


def stop_sqlite_server(proc: subprocess.Popen) -> None:
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)


def bench_sqlite_baseline(host: str, port: int, operations: List[Tuple[str, str, str]], iterations: int) -> List[float]:
    latencies = []
    for _ in range(iterations):
        with socket.create_connection((host, port), timeout=2.0) as sock:
            sock.settimeout(2.0)
            start = time.perf_counter()
            for op in operations:
                cmd_name, key, value = op
                command = f"{cmd_name} {key} {value}" if value else f"{cmd_name} {key}"
                sock.sendall(command.encode("utf-8") + b"\n")
                data = b""
                while b"END\n" not in data:
                    chunk = sock.recv(4096)
                    if not chunk:
                        break
                    data += chunk
            elapsed = time.perf_counter() - start
            latencies.append(elapsed)
    return latencies


def summarize(values: List[float]) -> dict:
    return {
        "count": len(values),
        "min_s": round(min(values), 6),
        "max_s": round(max(values), 6),
        "avg_s": round(statistics.mean(values), 6),
        "median_s": round(statistics.median(values), 6),
        "ops_per_sec": round(len(values) / sum(values), 3) if sum(values) > 0 else 0.0,
    }


def run_benchmark() -> str:
    if not BIN.exists():
        subprocess.run(["make"], cwd=str(ROOT), check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    port = find_free_port()
    proc, cfg_path = start_server(port)
    sqlite_port = find_free_port()
    sqlite_proc = start_sqlite_server(sqlite_port)
    try:
        workloads = [
            ("Write-only (SET)", [("SET", "k1", "v1"), ("SET", "k2", "v2")], 400),
            ("Read-after-write (SET + GET)", [("SET", "k1", "v1"), ("GET", "k1", "")], 400),
            ("Mixed read/write", [("SET", "k1", "v1"), ("GET", "k1", ""), ("SET", "k2", "v2"), ("GET", "k2", "")], 400),
        ]

        lines = ["Mini Redis vs SQLite benchmark report", "==========================", f"Server port: {port}", ""]
        for name, operations, iterations in workloads:
            mini = bench_miniredis("127.0.0.1", port, operations, iterations)
            sqlite = bench_sqlite_baseline("127.0.0.1", sqlite_port, operations, iterations)
            mini_summary = summarize(mini)
            sqlite_summary = summarize(sqlite)
            ratio = round(mini_summary["avg_s"] / sqlite_summary["avg_s"], 3) if sqlite_summary["avg_s"] > 0 else float("inf")
            lines.append(f"{name}")
            lines.append(f"  Mini Redis avg per batch: {mini_summary['avg_s']:.6f}s ({mini_summary['ops_per_sec']:.2f} batches/s)")
            lines.append(f"  SQLite baseline avg per batch: {sqlite_summary['avg_s']:.6f}s ({sqlite_summary['ops_per_sec']:.2f} batches/s)")
            lines.append(f"  Mini Redis is {ratio}x slower on average")
            lines.append("")

        report = "\n".join(lines)
        print(report)
        RESULT_FILE.write_text(report + "\n", encoding="utf-8")
        return report
    finally:
        stop_server(proc, cfg_path)
        stop_sqlite_server(sqlite_proc)


if __name__ == "__main__":
    run_benchmark()
