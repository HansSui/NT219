#!/usr/bin/env python3
"""Summarize aestool benchmark CSVs into report tables."""
import argparse
import csv
import math
import statistics
import sys
from pathlib import Path

MODES = ["ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm"]
SIZE_LABELS = {
    1024: "1KB",
    4096: "4KB",
    16384: "16KB",
    262144: "256KB",
    1048576: "1MB",
    8388608: "8MB",
}
CLASSES = {
    "block": ["ecb", "cbc"],
    "stream": ["ofb", "cfb", "ctr"],
    "xts": ["xts"],
    "aead": ["gcm", "ccm"],
    "non-aead": ["ecb", "cbc", "ofb", "cfb", "ctr", "xts"],
}


def load(directory):
    data = {}
    for path in sorted(directory.glob("bench_*.csv")):
        parts = path.stem.split("_")
        if len(parts) < 5 or parts[3] not in MODES:
            continue
        with open(path, newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
        if not rows:
            continue
        data[(parts[1], parts[2], parts[3], int(rows[0]["size_bytes"]))] = rows
    return data


def stats(rows, column):
    vals = [float(row[column]) for row in rows]
    n = len(vals)
    mean = statistics.mean(vals)
    sd = statistics.stdev(vals) if n > 1 else 0.0
    return {
        "n": n,
        "mean": mean,
        "median": statistics.median(vals),
        "sd": sd,
        "ci95": 1.96 * sd / math.sqrt(n) if n else 0.0,
        "cv": sd / mean if mean else 0.0,
    }


def write_summary(data, path):
    columns = ["n", "mean", "median", "sd", "ci95", "cv"]
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "platform",
                "compiler",
                "mode",
                "size_bytes",
                "size",
                "runs",
                "enc_mean_us",
                "enc_median_us",
                "enc_sd_us",
                "enc_ci95_us",
                "enc_cv",
                "dec_mean_us",
                "dec_median_us",
                "dec_sd_us",
                "dec_ci95_us",
                "dec_cv",
                "enc_mean_MBps",
                "dec_mean_MBps",
            ]
        )
        for key in sorted(data, key=lambda k: (k[0], k[1], MODES.index(k[2]), k[3])):
            platform, compiler, mode, size = key
            rows = data[key]
            enc = stats(rows, "encrypt_us")
            dec = stats(rows, "decrypt_us")
            enc_tp = stats(rows, "throughput_encrypt_MBps")
            dec_tp = stats(rows, "throughput_decrypt_MBps")
            writer.writerow(
                [
                    platform,
                    compiler,
                    mode.upper(),
                    size,
                    SIZE_LABELS.get(size, str(size)),
                    enc["n"],
                    f"{enc['mean']:.3f}",
                    f"{enc['median']:.3f}",
                    f"{enc['sd']:.3f}",
                    f"{enc['ci95']:.3f}",
                    f"{enc['cv']:.4f}",
                    f"{dec['mean']:.3f}",
                    f"{dec['median']:.3f}",
                    f"{dec['sd']:.3f}",
                    f"{dec['ci95']:.3f}",
                    f"{dec['cv']:.4f}",
                    f"{enc_tp['mean']:.3f}",
                    f"{dec_tp['mean']:.3f}",
                ]
            )


def write_tag_overhead(data, path):
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "platform",
                "compiler",
                "size_bytes",
                "size",
                "mode",
                "enc_overhead_us",
                "enc_overhead_pct",
                "dec_overhead_us",
                "dec_overhead_pct",
                "enc_throughput_loss_pct",
                "dec_throughput_loss_pct",
            ]
        )
        keys = sorted({key[:2] for key in data})
        sizes = sorted({key[3] for key in data})
        for platform, compiler in keys:
            for size in sizes:
                ctr = data.get((platform, compiler, "ctr", size))
                if not ctr:
                    continue
                ctr_enc = stats(ctr, "encrypt_us")["mean"]
                ctr_dec = stats(ctr, "decrypt_us")["mean"]
                ctr_enc_tp = stats(ctr, "throughput_encrypt_MBps")["mean"]
                ctr_dec_tp = stats(ctr, "throughput_decrypt_MBps")["mean"]
                for mode in ("gcm", "ccm"):
                    rows = data.get((platform, compiler, mode, size))
                    if not rows:
                        continue
                    enc = stats(rows, "encrypt_us")["mean"]
                    dec = stats(rows, "decrypt_us")["mean"]
                    enc_tp = stats(rows, "throughput_encrypt_MBps")["mean"]
                    dec_tp = stats(rows, "throughput_decrypt_MBps")["mean"]
                    writer.writerow(
                        [
                            platform,
                            compiler,
                            size,
                            SIZE_LABELS.get(size, str(size)),
                            mode.upper(),
                            f"{enc - ctr_enc:.3f}",
                            f"{100 * (enc / ctr_enc - 1):.2f}",
                            f"{dec - ctr_dec:.3f}",
                            f"{100 * (dec / ctr_dec - 1):.2f}",
                            f"{100 * (1 - enc_tp / ctr_enc_tp):.2f}",
                            f"{100 * (1 - dec_tp / ctr_dec_tp):.2f}",
                        ]
                    )


def write_class_summary(data, path):
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "size_bytes",
                "size",
                "class",
                "modes",
                "enc_mean_us",
                "dec_mean_us",
                "enc_mean_MBps",
                "dec_mean_MBps",
            ]
        )
        sizes = sorted({key[3] for key in data})
        for size in sizes:
            for name, modes in CLASSES.items():
                rows = [
                    row
                    for (platform, compiler, mode, s), group in data.items()
                    if s == size and mode in modes
                    for row in group
                ]
                if not rows:
                    continue
                writer.writerow(
                    [
                        size,
                        SIZE_LABELS.get(size, str(size)),
                        name,
                        "+".join(m.upper() for m in modes),
                        f"{statistics.mean(float(r['encrypt_us']) for r in rows):.3f}",
                        f"{statistics.mean(float(r['decrypt_us']) for r in rows):.3f}",
                        f"{statistics.mean(float(r['throughput_encrypt_MBps']) for r in rows):.3f}",
                        f"{statistics.mean(float(r['throughput_decrypt_MBps']) for r in rows):.3f}",
                    ]
                )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dir",
        default="output/ubuntu/benchmarks",
        help="directory containing bench_*.csv",
    )
    parser.add_argument("--prefix", default=None, help="output prefix")
    args = parser.parse_args()

    directory = Path(args.dir)
    data = load(directory)
    if not data:
        sys.exit(f"no benchmark CSVs found in {directory}")

    prefix = Path(args.prefix) if args.prefix else directory / "summary"
    write_summary(data, Path(f"{prefix}.csv"))
    write_tag_overhead(data, Path(f"{prefix}_tag_overhead.csv"))
    write_class_summary(data, Path(f"{prefix}_classes.csv"))
    print(f"wrote {prefix}.csv")
    print(f"wrote {prefix}_tag_overhead.csv")
    print(f"wrote {prefix}_classes.csv")


if __name__ == "__main__":
    main()
