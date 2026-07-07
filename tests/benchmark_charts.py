#!/usr/bin/env python3
import csv
import os
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[0]
CSV_PATH = ROOT / "benchmark_suite_results.csv"
OUT_DIR = ROOT / "benchmark_charts"

WORKLOAD_ORDER = [
    "write-heavy",
    "read-heavy",
    "mixed",
    "read-after-write",
    "random-access",
]

CONCURRENT_WORKLOAD = "concurrent-mixed"

NUMERIC_FIELDS = [
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
]


def load_csv(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            for key in NUMERIC_FIELDS:
                if key in row:
                    row[key] = float(row[key]) if row[key] != "" else 0.0
            row["system"] = row["system"].strip()
            row["workload"] = row["workload"].strip()
            rows.append(row)
    return rows


def aggregate(rows):
    agg = {}
    for row in rows:
        key = (
            row["system"],
            row["workload"],
            int(row["value_size"]),
            int(row["dataset_size"]),
            int(row["thread_count"]),
        )
        if key not in agg:
            agg[key] = {
                "count": 0,
                "throughput_ops_s": 0.0,
                "avg_latency_s": 0.0,
                "p95_latency_s": 0.0,
                "total_time_s": 0.0,
            }
        agg[key]["count"] += 1
        agg[key]["throughput_ops_s"] += row["throughput_ops_s"]
        agg[key]["avg_latency_s"] += row["avg_latency_s"]
        agg[key]["p95_latency_s"] += row["p95_latency_s"]
        agg[key]["total_time_s"] += row["total_time_s"]
    for key, stats in agg.items():
        count = stats["count"]
        for metric in ["throughput_ops_s", "avg_latency_s", "p95_latency_s", "total_time_s"]:
            stats[metric] /= count
    return agg


def ensure_dir(path):
    path.mkdir(parents=True, exist_ok=True)


def plot_workload_throughput(agg, systems, dataset_sizes, value_sizes):
    for dataset_size in dataset_sizes:
        fig, axes = plt.subplots(1, len(value_sizes), figsize=(5 * len(value_sizes), 4), sharey=True)
        if len(value_sizes) == 1:
            axes = [axes]
        for ax, value_size in zip(axes, value_sizes):
            for system in systems:
                xs = []
                ys = []
                for workload in WORKLOAD_ORDER:
                    key = (system, workload, value_size, dataset_size, 1)
                    if key in agg:
                        xs.append(workload)
                        ys.append(agg[key]["throughput_ops_s"])
                if xs:
                    ax.plot(xs, ys, marker="o", label=system)
            ax.set_title(f"dataset={dataset_size} value={value_size}")
            ax.set_xlabel("workload")
            ax.set_xticks(WORKLOAD_ORDER)
            ax.set_xticklabels(WORKLOAD_ORDER, rotation=20)
            ax.grid(True, alpha=0.3)
        axes[0].set_ylabel("throughput ops/s")
        fig.suptitle(f"Throughput vs workload (dataset={dataset_size})")
        fig.legend(loc="upper center", ncol=len(systems), bbox_to_anchor=(0.5, 1.05))
        fig.tight_layout(rect=[0, 0, 1, 0.95])
        out_file = OUT_DIR / f"throughput_workload_dataset_{dataset_size}.png"
        fig.savefig(out_file)
        plt.close(fig)


def plot_latency_workload(agg, systems, dataset_sizes, value_sizes):
    for dataset_size in dataset_sizes:
        fig, axes = plt.subplots(1, len(value_sizes), figsize=(5 * len(value_sizes), 4), sharey=True)
        if len(value_sizes) == 1:
            axes = [axes]
        for ax, value_size in zip(axes, value_sizes):
            for system in systems:
                xs = []
                ys = []
                for workload in WORKLOAD_ORDER:
                    key = (system, workload, value_size, dataset_size, 1)
                    if key in agg:
                        xs.append(workload)
                        ys.append(agg[key]["avg_latency_s"] * 1000.0)
                if xs:
                    ax.plot(xs, ys, marker="o", label=system)
            ax.set_title(f"dataset={dataset_size} value={value_size}")
            ax.set_xlabel("workload")
            ax.set_xticks(WORKLOAD_ORDER)
            ax.set_xticklabels(WORKLOAD_ORDER, rotation=20)
            ax.grid(True, alpha=0.3)
        axes[0].set_ylabel("avg latency (ms)")
        fig.suptitle(f"Average latency vs workload (dataset={dataset_size})")
        fig.legend(loc="upper center", ncol=len(systems), bbox_to_anchor=(0.5, 1.05))
        fig.tight_layout(rect=[0, 0, 1, 0.95])
        out_file = OUT_DIR / f"latency_workload_dataset_{dataset_size}.png"
        fig.savefig(out_file)
        plt.close(fig)


def plot_concurrent_throughput(agg, systems, dataset_sizes, value_sizes, thread_counts):
    for dataset_size in dataset_sizes:
        fig, axes = plt.subplots(1, len(value_sizes), figsize=(5 * len(value_sizes), 4), sharey=True)
        if len(value_sizes) == 1:
            axes = [axes]
        for ax, value_size in zip(axes, value_sizes):
            for system in systems:
                xs = []
                ys = []
                for thread_count in thread_counts:
                    key = (system, CONCURRENT_WORKLOAD, value_size, dataset_size, thread_count)
                    if key in agg:
                        xs.append(thread_count)
                        ys.append(agg[key]["throughput_ops_s"])
                if xs:
                    ax.plot(xs, ys, marker="o", label=system)
            ax.set_title(f"dataset={dataset_size} value={value_size}")
            ax.set_xlabel("thread count")
            ax.set_xscale("log", base=10)
            ax.set_xticks(thread_counts)
            ax.set_xticklabels([str(x) for x in thread_counts])
            ax.grid(True, alpha=0.3)
        axes[0].set_ylabel("throughput ops/s")
        fig.suptitle(f"Concurrent mixed throughput (dataset={dataset_size})")
        fig.legend(loc="upper center", ncol=len(systems), bbox_to_anchor=(0.5, 1.05))
        fig.tight_layout(rect=[0, 0, 1, 0.95])
        out_file = OUT_DIR / f"concurrent_throughput_dataset_{dataset_size}.png"
        fig.savefig(out_file)
        plt.close(fig)


def main():
    rows = load_csv(CSV_PATH)
    agg = aggregate(rows)
    systems = sorted({row["system"] for row in rows})
    dataset_sizes = sorted({int(row["dataset_size"]) for row in rows})
    value_sizes = sorted({int(row["value_size"]) for row in rows})
    thread_counts = sorted({int(row["thread_count"]) for row in rows if row["workload"] == CONCURRENT_WORKLOAD})
    ensure_dir(OUT_DIR)
    plot_workload_throughput(agg, systems, dataset_sizes, value_sizes)
    plot_latency_workload(agg, systems, dataset_sizes, value_sizes)
    plot_concurrent_throughput(agg, systems, dataset_sizes, value_sizes, thread_counts)
    print("Generated charts:")
    for path in sorted(OUT_DIR.iterdir()):
        print("-", path.name)


if __name__ == "__main__":
    main()
