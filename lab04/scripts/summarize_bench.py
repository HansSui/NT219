#!/usr/bin/env python3
"""Summarize hashtool benchmark CSV rows into mean/median/stddev/95% CI."""
import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path


def stats(values):
    n = len(values)
    mean = statistics.mean(values) if n else 0.0
    sd = statistics.stdev(values) if n > 1 else 0.0
    return n, mean, statistics.median(values) if n else 0.0, sd, 1.96 * sd / math.sqrt(n) if n else 0.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    groups = defaultdict(list)
    with open(args.input, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            groups[(row["algo"], row["size_mib"], row["io"])].append(row)

    output = Path(args.out)
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "algo", "size_mib", "io", "runs",
            "elapsed_mean_ms", "elapsed_median_ms", "elapsed_sd_ms", "elapsed_ci95_ms",
            "mbps_mean", "mbps_median", "mbps_sd", "mbps_ci95",
            "cpu_pct_mean", "cpu_pct_ci95",
        ])
        for key in sorted(groups, key=lambda k: (int(k[1]), k[0], k[2])):
            rows = groups[key]
            ms = stats([float(row["elapsed_ms"]) for row in rows])
            mbps = stats([float(row["mbps"]) for row in rows])
            cpu = stats([float(row["cpu_pct"]) for row in rows])
            writer.writerow([*key, ms[0], *[f"{v:.3f}" for v in ms[1:]],
                             *[f"{v:.3f}" for v in mbps[1:]],
                             *[f"{v:.3f}" for v in cpu[1:]]])
    print(f"wrote {output}")


if __name__ == "__main__":
    main()
