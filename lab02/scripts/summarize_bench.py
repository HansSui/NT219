#!/usr/bin/env python3
"""Summarize aestool CTR benchmark CSVs into report tables."""
import argparse
import csv
import math
import statistics
import sys
from pathlib import Path

SIZE_LABELS = {
    1024: "1KB",
    4096: "4KB",
    16384: "16KB",
    262144: "256KB",
    1048576: "1MB",
    8388608: "8MB",
    104857600: "100MB",
    1073741824: "1GB",
}


def load(directory):
    data = {}
    for path in sorted(directory.glob("bench_*.csv")):
        parts = path.stem.split("_")
        if len(parts) < 5 or parts[3] != "ctr":
            continue
        with open(path, newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
        if rows:
            data[(parts[1], parts[2], int(rows[0]["size_bytes"]))] = rows
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


def trimmed_mean(values, fraction=0.05):
    ordered = sorted(values)
    cut = int(len(ordered) * fraction)
    if len(ordered) - 2 * cut <= 0:
        return statistics.mean(ordered)
    return statistics.mean(ordered[cut : len(ordered) - cut])


def robust_stats(rows, time_column, throughput_column):
    times = [float(row[time_column]) for row in rows]
    throughputs = [float(row[throughput_column]) for row in rows]
    base = stats(rows, time_column)
    median = base["median"]
    outliers = sum(1 for value in times if median and value > 2.0 * median)
    return {
        "median_us": median,
        "trimmed_mean_us": trimmed_mean(times),
        "median_MBps": statistics.median(throughputs),
        "outlier_pct": 100.0 * outliers / len(times) if times else 0.0,
        "cv": base["cv"],
        "stability": "ok"
        if base["cv"] <= 0.25 and (not times or outliers / len(times) <= 0.05)
        else "noisy",
    }


def write_summary(data, path):
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
        for key in sorted(data, key=lambda k: (k[0], k[1], k[2])):
            platform, compiler, size = key
            rows = data[key]
            enc = stats(rows, "encrypt_us")
            dec = stats(rows, "decrypt_us")
            enc_tp = stats(rows, "throughput_encrypt_MBps")
            dec_tp = stats(rows, "throughput_decrypt_MBps")
            writer.writerow(
                [
                    platform,
                    compiler,
                    "CTR",
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


def write_robust_summary(data, path):
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
                "enc_median_us",
                "dec_median_us",
                "enc_trimmed_mean_us",
                "dec_trimmed_mean_us",
                "enc_median_MBps",
                "dec_median_MBps",
                "enc_outlier_pct",
                "dec_outlier_pct",
                "enc_cv",
                "dec_cv",
                "enc_stability",
                "dec_stability",
            ]
        )
        noisy = {"enc": 0, "dec": 0}
        worst = []
        for key in sorted(data, key=lambda k: (k[0], k[1], k[2])):
            platform, compiler, size = key
            rows = data[key]
            enc = robust_stats(rows, "encrypt_us", "throughput_encrypt_MBps")
            dec = robust_stats(rows, "decrypt_us", "throughput_decrypt_MBps")
            if enc["stability"] == "noisy":
                noisy["enc"] += 1
            if dec["stability"] == "noisy":
                noisy["dec"] += 1
            worst.append((enc["cv"], f"CTR {SIZE_LABELS.get(size, size)} enc"))
            worst.append((dec["cv"], f"CTR {SIZE_LABELS.get(size, size)} dec"))
            writer.writerow(
                [
                    platform,
                    compiler,
                    "CTR",
                    size,
                    SIZE_LABELS.get(size, str(size)),
                    len(rows),
                    f"{enc['median_us']:.3f}",
                    f"{dec['median_us']:.3f}",
                    f"{enc['trimmed_mean_us']:.3f}",
                    f"{dec['trimmed_mean_us']:.3f}",
                    f"{enc['median_MBps']:.3f}",
                    f"{dec['median_MBps']:.3f}",
                    f"{enc['outlier_pct']:.2f}",
                    f"{dec['outlier_pct']:.2f}",
                    f"{enc['cv']:.4f}",
                    f"{dec['cv']:.4f}",
                    enc["stability"],
                    dec["stability"],
                ]
            )
    total = len(data)
    print(f"stability: enc noisy {noisy['enc']}/{total}, dec noisy {noisy['dec']}/{total}")
    for cv, label in sorted(worst, reverse=True)[:5]:
        print(f"  worst CV: {label} cv={cv:.3f}")


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
        sys.exit(f"no CTR benchmark CSVs found in {directory}")

    prefix = Path(args.prefix) if args.prefix else directory / "summary"
    write_summary(data, Path(f"{prefix}.csv"))
    write_robust_summary(data, Path(f"{prefix}_robust.csv"))
    print(f"wrote {prefix}.csv")
    print(f"wrote {prefix}_robust.csv")


if __name__ == "__main__":
    main()
