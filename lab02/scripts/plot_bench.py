#!/usr/bin/env python3
"""Plot one compact summary figure from one or more aestool benchmark CSVs.

Three grouped bars per size: encrypt, decrypt and total. Total throughput is
2 * size / (encrypt_time + decrypt_time); total latency is the sum of both
times, so the total is the aggregate cost of processing the payload both ways.
"""
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
    size = int(rows[0]["size_bytes"])
    return {
        "size": size,
        "mode": rows[0]["mode"],
        "enc_mbps": mean_ci([float(row["throughput_encrypt_MBps"]) for row in rows]),
        "dec_mbps": mean_ci([float(row["throughput_decrypt_MBps"]) for row in rows]),
        # Total: both directions over their combined time, per run.
        "total_mbps": mean_ci([
            2.0 * size / (float(row["encrypt_us"]) + float(row["decrypt_us"]))
            * 1e6 / (1024 ** 2)
            for row in rows
        ]),
        "enc_ms": mean_ci([float(row["encrypt_us"]) / 1000 for row in rows]),
        "dec_ms": mean_ci([float(row["decrypt_us"]) / 1000 for row in rows]),
        "total_ms": mean_ci([
            (float(row["encrypt_us"]) + float(row["decrypt_us"])) / 1000
            for row in rows
        ]),
    }


def size_label(size):
    if size >= 1024 ** 3:
        return f"{size // 1024 ** 3} GiB"
    if size >= 1024 ** 2:
        return f"{size // 1024 ** 2} MiB"
    return f"{size // 1024} KiB"


def bars(ax, points, keys, labels, colors, fmt, log=False):
    x = list(range(len(points)))
    width = 0.8 / len(keys)
    for index, (key, label, color) in enumerate(zip(keys, labels, colors)):
        offset = (index - (len(keys) - 1) / 2) * width
        rects = ax.bar([value + offset for value in x],
                       [point[key][0] for point in points], width,
                       yerr=[point[key][1] for point in points], capsize=3,
                       label=label, color=color)
        for rect, point in zip(rects, points):
            height = point[key][0]
            if height <= 0:
                continue
            ax.annotate(fmt.format(height),
                        (rect.get_x() + rect.get_width() / 2,
                         height + point[key][1]),
                        textcoords="offset points", xytext=(0, 2 if log else 3),
                        ha="center", va="bottom", fontsize=7,
                        rotation=90 if log else 0)
    ax.set_xticks(x)
    ax.set_xticklabels([size_label(point["size"]) for point in points])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_files", nargs="+")
    parser.add_argument("--out-dir", default="graph")
    parser.add_argument("--platform", default="")
    args = parser.parse_args()

    points = sorted((load(path) for path in args.csv_files), key=lambda p: p["size"])
    mode = points[0]["mode"]
    label = f"{args.platform} {mode}".strip()

    labels = ("encrypt", "decrypt", "total")
    colors = ("tab:blue", "tab:orange", "tab:green")

    fig, (ax_mbps, ax_ms) = plt.subplots(1, 2, figsize=(11, 4.2))
    bars(ax_mbps, points, ("enc_mbps", "dec_mbps", "total_mbps"),
         labels, colors, "{:.1f}")
    ax_mbps.set_title("Throughput (mean, 95% CI)")
    ax_mbps.set_ylabel("MiB/s")

    bars(ax_ms, points, ("enc_ms", "dec_ms", "total_ms"), labels, colors,
         "{:.1f}", log=True)
    ax_ms.set_title("Latency (mean, 95% CI)")
    ax_ms.set_ylabel("milliseconds (log)")
    ax_ms.set_yscale("log")

    for ax in (ax_mbps, ax_ms):
        ax.set_xlabel("input size")
        ax.grid(True, axis="y", alpha=0.3)
        ax.legend()

    fig.suptitle(f"{label}: AES-CTR benchmark summary")
    fig.text(0.5, 0.01,
             "total = encrypt + decrypt (throughput: 2*size / total time)",
             ha="center", fontsize=8)
    fig.tight_layout(rect=(0, 0.03, 1, 1))

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    platform = (args.platform or "all").replace(" ", "_").replace("-", "_")
    out = out_dir / f"bench_{platform}_{mode}_summary.png"
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
