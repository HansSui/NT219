#!/usr/bin/env python3
"""Compare aestool benchmarks from two platforms (Windows vs Linux).

Reads bench_*.csv from two directories, aggregates all runs per mode/size and
writes comparison charts plus throughput/latency heatmaps:

  cross_throughput_by_mode_1MB.png
  cross_latency_by_mode_16KB.png
  cross_throughput_vs_size.png
  cross_latency_vs_size.png
  heatmap_throughput_<platform>.png        (one per platform, enc + dec)
  heatmap_ratio_windows_vs_ubuntu.png      (throughput ratio, enc + dec)
  heatmap_latency_ratio_windows_vs_ubuntu.png
  cross_platform_summary.csv

Example:
  python scripts/plot_crossplatform.py \
      --linux-dir output/ubuntu/benchmarks \
      --windows-dir output/windows/benchmarks \
      --out-dir output/compare/graph
"""
import argparse
import csv
import statistics
import sys
from pathlib import Path

import numpy as np

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import TwoSlopeNorm
from matplotlib.lines import Line2D
from matplotlib.patches import Patch

MODES = ["ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm"]
SIZES = [1024, 4096, 16384, 262144, 1048576, 8388608]
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

LINUX_COLOR = "#e07b39"
WINDOWS_COLOR = "#2b6cb0"
NOTE = (
    "Ubuntu runs: GCC + Clang pooled. Windows runs: MSVC 2026 (x64). "
    "Hosts and CPUs differ, so ratios are indicative rather than controlled."
)


