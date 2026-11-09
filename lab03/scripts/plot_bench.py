#!/usr/bin/env python3
"""Plot Lab 3 RSA latency and AES-GCM/hybrid throughput as grouped bars.

Each group is a key size or payload size; bars are the cases at that group,
with 95% CI whiskers and value labels. This mirrors the Lab 1/Lab 2
grouped-bar comparison style so RSA keygen/encrypt/decrypt and AES vs hybrid
can be read side by side.
"""
import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

CASE_LABELS = {
    "rsa_keygen": "keygen",
    "rsa_encrypt": "RSA encrypt",
    "rsa_decrypt": "RSA decrypt",
    "aes_gcm": "AES-GCM encrypt",
    "aes_gcm_decrypt": "AES-GCM decrypt",
    "hybrid_encrypt": "hybrid encrypt",
    "hybrid_decrypt": "hybrid decrypt",
}
COLORS = ("#2b6cb0", "#e07b39", "#2f855a", "#b83280")


def grouped(path):
    groups = defaultdict(list)
    with open(path, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            groups[row["case"]].append(row)
    return groups


def mean_ci(values):
    n = len(values)
    if n == 0:
        return 0.0, 0.0
    mean = statistics.mean(values)
    ci = 1.96 * statistics.stdev(values) / n ** 0.5 if n > 1 else 0.0
    return mean, ci


def metric(rows, column):
    """Mean and 95% CI; throughput is scaled from per-op to aggregate."""
    if column == "mbps":
        return mean_ci([float(row["mbps"]) * int(row["threads"]) for row in rows])
    return mean_ci([float(row[column]) for row in rows])


def bars(ax, categories, cases, data, field, column, fmt, log=False):
    width = 0.8 / len(cases)
    for index, case in enumerate(cases):
        stats = [
            metric([row for row in data.get(case, []) if int(row[field]) == match], column)
            for match, _ in categories
        ]
        offset = (index - (len(cases) - 1) / 2) * width
        rects = ax.bar(
            [i + offset for i in range(len(categories))],
            [stat[0] for stat in stats],
            width,
            yerr=[stat[1] for stat in stats],
            capsize=3,
            label=CASE_LABELS[case],
            color=COLORS[index],
        )
        for rect, (mean, ci) in zip(rects, stats):
            if mean <= 0:
                continue
            ax.annotate(
                fmt.format(mean),
                (rect.get_x() + rect.get_width() / 2, mean + ci),
                textcoords="offset points",
                xytext=(0, 2),
                ha="center",
                va="bottom",
                fontsize=7,
                rotation=90 if log else 0,
            )
    ax.set_xticks(range(len(categories)))
    ax.set_xticklabels([label for _, label in categories])
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend(fontsize=8)
    ax.margins(y=0.22)


def size_label(value):
    if value >= 1024 ** 2:
        return f"{value // 1024 ** 2} MiB"
    if value >= 1024:
        return f"{value // 1024} KiB"
    return str(value)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--out-dir", default="graph")
    parser.add_argument("--platform", default="")
    args = parser.parse_args()

    data = grouped(args.input)
    if not data:
        raise SystemExit(f"no rows in {args.input}")
    threads = max(int(row["threads"]) for rows in data.values() for row in rows)
    platform = args.platform.strip()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    rsa_cases = [case for case in ("rsa_keygen", "rsa_encrypt", "rsa_decrypt") if case in data]
    if rsa_cases:
        bits = sorted({int(row["bits"]) for case in rsa_cases for row in data[case]})
        fig, ax = plt.subplots(figsize=(8, 4.8))
        bars(ax, [(b, f"{b}-bit") for b in bits], rsa_cases, data, "bits",
             "elapsed_ms", "{:.4g}", log=True)
        ax.set_yscale("log")
        ax.set_ylabel("per-operation latency (ms, log scale)")
        ax.set_title(f"{platform} RSA latency by key size ({threads} threads, 95% CI)")
        fig.tight_layout()
        fig.savefig(out / "rsa_latency.png", dpi=150)
        plt.close(fig)

    aes_cases = [case for case in ("aes_gcm", "aes_gcm_decrypt", "hybrid_encrypt", "hybrid_decrypt")
                 if case in data]
    if aes_cases:
        sizes = sorted({int(row["size_bytes"]) for case in aes_cases for row in data[case]})
        categories = [(size, size_label(size)) for size in sizes]

        fig, ax = plt.subplots(figsize=(8, 4.8))
        bars(ax, categories, aes_cases, data, "size_bytes", "mbps", "{:,.0f}")
        ax.set_ylabel("aggregate throughput (MB/s)")
        ax.set_title(f"{platform} AES-GCM vs hybrid throughput ({threads} threads, 95% CI)")
        fig.tight_layout()
        fig.savefig(out / "hybrid_throughput.png", dpi=150)
        plt.close(fig)

        fig, ax = plt.subplots(figsize=(8, 4.8))
        bars(ax, categories, aes_cases, data, "size_bytes", "elapsed_ms", "{:.4g}", log=True)
        ax.set_yscale("log")
        ax.set_ylabel("per-operation latency (ms, log scale)")
        ax.set_title(f"{platform} AES-GCM vs hybrid latency ({threads} threads, 95% CI)")
        fig.tight_layout()
        fig.savefig(out / "hybrid_latency.png", dpi=150)
        plt.close(fig)

    print(f"wrote plots to {out}")


if __name__ == "__main__":
    main()
