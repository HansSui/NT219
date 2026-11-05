#!/usr/bin/env python3
"""Concurrent nonce-reuse regression test for aestool.

Eight independent processes encrypt with the same key and CTR nonce in one
working directory. The per-directory registry and lock must let exactly one
through and reject the other seven, and only one output file may exist.

This is deliberately not part of ctest: it depends on the host scheduler and
on Python, while the normal build only requires CMake, a C++17 compiler and
Crypto++.

Usage:
    python tests/nonce_race_test.py --tool build/windows-gcc/aestool.exe
    python tests/nonce_race_test.py --tool build/ubuntu-gcc/aestool --processes 8
"""
import argparse
import os
import subprocess
import sys
import tempfile
import time

KEY_HEX = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
NONCE = "00112233445566778899aabbccddeeff"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", required=True, help="path to aestool or aestool.exe")
    parser.add_argument("--processes", type=int, default=8, help="concurrent processes (default 8)")
    parser.add_argument("--timeout", type=float, default=30.0, help="seconds to wait for all processes")
    args = parser.parse_args()

    tool = os.path.abspath(args.tool)
    if not os.path.exists(tool):
        print(f"tool not found: {tool}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="aestool-race-") as work:
        key_path = os.path.join(work, "key.bin")
        with open(key_path, "wb") as handle:
            handle.write(bytes.fromhex(KEY_HEX))

        procs = []
        for index in range(args.processes):
            out = os.path.join(work, f"race_{index}.bin")
            proc = subprocess.Popen(
                [tool, "encrypt", "--mode", "ctr", "--key", key_path,
                 "--iv", NONCE, "--text", "race", "--out", out],
                cwd=work,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            procs.append(proc)

        successes, failures = 0, 0
        deadline = time.time() + args.timeout
        for proc in procs:
            try:
                proc.communicate(timeout=max(0.1, deadline - time.time()))
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.communicate()
                print("a process timed out", file=sys.stderr)
                return 1
            if proc.returncode == 0:
                successes += 1
            else:
                failures += 1

        outputs = sorted(name for name in os.listdir(work) if name.startswith("race_") and name.endswith(".bin"))
        print(f"processes={args.processes} successes={successes} failures={failures} outputs={len(outputs)}")
        for name in outputs:
            print(f"  {name} ({os.path.getsize(os.path.join(work, name))} bytes)")

        if successes != 1 or failures != args.processes - 1 or len(outputs) != 1:
            print("FAIL: expected exactly one success and one output file", file=sys.stderr)
            return 1
        print("PASS: nonce lock is exclusive across processes")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
