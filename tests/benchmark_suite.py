#!/usr/bin/env python3
import argparse
import csv
import math
import os
import random
import socket
import statistics
import string
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[1]
MINIREDIS_BIN = ROOT / "miniredis-server"
SQLITE_SERVER_SCRIPT = ROOT / "tests" / "sqlite_network_server.py"
RESULTS_CSV = ROOT / "tests" / "benchmark_suite_results.csv"
REPORT_MD = ROOT / "docs" / "BENCHMARK_SUITE.md"

DEFAULT_REPEAT = 5
DEFAULT_WARMUP_OPS = 2000
DEFAULT_MAX_MEMORY_FRACTION = 0.4

@dataclass
class ServerProcess:
    proc: subprocess.Popen
    config_path: Optional[str]


@dataclass
class BenchmarkResult:
    system: str
    workload: str
    value_size: int
    dataset_size: int
    thread_count: int
    repeat: int
    total_ops: int
    total_time_s: float
    throughput_ops_s: float
    avg_latency_s: float
    p95_latency_s: float
    min_latency_s: float
    max_latency_s: float
    stddev_latency_s: float
    server_rss_kb: int
    notes: str


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_for_port(port: int, timeout: float = 10.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.3):
                return
        except OSError:
            time.sleep(0.1)
    raise RuntimeError(f"Server did not bind to port {port} within {timeout} seconds")


def get_rss_kb(pid: int) -> int:
    try:
        with open(f"/proc/{pid}/status", "r") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    except FileNotFoundError:
        return 0
    return 0


def make_value(size: int) -> str:
    return "x" * size


def key_name(prefix: str, i: int) -> str:
    return f"{prefix}:{i}"


def maybe_skip_combo(dataset_size: int, value_size: int) -> bool:
    try:
        pages = os.sysconf("SC_PAGE_SIZE")
        phys = os.sysconf("SC_PHYS_PAGES")
        total_mem = pages * phys
    except (ValueError, OSError):
        total_mem = 4 * 1024**3
    threshold = min(total_mem * DEFAULT_MAX_MEMORY_FRACTION, 4 * 1024**3)
    estimated = dataset_size * (value_size + 64)
    return estimated > threshold


