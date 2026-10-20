#!/usr/bin/env python3
"""Plot one aestool benchmark CSV file (throughput and latency)."""
import argparse
import csv
import sys
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is required: python3 -m pip install matplotlib")


def column(rows, name):
    return [float(row[name]) for row in rows]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_file")
    parser.add_argument("--out-dir", default="graph")
    parser.add_argument("--platform", default="")
    args = parser.parse_args()

    with open(args.csv_file, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        sys.exit(f"no rows in {args.csv_file}")

    mode = rows[0]["mode"]
    size = int(rows[0]["size_bytes"])
    runs = [int(row["run"]) for row in rows]
    label = f"{args.platform} {mode} {size} B".strip()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = Path(args.csv_file).stem

    fig, ax = plt.subplots()
    ax.plot(runs, column(rows, "throughput_encrypt_MBps"), label="encrypt")
    ax.plot(runs, column(rows, "throughput_decrypt_MBps"), label="decrypt")
    ax.set_title(f"Throughput - {label}")
    ax.set_xlabel("run")
    ax.set_ylabel("MB/s")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / f"{stem}_throughput.png", dpi=150)
    plt.close(fig)

    fig, ax = plt.subplots()
    ax.plot(runs, column(rows, "encrypt_us"), label="encrypt")
    ax.plot(runs, column(rows, "decrypt_us"), label="decrypt")
    ax.set_title(f"Latency - {label}")
    ax.set_xlabel("run")
    ax.set_ylabel("microseconds")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / f"{stem}_latency.png", dpi=150)
    plt.close(fig)

    print(f"wrote {out_dir / (stem + '_throughput.png')}")
    print(f"wrote {out_dir / (stem + '_latency.png')}")


if __name__ == "__main__":
    main()
