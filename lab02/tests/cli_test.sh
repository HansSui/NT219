#!/usr/bin/env bash
set -euo pipefail

tool="${1:?usage: cli_test.sh /path/to/aestool}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

# 1. Round-trip test in CTR mode (16-byte key and 16-byte IV)
head -c 16 /dev/urandom > key16.bin
head -c 16 /dev/urandom > iv16.bin
printf 'Lab 2 Pure C++ AES-128 CTR round trip test payload! Arbitrary length text.' > plain.txt

"$tool" encrypt --mode ctr --key key16.bin --iv iv16.bin --in plain.txt --out ct.bin
"$tool" decrypt --mode ctr --key key16.bin --iv iv16.bin --in ct.bin --out out.txt
cmp plain.txt out.txt

# 2. Non-ASCII / Vietnamese path support
mkdir -p "thư mục"
"$tool" encrypt --mode ctr --key key16.bin --iv iv16.bin --in plain.txt --out "thư mục/ct.bin"
"$tool" decrypt --mode ctr --key key16.bin --iv iv16.bin --in "thư mục/ct.bin" --out "thư mục/out.txt"
cmp plain.txt "thư mục/out.txt"

# 3. Fail-closed: invalid key length (15 bytes)
head -c 15 /dev/urandom > key15.bin
if "$tool" encrypt --mode ctr --key key15.bin --iv iv16.bin --in plain.txt --out bad.bin 2>/dev/null; then
    echo "FAIL: 15-byte key was accepted" >&2
    exit 1
fi

# 4. Fail-closed: invalid IV length (8 bytes)
head -c 8 /dev/urandom > iv8.bin
if "$tool" encrypt --mode ctr --key key16.bin --iv iv8.bin --in plain.txt --out bad.bin 2>/dev/null; then
    echo "FAIL: 8-byte IV was accepted" >&2
    exit 1
fi

# 5. Benchmark command produces valid CSV
"$tool" bench --mode ctr --key-hex 000102030405060708090a0b0c0d0e0f --size 1024 --runs 5 --out bench.csv >/dev/null
lines=$(wc -l < bench.csv)
if [[ "$lines" -ne 6 ]]; then
    echo "FAIL: bench CSV did not write header + 5 run rows" >&2
    exit 1
fi

echo "All CLI smoke tests passed successfully."
