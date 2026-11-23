#!/usr/bin/env python3
"""Plot Lab 4 hash throughput and latency from the summary CSV."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

COLORS = ("#2b6cb0", "#e07b39", "#2f855a", "#b83280")


def load(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            rows.append(row)
    return rows


def plot_metric(rows, io, metric, ci, ylabel, title, out):
    groups = defaultdict(lambda: defaultdict(list))
    sizes = set()
    algos = []
    for row in rows:
        if row["io"] != io:
            continue
        size = int(row["size_mib"])
        sizes.add(size)
        if row["algo"] not in algos:
            algos.append(row["algo"])
        groups[size][row["algo"]].append((float(row[metric]), float(row[ci])))
    if not sizes:
        return
    sizes = sorted(sizes)
    width = 0.8 / max(1, len(algos))
    fig, ax = plt.subplots(figsize=(8, 4.5))
    for index, algo in enumerate(algos):
        values = []
        errors = []
        for size in sizes:
            points = groups[size].get(algo, [])
            values.append(sum(p[0] for p in points) / len(points) if points else 0)
            errors.append(points[0][1] if points else 0)
        positions = [i + (index - (len(algos) - 1) / 2) * width for i in range(len(sizes))]
        ax.bar(positions, values, width, label=algo, color=COLORS[index % len(COLORS)],
               yerr=errors, capsize=3)
    ax.set_xticks(range(len(sizes)))
    ax.set_xticklabels([f"{size} MiB" for size in sizes])
    ax.set_ylabel(ylabel)
    ax.set_title(f"{title} ({io})")
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--platform", default="Ubuntu")
    args = parser.parse_args()
    rows = load(args.input)
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for io in ("stream", "mmap"):
        plot_metric(rows, io, "mbps_mean", "mbps_ci95", "Throughput (MB/s)",
                    f"{args.platform} SHA throughput", out / f"throughput_{io}.png")
        plot_metric(rows, io, "elapsed_mean_ms", "elapsed_ci95_ms", "Latency (ms)",
                    f"{args.platform} SHA latency", out / f"latency_{io}.png")
    print(f"wrote plots to {out}")


if __name__ == "__main__":
    main()
