#!/usr/bin/env python3
"""Plot Lab 3 RSA latency and hybrid/AES throughput."""
import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


def grouped(path):
    groups = defaultdict(list)
    with open(path, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            groups[row["case"]].append(row)
    return groups


def mean_ci(values):
    n = len(values)
    mean = statistics.mean(values)
    return mean, (1.96 * statistics.stdev(values) / n ** 0.5 if n > 1 else 0.0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--out-dir", default="graph")
    parser.add_argument("--platform", default="")
    args = parser.parse_args()

    groups = grouped(args.input)
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    # RSA latency by key size and operation.
    fig, ax = plt.subplots(figsize=(7, 4))
    for case in ("rsa_keygen", "rsa_encrypt", "rsa_decrypt"):
        if case not in groups:
            continue
        bits = sorted({int(row["bits"]) for row in groups[case]})
        means = [mean_ci([float(row["elapsed_ms"]) for row in groups[case] if int(row["bits"]) == b])[0] for b in bits]
        cis = [mean_ci([float(row["elapsed_ms"]) for row in groups[case] if int(row["bits"]) == b])[1] for b in bits]
        ax.errorbar(bits, means, yerr=cis, marker="o", capsize=4, label=case)
    ax.set_xlabel("RSA modulus (bits)")
    ax.set_ylabel("per-operation latency (ms)")
    ax.set_title(f"{args.platform} RSA latency (4 threads, 95% CI)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out / "rsa_latency.png", dpi=150)
    plt.close(fig)

    # AES-GCM and hybrid throughput vs input size.
    fig, ax = plt.subplots(figsize=(7, 4))
    for case in ("aes_gcm", "hybrid_encrypt"):
        if case not in groups:
            continue
        sizes = sorted({int(row["size_bytes"]) for row in groups[case]})
        means, cis = [], []
        for size in sizes:
            values = [
                float(row["mbps"]) * int(row["threads"])
                for row in groups[case]
                if int(row["size_bytes"]) == size
            ]
            mean, ci = mean_ci(values)
            means.append(mean)
            cis.append(ci)
        ax.errorbar(sizes, means, yerr=cis, marker="o", capsize=4, label=case)
    ax.set_xscale("log", base=2)
    ax.set_xlabel("payload size (bytes)")
    ax.set_ylabel("aggregate throughput (MB/s)")
    ax.set_title(f"{args.platform} AES-GCM / hybrid throughput (4 threads, 95% CI)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out / "hybrid_throughput.png", dpi=150)
    plt.close(fig)

    # AES-GCM and hybrid latency vs input size.
    fig, ax = plt.subplots(figsize=(7, 4))
    for case in ("aes_gcm", "aes_gcm_decrypt", "hybrid_encrypt", "hybrid_decrypt"):
        if case not in groups:
            continue
        sizes = sorted({int(row["size_bytes"]) for row in groups[case]})
        means, cis = [], []
        for size in sizes:
            values = [
                float(row["elapsed_ms"])
                for row in groups[case]
                if int(row["size_bytes"]) == size
            ]
            mean, ci = mean_ci(values)
            means.append(mean)
            cis.append(ci)
        ax.errorbar(sizes, means, yerr=cis, marker="o", capsize=4, label=case)
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xlabel("payload size (bytes)")
    ax.set_ylabel("per-operation latency (ms, log)")
    ax.set_title(f"{args.platform} AES-GCM / hybrid latency (4 threads, 95% CI)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out / "hybrid_latency.png", dpi=150)
    plt.close(fig)
    print(f"wrote plots to {out}")


if __name__ == "__main__":
    main()
