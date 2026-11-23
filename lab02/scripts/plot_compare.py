#!/usr/bin/env python3
"""Plot own AES-CTR vs OpenSSL median throughput per platform.

Reads `summary_robust.csv` in each platform's `openssl/` folder and at the
benchmark root, then writes one log-scale comparison figure per platform.
Medians are used because the Windows OpenSSL means are flagged noisy.
"""
import csv
import sys
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is required: python3 -m pip install matplotlib")

ROOT = Path(__file__).resolve().parent.parent / "output"
SIZE_LABELS = {1048576: "1 MiB", 104857600: "100 MiB", 1073741824: "1 GiB"}


def load(path):
    with open(path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    return {int(row["size_bytes"]): row for row in rows}


def draw(ax, sizes, own, openssl, column, title):
    x = range(len(sizes))
    width = 0.36
    own_vals = [float(own[size][column]) for size in sizes]
    ssl_vals = [float(openssl[size][column]) for size in sizes]
    for offset, vals, label, color in (
        (-width / 2, own_vals, "own (table-based)", "tab:blue"),
        (width / 2, ssl_vals, "OpenSSL (AES-NI)", "tab:red"),
    ):
        rects = ax.bar([value + offset for value in x], vals, width,
                       label=label, color=color)
        for rect, val in zip(rects, vals):
            ax.annotate(f"{val:.0f}", (rect.get_x() + rect.get_width() / 2, val),
                        textcoords="offset points", xytext=(0, 3),
                        ha="center", va="bottom", fontsize=7, rotation=90)
    ax.set_xticks(list(x))
    ax.set_xticklabels([
        f"{SIZE_LABELS[size]}\n{ssl_vals[index] / own_vals[index]:.0f}x"
        for index, size in enumerate(sizes)
    ])
    ax.set_yscale("log")
    ax.set_ylabel("MiB/s (log, median)")
    ax.set_title(title)
    ax.grid(True, axis="y", alpha=0.3)
    ax.set_ylim(top=max(own_vals + ssl_vals) * 3.5)


def main():
    for platform, label in (("ubuntu", "Ubuntu GCC 15.2"), ("windows", "Windows MSVC 19.51")):
        base = ROOT / platform / "benchmarks"
        own = load(base / "summary_robust.csv")
        openssl = load(base / "openssl" / "summary_robust.csv")
        sizes = sorted(own)
        if sizes != sorted(openssl):
            sys.exit(f"{platform}: size mismatch between own and OpenSSL summaries")

        fig, (left, right) = plt.subplots(1, 2, figsize=(10, 4.6))
        draw(left, sizes, own, openssl, "enc_median_MBps", "Encrypt")
        draw(right, sizes, own, openssl, "dec_median_MBps", "Decrypt")
        fig.suptitle(f"{label}: own AES-CTR vs OpenSSL (median throughput, 4 threads)")
        fig.legend(handles=left.get_legend_handles_labels()[0],
                   labels=left.get_legend_handles_labels()[1],
                   loc="lower center", ncol=2, frameon=False, bbox_to_anchor=(0.5, 0.055))
        fig.text(0.5, 0.015,
                 "nX in tick labels = OpenSSL speedup; gap is expected (AES-NI vs table-based AES)",
                 ha="center", fontsize=8)
        fig.tight_layout(rect=(0, 0.1, 1, 1))
        out = base / "graphs" / f"compare_openssl_vs_own_{platform}.png"
        out.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(out, dpi=150)
        plt.close(fig)
        print(f"wrote {out}")


if __name__ == "__main__":
    main()
