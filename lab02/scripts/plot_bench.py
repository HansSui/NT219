#!/usr/bin/env python3
"""Plot one compact summary figure from one or more aestool benchmark CSVs."""
import argparse
import csv
import sys
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is required: python3 -m pip install matplotlib")


def mean_ci(values):
    n = len(values)
    if n == 0:
        sys.exit("no rows to plot")
    mean = sum(values) / n
    ci = 0.0
    if n > 1:
        variance = sum((value - mean) ** 2 for value in values) / (n - 1)
        ci = 1.96 * variance ** 0.5 / n ** 0.5
    return mean, ci


def load(path):
    with open(path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        sys.exit(f"no rows in {path}")
    return {
        "size": int(rows[0]["size_bytes"]),
        "mode": rows[0]["mode"],
        "enc_mbps": mean_ci([float(row["throughput_encrypt_MBps"]) for row in rows]),
        "dec_mbps": mean_ci([float(row["throughput_decrypt_MBps"]) for row in rows]),
        "enc_ms": mean_ci([float(row["encrypt_us"]) / 1000 for row in rows]),
        "dec_ms": mean_ci([float(row["decrypt_us"]) / 1000 for row in rows]),
    }


def size_label(size):
    if size >= 1024 ** 3:
        return f"{size // 1024 ** 3} GiB"
    if size >= 1024 ** 2:
        return f"{size // 1024 ** 2} MiB"
    return f"{size // 1024} KiB"


def series(ax, points, key, label, color):
    means = [point[key][0] for point in points]
    cis = [point[key][1] for point in points]
    ax.errorbar([point["size"] for point in points], means, yerr=cis,
                marker="o", capsize=4, label=label, color=color)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_files", nargs="+")
    parser.add_argument("--out-dir", default="graph")
    parser.add_argument("--platform", default="")
    args = parser.parse_args()

    points = sorted((load(path) for path in args.csv_files), key=lambda p: p["size"])
    mode = points[0]["mode"]
    label = f"{args.platform} {mode}".strip()

    fig, (ax_mbps, ax_ms) = plt.subplots(1, 2, figsize=(11, 4.2))
    series(ax_mbps, points, "enc_mbps", "encrypt", "tab:blue")
    series(ax_mbps, points, "dec_mbps", "decrypt", "tab:orange")
    ax_mbps.set_title("Throughput (mean, 95% CI)")
    ax_mbps.set_ylabel("MiB/s")

    series(ax_ms, points, "enc_ms", "encrypt", "tab:blue")
    series(ax_ms, points, "dec_ms", "decrypt", "tab:orange")
    ax_ms.set_title("Latency (mean, 95% CI)")
    ax_ms.set_ylabel("milliseconds (log)")
    ax_ms.set_yscale("log")

    for ax in (ax_mbps, ax_ms):
        ax.set_xscale("log", base=2)
        ax.set_xticks([point["size"] for point in points])
        ax.set_xticklabels([size_label(point["size"]) for point in points])
        ax.set_xlabel("input size")
        ax.grid(True, alpha=0.3)
        ax.legend()

    fig.suptitle(f"{label}: AES-CTR benchmark summary")
    fig.tight_layout()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    platform = (args.platform or "all").replace(" ", "_").replace("-", "_")
    out = out_dir / f"bench_{platform}_{mode}_summary.png"
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
