#!/usr/bin/env python3
"""Plot AES-NI vs table-based AES CTR throughput from two summary CSVs.

Both summaries must contain the same sizes with `enc_mean_MBps` and
`dec_mean_MBps`; the figure uses a log y-axis because the gap is ~70x.
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

ROOT = Path(__file__).resolve().parent.parent / "output" / "ubuntu" / "benchmarks" / "aesni"
SIZE_LABELS = {1048576: "1 MiB", 104857600: "100 MiB", 1073741824: "1 GiB"}


def load(path):
    with open(path, newline="", encoding="utf-8") as handle:
        return {int(row["size_bytes"]): row for row in csv.DictReader(handle)}


def draw(ax, sizes, soft, ni, column, title):
    x = range(len(sizes))
    width = 0.36
    soft_vals = [float(soft[size][column]) for size in sizes]
    ni_vals = [float(ni[size][column]) for size in sizes]
    for offset, vals, label, color in (
        (-width / 2, soft_vals, "table-based AES", "tab:blue"),
        (width / 2, ni_vals, "AES-NI", "tab:green"),
    ):
        rects = ax.bar([value + offset for value in x], vals, width,
                       label=label, color=color)
        for rect, val in zip(rects, vals):
            ax.annotate(f"{val:.0f}", (rect.get_x() + rect.get_width() / 2, val),
                        textcoords="offset points", xytext=(0, 3),
                        ha="center", va="bottom", fontsize=7, rotation=90)
    ax.set_xticks(list(x))
    ax.set_xticklabels([
        f"{SIZE_LABELS[size]}\n{ni_vals[i] / soft_vals[i]:.0f}x"
        for i, size in enumerate(sizes)
    ])
    ax.set_yscale("log")
    ax.set_ylabel("MiB/s (log, mean)")
    ax.set_title(title)
    ax.grid(True, axis="y", alpha=0.3)
    ax.set_ylim(top=max(soft_vals + ni_vals) * 3.5)


def main():
    soft = load(ROOT / "1t" / "soft" / "summary.csv")
    ni = load(ROOT / "1t" / "ni" / "summary.csv")
    sizes = sorted(soft)
    if sizes != sorted(ni):
        sys.exit("size mismatch between soft and AES-NI summaries")

    fig, (left, right) = plt.subplots(1, 2, figsize=(10, 4.6))
    draw(left, sizes, soft, ni, "enc_mean_MBps", "Encrypt")
    draw(right, sizes, soft, ni, "dec_mean_MBps", "Decrypt")
    fig.suptitle("Ubuntu GCC 15.2: AES-NI vs table-based AES-CTR (1 thread)")
    fig.legend(handles=left.get_legend_handles_labels()[0],
               labels=left.get_legend_handles_labels()[1],
               loc="lower center", ncol=2, frameon=False, bbox_to_anchor=(0.5, 0.055))
    fig.text(0.5, 0.015, "nX in tick labels = AES-NI speedup; single thread isolates the cipher",
             ha="center", fontsize=8)
    fig.tight_layout(rect=(0, 0.1, 1, 1))

    out = ROOT / "aesni_vs_software_1t.png"
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