def start_miniredis(port: int, store_size: int) -> ServerProcess:
    tmp = tempfile.NamedTemporaryFile("w", delete=False, suffix=".conf", dir=str(ROOT))
    tmp.write("port {port}\nstore-size {store}\naof-enabled no\nrdb-enabled no\nactive-expiry-hz 10\n".format(port=port, store=store_size))
    tmp.flush()
    tmp.close()
    proc = subprocess.Popen([str(MINIREDIS_BIN), "--config", tmp.name], cwd=str(ROOT), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    wait_for_port(port)
    return ServerProcess(proc=proc, config_path=tmp.name)


def stop_server(server: ServerProcess) -> None:
    if server.proc.poll() is None:
        server.proc.terminate()
        try:
            server.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.proc.kill()
            server.proc.wait(timeout=5)
    if server.config_path and os.path.exists(server.config_path):
        os.remove(server.config_path)


def start_sqlite_server(port: int, db_path: str) -> ServerProcess:
    proc = subprocess.Popen([sys.executable, str(SQLITE_SERVER_SCRIPT), str(port), db_path], cwd=str(ROOT), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    wait_for_port(port)
    return ServerProcess(proc=proc, config_path=None)


def stop_sqlite_server(server: ServerProcess) -> None:
    stop_server(server)


RESPONSE_TERMINATOR = b"END\n"

class PooledTCPClient:
    def __init__(self, host: str, port: int):
        self.sock = socket.create_connection((host, port), timeout=10.0)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.settimeout(15.0)

    def send_command(self, command: str) -> str:
        self.sock.sendall(command.encode("utf-8") + b"\n")
        data = b""
        deadline = time.monotonic() + 15.0
        while RESPONSE_TERMINATOR not in data:
            if time.monotonic() > deadline:
                raise TimeoutError("timeout waiting for server response")
            chunk = self.sock.recv(4096)
            if not chunk:
                break
            data += chunk
        decoded = data.decode("utf-8", errors="ignore")
        return decoded.replace("\r\nEND\n", "").replace("END\n", "").strip()

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass


def generate_workload(workload: str, dataset_size: int, total_ops: int, value_size: int) -> List[Tuple[str, str, str]]:
    value = make_value(value_size)
    if workload == "write-heavy":
        return [("SET", key_name("k", i % dataset_size), value) for i in range(total_ops)]
    if workload == "read-heavy":
        return [("GET", key_name("k", random.randrange(dataset_size)), "") for _ in range(total_ops)]
    if workload == "mixed":
        ops = []
        for i in range(total_ops):
            if i % 2 == 0:
                ops.append(("SET", key_name("k", i % dataset_size), value))
            else:
                ops.append(("GET", key_name("k", random.randrange(dataset_size)), ""))
        return ops
    if workload == "read-after-write":
        pairs = total_ops // 2
        ops = []
        for i in range(pairs):
            key = key_name("rw", i)
            ops.append(("SET", key, value))
            ops.append(("GET", key, ""))
        return ops
    if workload == "random-access":
        key_space = max(dataset_size, 100000)
        return [("GET", key_name("k", random.randrange(key_space)), "") if random.random() < 0.5 else ("SET", key_name("k", random.randrange(key_space)), value) for _ in range(total_ops)]
    raise ValueError(f"Unknown workload: {workload}")


def preload_keys(client: PooledTCPClient, dataset_size: int, value_size: int) -> None:
    value = make_value(value_size)
    for i in range(dataset_size):
        client.send_command(f"SET {key_name('k', i)} {value}")


def run_operation_batch(host: str, port: int, operations: List[Tuple[str, str, str]], thread_count: int) -> Tuple[List[float], int]:
    all_latencies: List[float] = []
    lock = threading.Lock()
    chunk_size = math.ceil(len(operations) / thread_count)
    error_count = 0

    def worker(sub_ops: List[Tuple[str, str, str]]) -> None:
        nonlocal error_count
        try:
            client = PooledTCPClient(host, port)
        except Exception as exc:
            with lock:
                error_count += len(sub_ops)
            print(f"Connection failed for {host}:{port}: {exc}")
            return

        local_latencies: List[float] = []
        for op in sub_ops:
            try:
                start = time.perf_counter()
                client.send_command("{} {} {}".format(op[0], op[1], op[2]).strip())
                local_latencies.append(time.perf_counter() - start)
            except Exception as exc:
                with lock:
                    error_count += 1
                print(f"Request error for {host}:{port}: {exc}")
        client.close()
        with lock:
            all_latencies.extend(local_latencies)

    threads = []
    for i in range(thread_count):
        start_idx = i * chunk_size
        end_idx = min(len(operations), start_idx + chunk_size)
        if start_idx >= end_idx:
            break
        thread = threading.Thread(target=worker, args=(operations[start_idx:end_idx],), daemon=True)
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()

    return all_latencies, len(all_latencies)


def summarize_latencies(latencies: List[float]) -> Tuple[float, float, float, float, float, float]:
    if not latencies:
        return 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
    avg = statistics.mean(latencies)
    p95 = percentile(latencies, 95)
    mn = min(latencies)
    mx = max(latencies)
    std = statistics.stdev(latencies) if len(latencies) > 1 else 0.0
    return avg, p95, mn, mx, std, sum(latencies)


def percentile(values: List[float], percent: float) -> float:
    if not values:
        return 0.0
    values_sorted = sorted(values)
    k = (len(values_sorted) - 1) * (percent / 100.0)
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return values_sorted[int(k)]
    d0 = values_sorted[int(f)] * (c - k)
    d1 = values_sorted[int(c)] * (k - f)
    return d0 + d1


def run_benchmark_suite(args: argparse.Namespace) -> List[BenchmarkResult]:
    results: List[BenchmarkResult] = []
    if not MINIREDIS_BIN.exists():
        subprocess.run(["make"], cwd=str(ROOT), check=True)

    dataset_sizes = [10000, 100000]
    if args.include_large and not maybe_skip_combo(1000000, 10240):
        dataset_sizes.append(1000000)

    value_sizes = [32, 1024, 10240]
    workloads = ["write-heavy", "read-heavy", "mixed", "read-after-write", "random-access"]
    thread_counts = [1]
    concurrency_levels = [1, 10, 50, 100]
    total_ops = 100000

    mini_port = find_free_port()
    sqlite_port = find_free_port()
    sqlite_dbfile = tempfile.NamedTemporaryFile(prefix="sqlite_bench_", suffix=".db", delete=False, dir=str(ROOT))
    sqlite_dbfile.close()

    mini_server = start_miniredis(mini_port, max(dataset_sizes) * 2)
    sqlite_server = start_sqlite_server(sqlite_port, sqlite_dbfile.name)

    try:
        for dataset_size in dataset_sizes:
            for value_size in value_sizes:
                if maybe_skip_combo(dataset_size, value_size):
                    print(f"Skipping dataset={dataset_size} value={value_size} because memory estimate is too large")
                    continue
                for workload in workloads:
                    for repeat in range(1, args.repeat + 1):
                        print(f"Running {workload} dataset={dataset_size} value={value_size} repeat={repeat}")
                        operations = generate_workload(workload, dataset_size, total_ops, value_size)
                        if workload != "write-heavy":
                            warmup_ops = operations[: min(DEFAULT_WARMUP_OPS, len(operations))]
                            _ = run_operation_batch("127.0.0.1", mini_port, warmup_ops, 1)
                            _ = run_operation_batch("127.0.0.1", sqlite_port, warmup_ops, 1)
                        print("Preloading Mini Redis...")
                        preload_client = PooledTCPClient("127.0.0.1", mini_port)
                        preload_client.sock.settimeout(60.0)
                        preload_keys(preload_client, min(dataset_size, 100000), value_size)
                        preload_client.close()
                        print("Preloading SQLite...")
                        preload_client = PooledTCPClient("127.0.0.1", sqlite_port)
                        preload_client.sock.settimeout(60.0)
                        preload_keys(preload_client, min(dataset_size, 100000), value_size)
                        preload_client.close()
                        mini_rss = get_rss_kb(mini_server.proc.pid)
                        sqlite_rss = get_rss_kb(sqlite_server.proc.pid)
                        latencies, total = run_operation_batch("127.0.0.1", mini_port, operations, 1)
                        avg, p95, mn, mx, std, total_latency = summarize_latencies(latencies)
                        total_time = total_latency
                        results.append(BenchmarkResult(
                            system="Mini Redis",
                            workload=workload,
                            value_size=value_size,
                            dataset_size=dataset_size,
                            thread_count=1,
                            repeat=repeat,
                            total_ops=total,
                            total_time_s=total_time,
                            throughput_ops_s=total / total_time if total_time > 0 else 0.0,
                            avg_latency_s=avg,
                            p95_latency_s=p95,
                            min_latency_s=mn,
                            max_latency_s=mx,
                            stddev_latency_s=std,
                            server_rss_kb=mini_rss,
                            notes="",
                        ))
                        latencies, total = run_operation_batch("127.0.0.1", sqlite_port, operations, 1)
                        avg, p95, mn, mx, std, total_latency = summarize_latencies(latencies)
                        total_time = total_latency
                        results.append(BenchmarkResult(
                            system="SQLite",
                            workload=workload,
                            value_size=value_size,
                            dataset_size=dataset_size,
                            thread_count=1,
                            repeat=repeat,
                            total_ops=total,
                            total_time_s=total_time,
                            throughput_ops_s=total / total_time if total_time > 0 else 0.0,
                            avg_latency_s=avg,
                            p95_latency_s=p95,
                            min_latency_s=mn,
                            max_latency_s=mx,
                            stddev_latency_s=std,
                            server_rss_kb=sqlite_rss,
                            notes="",
                        ))
                for thread_count in concurrency_levels:
                    for repeat in range(1, args.repeat + 1):
                        workload = "mixed"
                        print(f"Running concurrent mixed dataset={dataset_size} value={value_size} threads={thread_count} repeat={repeat}")
                        operations = generate_workload(workload, dataset_size, total_ops, value_size)
                        if workload != "write-heavy":
                            warmup_ops = operations[: min(DEFAULT_WARMUP_OPS, len(operations))]
                            _ = run_operation_batch("127.0.0.1", mini_port, warmup_ops, thread_count)
                            _ = run_operation_batch("127.0.0.1", sqlite_port, warmup_ops, thread_count)
                        mini_rss = get_rss_kb(mini_server.proc.pid)
                        sqlite_rss = get_rss_kb(sqlite_server.proc.pid)
                        latencies, total = run_operation_batch("127.0.0.1", mini_port, operations, thread_count)
                        avg, p95, mn, mx, std, total_latency = summarize_latencies(latencies)
                        total_time = total_latency
                        results.append(BenchmarkResult(
                            system="Mini Redis",
                            workload="concurrent-mixed",
                            value_size=value_size,
                            dataset_size=dataset_size,
                            thread_count=thread_count,
                            repeat=repeat,
                            total_ops=total,
                            total_time_s=total_time,
                            throughput_ops_s=total / total_time if total_time > 0 else 0.0,
                            avg_latency_s=avg,
                            p95_latency_s=p95,
                            min_latency_s=mn,
                            max_latency_s=mx,
                            stddev_latency_s=std,
                            server_rss_kb=mini_rss,
                            notes="",
                        ))
                        latencies, total = run_operation_batch("127.0.0.1", sqlite_port, operations, thread_count)
                        avg, p95, mn, mx, std, total_latency = summarize_latencies(latencies)
                        total_time = total_latency
                        results.append(BenchmarkResult(
                            system="SQLite",
                            workload="concurrent-mixed",
                            value_size=value_size,
                            dataset_size=dataset_size,
                            thread_count=thread_count,
                            repeat=repeat,
                            total_ops=total,
                            total_time_s=total_time,
                            throughput_ops_s=total / total_time if total_time > 0 else 0.0,
                            avg_latency_s=avg,
                            p95_latency_s=p95,
                            min_latency_s=mn,
                            max_latency_s=mx,
                            stddev_latency_s=std,
                            server_rss_kb=sqlite_rss,
                            notes="",
                        ))
    finally:
        stop_server(mini_server)
        stop_sqlite_server(sqlite_server)
        os.remove(sqlite_dbfile.name)

    save_results(results)
    return results


def save_results(results: List[BenchmarkResult]) -> None:
    with open(RESULTS_CSV, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "system",
            "workload",
            "value_size",
            "dataset_size",
            "thread_count",
            "repeat",
            "total_ops",
            "total_time_s",
            "throughput_ops_s",
            "avg_latency_s",
            "p95_latency_s",
            "min_latency_s",
            "max_latency_s",
            "stddev_latency_s",
            "server_rss_kb",
            "notes",
        ])
        for r in results:
            writer.writerow([
                r.system,
                r.workload,
                r.value_size,
                r.dataset_size,
                r.thread_count,
                r.repeat,
                r.total_ops,
                r.total_time_s,
                r.throughput_ops_s,
                r.avg_latency_s,
                r.p95_latency_s,
                r.min_latency_s,
                r.max_latency_s,
                r.stddev_latency_s,
                r.server_rss_kb,
                r.notes,
            ])
    print(f"Saved benchmark results to {RESULTS_CSV}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run comprehensive Mini Redis vs SQLite benchmark suite.")
    parser.add_argument("--repeat", type=int, default=DEFAULT_REPEAT, help="Number of repetitions per test")
    parser.add_argument("--include-large", action="store_true", help="Include 1M-key dataset tests when memory permits")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    results = run_benchmark_suite(args)
    print(f"Completed {len(results)} benchmark sample runs.")


if __name__ == "__main__":
    main()
