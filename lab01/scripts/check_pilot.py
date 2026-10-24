#!/usr/bin/env python3
"""Check whether a short benchmark pilot is stable enough for the full run.

A single scheduling hiccup should not invalidate the run: the check uses a
1%-trimmed CV and the percentage of >2x-median outliers, while still printing
the raw statistics for context.
"""
import argparse
import csv
import os
import statistics
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv")
    parser.add_argument("--baseline", default=None, help="previous summary.csv for a slowdown check")
    parser.add_argument("--max-slowdown", type=float, default=1.50)
    parser.add_argument("--max-trimmed-cv", type=float, default=0.20)
    parser.add_argument("--max-outlier-pct", type=float, default=2.0)
    parser.add_argument("--max-mean-over-median", type=float, default=1.50)
    args = parser.parse_args()

    with open(args.csv, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        sys.exit("pilot: no rows")

    values = sorted(float(row["encrypt_us"]) for row in rows)
    n = len(values)
    mean = statistics.mean(values)
    median = statistics.median(values)
    sd = statistics.stdev(values) if n > 1 else 0.0
    cv = sd / mean if mean else 0.0
    ratio = mean / median if median else 0.0

    cut = max(1, int(n * 0.01)) if n >= 20 else 0
    trimmed = values[cut : n - cut] if cut else values
    trimmed_cv = statistics.stdev(trimmed) / statistics.mean(trimmed) if len(trimmed) > 1 else 0.0
    outliers = sum(1 for value in values if median and value > 2.0 * median)
    outlier_pct = 100.0 * outliers / n

    print(
        f"pilot: n={n} mean={mean:.3f}us median={median:.3f}us "
        f"cv={cv:.3f} trimmed_cv={trimmed_cv:.3f} "
        f"outlier_pct={outlier_pct:.1f}% mean/median={ratio:.2f} "
        f"max={max(values):.3f}us"
    )

    noisy = (
        trimmed_cv > args.max_trimmed_cv
        or outlier_pct > args.max_outlier_pct
        or ratio > args.max_mean_over_median
    )

    if args.baseline and os.path.exists(args.baseline):
        with open(args.baseline, newline="", encoding="utf-8") as handle:
            baseline_rows = list(csv.DictReader(handle))
        baseline = [
            float(row["enc_median_us"])
            for row in baseline_rows
            if row.get("mode", "").upper() == "CTR"
            and int(row.get("size_bytes", 0)) == 1048576
        ]
        if baseline:
            slowdown = median / baseline[0] if baseline[0] else 0.0
            print(f"pilot: baseline CTR 1MB median={baseline[0]:.3f}us slowdown={slowdown:.2f}x")
            if slowdown > args.max_slowdown:
                print(
                    "pilot: SLOWER than the previous run - check AC power, "
                    "power plan, and background load"
                )
                noisy = True

    if noisy:
        print(
            "pilot: NOISY - close background apps, wait for indexing/updates "
            "to finish, and rerun (or set FORCE=1 to ignore)"
        )
        return 2
    print("pilot: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
