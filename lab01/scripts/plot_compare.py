#!/usr/bin/env python3
"""Build combined comparison charts from all aestool benchmark CSVs."""
import argparse
import csv
import statistics
import sys
from collections import defaultdict
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is required: python3 -m pip install matplotlib")

MODES = ["ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm"]
SIZES = [1024, 4096, 16384, 262144, 1048576, 8388608]
CLASSES = {
    "block": ["ecb", "cbc"],
    "stream": ["ofb", "cfb", "ctr"],
    "xts": ["xts"],
    "aead": ["gcm", "ccm"],
    "non-aead": ["ecb", "cbc", "ofb", "cfb", "ctr", "xts"],
}
SIZE_LABELS = {
    1024: "1KB",
    4096: "4KB",
    16384: "16KB",
    262144: "256KB",
    1048576: "1MB",
    8388608: "8MB",
}
METRICS = {
    "encrypt": ("throughput_encrypt_MBps", "encrypt_us"),
    "decrypt": ("throughput_decrypt_MBps", "decrypt_us"),
}


def load(directory):
    """Return {(platform, compiler, mode, size): rows} from bench_*.csv."""
    data = {}
    for path in sorted(directory.glob("bench_*.csv")):
        parts = path.stem.split("_")
        if len(parts) < 5 or parts[3] not in MODES:
            continue
        key = (parts[1], parts[2], parts[3], None)
        with open(path, newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
        if not rows:
            continue
        key = (key[0], key[1], key[2], int(rows[0]["size_bytes"]))
        data[key] = rows
    return data


def values(data, mode, size, column, compiler=None):
    rows = [
        row
        for (platform, comp, m, s), group in data.items()
        if m == mode and s == size and (compiler is None or comp == compiler)
        for row in group
    ]
    return [float(row[column]) for row in rows]


def mean(data, mode, size, column, compiler=None):
    vals = values(data, mode, size, column, compiler)
    return statistics.mean(vals) if vals else None


def class_mean(data, modes, size, column):
    vals = [
        float(row[column])
        for (platform, compiler, mode, s), rows in data.items()
        if mode in modes and s == size
        for row in rows
    ]
    return statistics.mean(vals) if vals else None


def mode_list(data, size=None):
    return [m for m in MODES if any(k[2] == m and (size is None or k[3] == size) for k in data)]


def size_list(data):
    return [s for s in SIZES if any(k[3] == s for k in data)]


def plot_by_mode(data, size, out_dir, compilers):
    modes = mode_list(data, size)
    fig, axes = plt.subplots(1, 2, figsize=(15, 6))
    width = 0.8 / max(len(compilers), 1)
    offsets = [(i - (len(compilers) - 1) / 2) * width for i in range(len(compilers))]

    for ax, (kind, (throughput_col, _)) in zip(axes, METRICS.items()):
        for compiler, offset in zip(compilers, offsets):
            heights = [mean(data, mode, size, throughput_col, compiler) or 0 for mode in modes]
            ax.bar(
                [x + offset for x in range(len(modes))],
                heights,
                width,
                label=compiler,
            )
        ax.set_title(f"{kind.capitalize()} throughput at {SIZE_LABELS.get(size, size)}")
        ax.set_ylabel("MB/s")
        ax.set_xticks(range(len(modes)))
        ax.set_xticklabels([m.upper() for m in modes])
        ax.grid(axis="y", alpha=0.3)
        ax.legend(title="compiler")

    fig.suptitle(f"Throughput by mode - {SIZE_LABELS.get(size, size)} payload")
    fig.tight_layout()
    path = out_dir / f"comparison_throughput_by_mode_{SIZE_LABELS.get(size, size)}.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def plot_vs_size(data, out_dir, metric):
    """metric is 'throughput' or 'latency'."""
    modes = mode_list(data)
    sizes = size_list(data)
    fig, axes = plt.subplots(1, 2, figsize=(15, 6))
    colors = plt.get_cmap("tab10")

    for ax, (kind, (throughput_col, latency_col)) in zip(axes, METRICS.items()):
        for index, mode in enumerate(modes):
            if metric == "throughput":
                points = [mean(data, mode, size, throughput_col) for size in sizes]
                ylabel = "MB/s"
            else:
                points = [
                    (mean(data, mode, size, latency_col) or 0) / 1000 for size in sizes
                ]
                ylabel = "milliseconds"
            ax.plot(
                sizes,
                points,
                marker="o",
                linewidth=1.8,
                color=colors(index % 10),
                label=mode.upper(),
            )
        ax.set_xscale("log", base=2)
        ax.set_xticks(sizes)
        ax.set_xticklabels([SIZE_LABELS.get(s, str(s)) for s in sizes])
        if metric == "latency":
            ax.set_yscale("log")
        ax.set_xlabel("payload size")
        ax.set_ylabel(ylabel)
        ax.set_title(f"{kind.capitalize()} {metric}")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(ncol=2)

    fig.suptitle(f"{metric.capitalize()} comparison across payload sizes (mean of GCC/Clang)")
    fig.tight_layout()
    path = out_dir / f"comparison_{metric}_vs_size.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def plot_box(data, size, out_dir):
    modes = mode_list(data, size)
    fig, axes = plt.subplots(1, 2, figsize=(15, 6))
    colors = plt.get_cmap("tab10")

    for ax, (kind, (_, latency_col)) in zip(axes, METRICS.items()):
        samples = [values(data, mode, size, latency_col) for mode in modes]
        boxes = ax.boxplot(samples, showfliers=False, patch_artist=True)
        for index, box in enumerate(boxes["boxes"]):
            box.set_facecolor(colors(index % 10))
            box.set_alpha(0.55)
        ax.set_xticks(range(1, len(modes) + 1))
        ax.set_xticklabels([m.upper() for m in modes])
        ax.set_ylabel("microseconds")
        ax.set_title(f"{kind.capitalize()} latency")
        ax.grid(axis="y", alpha=0.3)

    fig.suptitle(
        f"Latency distribution at {SIZE_LABELS.get(size, size)} payload (GCC + Clang, outliers hidden)"
    )
    fig.tight_layout()
    path = out_dir / f"comparison_latency_box_{SIZE_LABELS.get(size, size)}.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def plot_stream_block(data, out_dir):
    groups = [
        ("ECB (insecure)", ["ecb"]),
        ("CBC", ["cbc"]),
        ("Stream (OFB+CFB+CTR)", CLASSES["stream"]),
        ("XTS (storage)", CLASSES["xts"]),
    ]
    sizes = size_list(data)
    fig, axes = plt.subplots(1, 2, figsize=(15, 6))
    colors = plt.get_cmap("tab10")

    for ax, (kind, (throughput_col, _)) in zip(axes, METRICS.items()):
        for row_index, (name, modes) in enumerate(groups):
            points = [class_mean(data, modes, size, throughput_col) for size in sizes]
            ax.plot(
                sizes,
                points,
                marker="o",
                linewidth=2,
                color=colors(row_index),
                label=name,
            )
        ax.set_xscale("log", base=2)
        ax.set_xticks(sizes)
        ax.set_xticklabels([SIZE_LABELS.get(s, str(s)) for s in sizes])
        ax.set_xlabel("payload size")
        ax.set_ylabel("MB/s")
        ax.set_title(f"{kind.capitalize()} throughput: stream vs block")
        ax.grid(True, alpha=0.3)
        ax.legend()

    fig.suptitle("Stream vs block modes (mean of all runs in each group)")
    fig.tight_layout()
    path = out_dir / "comparison_stream_vs_block.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def plot_aead(data, out_dir):
    groups = [
        ("AEAD (GCM+CCM)", CLASSES["aead"]),
        ("Non-AEAD", CLASSES["non-aead"]),
    ]
    sizes = size_list(data)
    fig, axes = plt.subplots(1, 2, figsize=(15, 6))
    colors = plt.get_cmap("tab10")

    for ax, (kind, (throughput_col, _)) in zip(axes, METRICS.items()):
        for row_index, (name, modes) in enumerate(groups):
            points = [class_mean(data, modes, size, throughput_col) for size in sizes]
            ax.plot(
                sizes,
                points,
                marker="o",
                linewidth=2,
                color=colors(row_index),
                label=name,
            )
        ax.set_xscale("log", base=2)
        ax.set_xticks(sizes)
        ax.set_xticklabels([SIZE_LABELS.get(s, str(s)) for s in sizes])
        ax.set_xlabel("payload size")
        ax.set_ylabel("MB/s")
        ax.set_title(f"{kind.capitalize()} throughput: AEAD vs non-AEAD")
        ax.grid(True, alpha=0.3)
        ax.legend()

    fig.suptitle("AEAD vs non-AEAD (mean of all runs in each class)")
    fig.tight_layout()
    path = out_dir / "comparison_aead_vs_nonaead.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def plot_tag_overhead(data, out_dir):
    sizes = size_list(data)
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))

    for col_index, (kind, (throughput_col, latency_col)) in enumerate(METRICS.items()):
        absolute = axes[0][col_index]
        relative = axes[1][col_index]
        for mode in ("gcm", "ccm"):
            overhead = []
            loss = []
            for size in sizes:
                ctr_us = mean(data, "ctr", size, latency_col)
                mode_us = mean(data, mode, size, latency_col)
                ctr_tp = mean(data, "ctr", size, throughput_col)
                mode_tp = mean(data, mode, size, throughput_col)
                overhead.append(mode_us - ctr_us)
                loss.append(100 * (1 - mode_tp / ctr_tp))
            absolute.plot(
                sizes, overhead, marker="o", linewidth=2, label=mode.upper()
            )
            relative.plot(
                sizes, loss, marker="o", linewidth=2, label=mode.upper()
            )
        for ax in (absolute, relative):
            ax.set_xscale("log", base=2)
            ax.set_xticks(sizes)
            ax.set_xticklabels([SIZE_LABELS.get(s, str(s)) for s in sizes])
            ax.grid(True, alpha=0.3)
            ax.legend()
        absolute.set_ylabel("microseconds vs CTR")
        absolute.set_title(f"{kind.capitalize()} tag overhead (absolute)")
        relative.set_xlabel("payload size")
        relative.set_ylabel("throughput loss (%)")
        relative.set_title(f"{kind.capitalize()} tag overhead (relative)")

    fig.suptitle("AEAD tag overhead compared with CTR")
    fig.tight_layout()
    path = out_dir / "comparison_tag_overhead.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dir",
        default="output/ubuntu/benchmarks",
        help="directory containing bench_*.csv",
    )
    parser.add_argument(
        "--out-dir",
        default=None,
        help="output directory (default: <dir>/graph)",
    )
    parser.add_argument(
        "--bar-size",
        type=int,
        default=1048576,
        help="payload size for the by-mode bar chart",
    )
    parser.add_argument(
        "--box-size",
        type=int,
        default=16384,
        help="payload size for the box plot",
    )
    args = parser.parse_args()

    directory = Path(args.dir)
    data = load(directory)
    if not data:
        sys.exit(f"no benchmark CSVs found in {directory}")

    out_dir = Path(args.out_dir) if args.out_dir else directory / "graph"
    out_dir.mkdir(parents=True, exist_ok=True)

    compilers = sorted({key[1] for key in data})
    plot_by_mode(data, args.bar_size, out_dir, compilers)
    plot_vs_size(data, out_dir, "throughput")
    plot_vs_size(data, out_dir, "latency")
    plot_box(data, args.box_size, out_dir)
    plot_stream_block(data, out_dir)
    plot_aead(data, out_dir)
    plot_tag_overhead(data, out_dir)


if __name__ == "__main__":
    main()