def load_dir(directory):
    """Return {(mode, size): rows} pooling every compiler found in a directory."""
    data = {}
    for path in sorted(directory.glob("bench_*.csv")):
        parts = path.stem.split("_")
        if len(parts) < 5 or parts[3] not in MODES:
            continue
        with open(path, newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
        if not rows:
            continue
        key = (parts[3], int(rows[0]["size_bytes"]))
        data.setdefault(key, []).extend(rows)
    return data


def mean_of(rows, column):
    vals = [float(row[column]) for row in rows]
    return statistics.mean(vals) if vals else None


def grid(dataset, column):
    matrix = np.full((len(MODES), len(SIZES)), np.nan)
    for (mode, size), rows in dataset.items():
        if mode in MODES and size in SIZES:
            value = mean_of(rows, column)
            if value is not None:
                matrix[MODES.index(mode), SIZES.index(size)] = value
    return matrix


def find_sizes(dataset):
    present = {size for (_, size) in dataset}
    return [size for size in SIZES if size in present]


def annotate_bars(ax, bars, fmt, log=False):
    for bar in bars:
        height = bar.get_height()
        if not np.isfinite(height) or height <= 0:
            continue
        ax.annotate(
            fmt.format(height),
            (bar.get_x() + bar.get_width() / 2, height),
            textcoords="offset points",
            xytext=(0, 3 if not log else 2),
            ha="center",
            va="bottom",
            fontsize=7,
            rotation=90 if log else 0,
        )


def plot_by_mode(platforms, size, out_dir):
    fig, axes = plt.subplots(1, 2, figsize=(15, 6.2))
    width = 0.8 / len(platforms)

    for ax, (kind, (throughput_col, _)) in zip(axes, METRICS.items()):
        for index, platform in enumerate(platforms):
            heights = [
                mean_of(platform["data"].get((mode, size), []), throughput_col) or 0
                for mode in MODES
            ]
            offset = (index - (len(platforms) - 1) / 2) * width
            bars = ax.bar(
                [x + offset for x in range(len(MODES))],
                heights,
                width,
                label=platform["label"],
                color=platform["color"],
            )
            annotate_bars(ax, bars, "{:.0f}")
        ax.set_title(f"{kind.capitalize()} throughput at {SIZE_LABELS.get(size, size)}")
        ax.set_ylabel("MB/s")
        ax.set_xticks(range(len(MODES)))
        ax.set_xticklabels([m.upper() for m in MODES])
        ax.grid(axis="y", alpha=0.3)
        ax.legend(title="platform")
        ax.margins(y=0.18)

    fig.suptitle(
        f"Windows vs Ubuntu - throughput by mode at {SIZE_LABELS.get(size, size)} payload"
    )
    fig.text(0.5, 0.005, NOTE, ha="center", fontsize=8, color="#666")
    fig.tight_layout(rect=(0, 0.035, 1, 0.94))
    path = out_dir / f"cross_throughput_by_mode_{SIZE_LABELS.get(size, size)}.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def plot_latency_by_mode(platforms, size, out_dir):
    fig, axes = plt.subplots(1, 2, figsize=(15, 6.2))
    width = 0.8 / len(platforms)

    for ax, (kind, (_, latency_col)) in zip(axes, METRICS.items()):
        for index, platform in enumerate(platforms):
            heights = [
                mean_of(platform["data"].get((mode, size), []), latency_col) or np.nan
                for mode in MODES
            ]
            offset = (index - (len(platforms) - 1) / 2) * width
            bars = ax.bar(
                [x + offset for x in range(len(MODES))],
                heights,
                width,
                label=platform["label"],
                color=platform["color"],
            )
            annotate_bars(ax, bars, "{:.1f}", log=True)
        ax.set_yscale("log")
        ax.set_title(
            f"{kind.capitalize()} latency at {SIZE_LABELS.get(size, size)} (log scale)"
        )
        ax.set_ylabel("microseconds")
        ax.set_xticks(range(len(MODES)))
        ax.set_xticklabels([m.upper() for m in MODES])
        ax.grid(axis="y", which="both", alpha=0.3)
        ax.legend(title="platform")
        ax.margins(y=0.25)

    fig.suptitle(
        f"Windows vs Ubuntu - latency by mode at {SIZE_LABELS.get(size, size)} payload"
    )
    fig.text(0.5, 0.005, NOTE, ha="center", fontsize=8, color="#666")
    fig.tight_layout(rect=(0, 0.035, 1, 0.94))
    path = out_dir / f"cross_latency_by_mode_{SIZE_LABELS.get(size, size)}.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def plot_vs_size(platforms, metric, out_dir):
    kind_throughput = metric == "throughput"
    sizes = sorted({size for p in platforms for (_, size) in p["data"]})
    sizes = [s for s in SIZES if s in sizes]
    colors = plt.get_cmap("tab10")

    fig, axes = plt.subplots(1, 2, figsize=(16, 6.8))
    for ax, (kind, (throughput_col, latency_col)) in zip(axes, METRICS.items()):
        for index, mode in enumerate(MODES):
            for platform in platforms:
                points = []
                for size in sizes:
                    rows = platform["data"].get((mode, size), [])
                    value = mean_of(rows, throughput_col if kind_throughput else latency_col)
                    points.append(value)
                if all(v is None for v in points):
                    continue
                if not kind_throughput:
                    points = [v / 1000 if v else np.nan for v in points]
                ax.plot(
                    sizes,
                    points,
                    marker="o",
                    markersize=4,
                    linewidth=1.6,
                    color=colors(index % 10),
                    linestyle="-" if platform["key"] == "linux" else "--",
                )
        ax.set_xscale("log", base=2)
        ax.set_xticks(sizes)
        ax.set_xticklabels([SIZE_LABELS.get(s, str(s)) for s in sizes])
        if not kind_throughput:
            ax.set_yscale("log")
        ax.set_xlabel("payload size")
        ax.set_ylabel("MB/s" if kind_throughput else "milliseconds")
        ax.set_title(f"{kind.capitalize()} {metric} vs payload size")
        ax.grid(True, which="both", alpha=0.3)

    mode_handles = [
        Line2D([], [], color=colors(i % 10), linewidth=2, marker="o", label=mode.upper())
        for i, mode in enumerate(MODES)
    ]
    platform_handles = [
        Line2D([], [], color="#333", linewidth=2, linestyle=style, label=platform["label"])
        for platform, style in zip(platforms, ["-", "--"])
    ]
    fig.legend(
        handles=mode_handles,
        loc="center left",
        bbox_to_anchor=(0.905, 0.45),
        title="mode",
        fontsize=8,
        title_fontsize=9,
    )
    fig.legend(
        handles=platform_handles,
        loc="upper center",
        bbox_to_anchor=(0.45, 0.925),
        ncol=2,
        title="platform (line style)",
        fontsize=8,
        title_fontsize=9,
    )
    fig.suptitle(f"Windows (dashed) vs Ubuntu (solid) - {metric} across payload sizes")
    fig.text(0.5, 0.005, NOTE, ha="center", fontsize=8, color="#666")
    fig.tight_layout(rect=(0, 0.035, 0.9, 0.87))
    path = out_dir / f"cross_{metric}_vs_size.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def _cell_text_color(cmap, norm, value):
    red, green, blue, _ = cmap(norm(value))
    luminance = 0.299 * red + 0.587 * green + 0.114 * blue
    return "white" if luminance < 0.55 else "#111"


def draw_heatmap(ax, matrix, title, cmap, fmt, norm=None, cbar_label="", fontsize=8):
    masked = np.ma.masked_invalid(matrix)
    image = ax.imshow(masked, cmap=cmap, norm=norm, aspect="auto")
    ax.set_title(title)
    ax.set_xticks(range(len(SIZES)))
    ax.set_xticklabels([SIZE_LABELS[s] for s in SIZES])
    ax.set_yticks(range(len(MODES)))
    ax.set_yticklabels([m.upper() for m in MODES])
    ax.set_xticks(np.arange(-0.5, len(SIZES), 1), minor=True)
    ax.set_yticks(np.arange(-0.5, len(MODES), 1), minor=True)
    ax.grid(which="minor", color="white", linewidth=0.8)
    ax.tick_params(which="minor", length=0)

    for row in range(len(MODES)):
        for col in range(len(SIZES)):
            value = matrix[row, col]
            if not np.isfinite(value):
                continue
            ax.text(
                col,
                row,
                fmt.format(value),
                ha="center",
                va="center",
                fontsize=fontsize,
                color=_cell_text_color(image.cmap, image.norm, value)
                if norm is None
                else "black",
                fontweight="bold" if norm is not None else "normal",
            )
    return image


def plot_platform_heatmaps(dataset, label, out_dir):
    fig, axes = plt.subplots(1, 2, figsize=(14, 6.4))
    for ax, (kind, (throughput_col, _)) in zip(axes, METRICS.items()):
        matrix = grid(dataset, throughput_col)
        image = draw_heatmap(
            ax,
            matrix,
            f"{label} - {kind} throughput (MB/s)",
            plt.get_cmap("viridis"),
            "{:,.0f}",
        )
        fig.colorbar(image, ax=ax, label="MB/s", fraction=0.046, pad=0.03)
    fig.suptitle(f"Throughput heatmap - {label}")
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    path = out_dir / f"heatmap_throughput_{label.split()[0].lower()}.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def plot_ratio_heatmap(numerator, denominator, columns, title, out_dir, name, fmt="{:.2f}x"):
    fig, axes = plt.subplots(1, 2, figsize=(14, 6.4))
    for ax, (kind, (throughput_col, latency_col)) in zip(axes, METRICS.items()):
        column = columns[kind]
        num = grid(numerator, column)
        den = grid(denominator, column)
        with np.errstate(divide="ignore", invalid="ignore"):
            ratio = np.where((den != 0) & np.isfinite(den), num / den, np.nan)
        finite = ratio[np.isfinite(ratio)]
        low = float(finite.min()) if finite.size else 0.5
        high = float(finite.max()) if finite.size else 1.5
        vmin = min(0.9, low)
        vmax = max(1.1, high)
        norm = TwoSlopeNorm(vcenter=1.0, vmin=vmin, vmax=vmax)
        cmap = plt.get_cmap("RdYlGn_r") if name == "latency" else plt.get_cmap("RdYlGn")
        image = draw_heatmap(
            ax,
            ratio,
            f"{kind.capitalize()} {title}",
            cmap,
            fmt,
            norm=norm,
        )
        fig.colorbar(image, ax=ax, label="Windows / Ubuntu", fraction=0.046, pad=0.03)
    if name == "throughput":
        legend_note = "green = Windows faster, red = Ubuntu faster"
    else:
        legend_note = "green = Windows faster, red = Windows slower (ratio > 1 = longer latency)"
    fig.suptitle(f"Windows vs Ubuntu - {title} ({legend_note}, 1.00 = equal)")
    fig.text(0.5, 0.005, NOTE, ha="center", fontsize=8, color="#666")
    fig.tight_layout(rect=(0, 0.035, 1, 0.94))
    path = out_dir / f"heatmap_{name}_ratio_windows_vs_ubuntu.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"wrote {path}")


def write_csv(platforms, out_path):
    sizes = sorted({size for p in platforms for (_, size) in p["data"]})
    header = ["mode", "size_bytes", "size"]
    for platform in platforms:
        tag = platform["key"]
        header += [
            f"{tag}_enc_MBps",
            f"{tag}_dec_MBps",
            f"{tag}_enc_us",
            f"{tag}_dec_us",
        ]
    header += ["enc_ratio_win_over_linux", "dec_ratio_win_over_linux"]

    with open(out_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(header)
        for mode in MODES:
            for size in sizes:
                row = [mode.upper(), size, SIZE_LABELS.get(size, str(size))]
                values = {}
                for platform in platforms:
                    rows = platform["data"].get((mode, size), [])
                    enc_tp = mean_of(rows, "throughput_encrypt_MBps")
                    dec_tp = mean_of(rows, "throughput_decrypt_MBps")
                    enc_us = mean_of(rows, "encrypt_us")
                    dec_us = mean_of(rows, "decrypt_us")
                    values[platform["key"]] = (enc_tp, dec_tp)
                    row += [
                        "" if enc_tp is None else f"{enc_tp:.3f}",
                        "" if dec_tp is None else f"{dec_tp:.3f}",
                        "" if enc_us is None else f"{enc_us:.3f}",
                        "" if dec_us is None else f"{dec_us:.3f}",
                    ]
                win = values.get("win", (None, None))
                lin = values.get("linux", (None, None))
                for win_value, lin_value in zip(win, lin):
                    if win_value and lin_value:
                        row.append(f"{win_value / lin_value:.4f}")
                    else:
                        row.append("")
                writer.writerow(row)
    print(f"wrote {out_path}")


def default_windows_dir(root):
    return root / "output/windows/benchmarks"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--linux-dir", default="output/ubuntu/benchmarks")
    parser.add_argument(
        "--windows-dir",
        default=None,
        help="default: output/windows/benchmarks",
    )
    parser.add_argument("--linux-label", default="Ubuntu (GCC+Clang)")
    parser.add_argument("--windows-label", default="Windows (MSVC 2026)")
    parser.add_argument("--out-dir", default="output/compare/graph")
    parser.add_argument("--bar-size", type=int, default=1048576)
    parser.add_argument("--latency-size", type=int, default=16384)
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent

    def resolve(path):
        candidate = Path(path)
        return candidate if candidate.is_absolute() else root / candidate

    windows_dir = resolve(args.windows_dir) if args.windows_dir else default_windows_dir(root)
    linux_dir = resolve(args.linux_dir)

    for name, directory in (("windows", windows_dir), ("linux", linux_dir)):
        if not directory.is_dir() or not any(directory.glob("bench_*.csv")):
            sys.exit(f"no benchmark CSVs found for {name}: {directory}")

    linux_data = load_dir(linux_dir)
    windows_data = load_dir(windows_dir)
    if not linux_data or not windows_data:
        sys.exit("one of the benchmark sets is empty")

    platforms = [
        {"key": "linux", "label": args.linux_label, "color": LINUX_COLOR, "data": linux_data},
        {"key": "win", "label": args.windows_label, "color": WINDOWS_COLOR, "data": windows_data},
    ]

    out_dir = resolve(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    plot_by_mode(platforms, args.bar_size, out_dir)
    plot_latency_by_mode(platforms, args.latency_size, out_dir)
    plot_vs_size(platforms, "throughput", out_dir)
    plot_vs_size(platforms, "latency", out_dir)

    plot_platform_heatmaps(linux_data, args.linux_label, out_dir)
    plot_platform_heatmaps(windows_data, args.windows_label, out_dir)

    # Windows / Ubuntu. Throughput > 1 means Windows is faster; latency > 1 means slower.
    plot_ratio_heatmap(
        windows_data,
        linux_data,
        {"encrypt": "throughput_encrypt_MBps", "decrypt": "throughput_decrypt_MBps"},
        "throughput ratio",
        out_dir,
        "throughput",
    )
    plot_ratio_heatmap(
        windows_data,
        linux_data,
        {"encrypt": "encrypt_us", "decrypt": "decrypt_us"},
        "latency ratio",
        out_dir,
        "latency",
    )

    write_csv(platforms, out_dir.parent / "cross_platform_summary.csv")


if __name__ == "__main__":
    main()
