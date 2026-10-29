#!/usr/bin/env python3
"""Measure system-call / file-I/O overhead that `aestool bench` excludes.

`aestool bench` times only the in-memory Crypto++ call. This script drives the
real CLI end-to-end over files, so the wall-clock includes process startup,
read()/write() syscalls and the filesystem copy. Subtracting the in-memory
crypto time (`summary.csv`) and an `aestool help` startup baseline isolates the
syscall/I/O cost. Standard library only; runs on Windows and Linux.

From lab01/:
  python scripts/bench_io.py --tool build/ubuntu-gcc/aestool \
      --summary output/ubuntu/benchmarks/summary.csv \
      --out output/ubuntu/benchmarks/io_overhead.csv
"""
import argparse
import csv
import os
import statistics
import subprocess
import tempfile
import time

SIZES = [(1024, "1KB"), (4096, "4KB"), (16384, "16KB"),
         (262144, "256KB"), (1048576, "1MB"), (8388608, "8MB")]


def timed(cmd):
    start = time.perf_counter()
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return (time.perf_counter() - start) * 1000.0  # ms


def load_summary(path, mode):
    crypto = {}
    if not path:
        return crypto
    with open(path, newline="") as handle:
        for row in csv.DictReader(handle):
            if row.get("mode", "").upper() == mode.upper():
                crypto[int(row["size_bytes"])] = row
    return crypto


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tool", required=True, help="path to the aestool binary")
    parser.add_argument("--mode", default="CBC", help="one non-AEAD mode is enough; I/O cost is mode-independent")
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--summary", default="", help="analysis summary.csv for the in-memory baseline")
    parser.add_argument("--out", default="output/ubuntu/benchmarks/io_overhead.csv")
    args = parser.parse_args()

    key = "00" * 32
    crypto = load_summary(args.summary, args.mode)
    startup_ms = statistics.mean(timed([args.tool, "help"]) for _ in range(args.runs))

    rows = []
    with tempfile.TemporaryDirectory() as work:
        for size, label in SIZES:
            src = os.path.join(work, "in.bin")
            with open(src, "wb") as handle:
                handle.write(os.urandom(size))
            ct = os.path.join(work, "ct.bin")
            out = os.path.join(work, "out.bin")
            enc = [timed([args.tool, "encrypt", "--mode", args.mode, "--key-hex", key,
                          "--in", src, "--out", ct]) for _ in range(args.runs)]
            dec = [timed([args.tool, "decrypt", "--mode", args.mode, "--key-hex", key,
                          "--in", ct, "--out", out]) for _ in range(args.runs)]

            enc_ms, dec_ms = statistics.mean(enc), statistics.mean(dec)
            base = crypto.get(size)
            c_enc = float(base["enc_mean_us"]) / 1000.0 if base else float("nan")
            c_dec = float(base["dec_mean_us"]) / 1000.0 if base else float("nan")
            rows.append({
                "mode": args.mode, "size_bytes": size, "size": label, "runs": args.runs,
                "startup_ms": round(startup_ms, 3),
                "end2end_enc_ms": round(enc_ms, 3), "end2end_dec_ms": round(dec_ms, 3),
                "crypto_enc_ms": round(c_enc, 3), "crypto_dec_ms": round(c_dec, 3),
                "io_enc_ms": round(max(0.0, enc_ms - startup_ms - c_enc), 3),
                "io_dec_ms": round(max(0.0, dec_ms - startup_ms - c_dec), 3),
                "end2end_enc_MBps": round(size / (enc_ms / 1000.0) / 1e6, 1),
                "end2end_dec_MBps": round(size / (dec_ms / 1000.0) / 1e6, 1),
            })
            print(f"{label:>5}  enc: end2end={enc_ms:8.2f}ms io={rows[-1]['io_enc_ms']:7.2f}ms"
                  f"  dec: end2end={dec_ms:8.2f}ms io={rows[-1]['io_dec_ms']:7.2f}ms")

    print(f"startup baseline (`{args.mode}` runs={args.runs}): {startup_ms:.2f} ms")
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
