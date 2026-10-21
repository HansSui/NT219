#!/usr/bin/env bash
set -euo pipefail

tool="${1:?usage: cli_test.sh /path/to/aestool}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

head -c 32 /dev/urandom > key.bin
printf 'round trip payload' > plain.txt

"$tool" encrypt --mode cbc --key key.bin --in plain.txt --out ct.bin
"$tool" decrypt --mode cbc --key key.bin --in ct.bin --out out.txt
cmp plain.txt out.txt

mkdir -p "thư mục"
"$tool" encrypt --mode cbc --key key.bin --in plain.txt --out "thư mục/ct.bin"
"$tool" decrypt --mode cbc --key key.bin --in "thư mục/ct.bin" --out "thư mục/out.txt"
cmp plain.txt "thư mục/out.txt"

nonce=00112233445566778899aabbccddeeff
"$tool" encrypt --mode ctr --key key.bin --iv "$nonce" --text first --out first.bin
if "$tool" encrypt --mode ctr --key key.bin --iv "$nonce" --text second \
        --out second.bin 2>/dev/null; then
    echo "nonce reuse was not rejected" >&2
    exit 1
fi

head -c 16385 /dev/zero > large.bin
if "$tool" encrypt --mode ecb --key key.bin --in large.bin --out ecb.bin 2>/dev/null; then
    echo "large ECB payload was not blocked" >&2
    exit 1
fi

head -c 8 /dev/urandom > short.iv
if "$tool" encrypt --mode cbc --key key.bin --iv short.iv --text data --out bad.bin 2>/dev/null; then
    echo "short IV was accepted" >&2
    exit 1
fi

"$tool" bench --mode ctr --key key.bin --size 1024 --runs 8 --warmup 0 \
    --threads 2 --out bench.csv >/dev/null
if [[ "$(wc -l < bench.csv)" -ne 9 ]]; then
    echo "threaded bench did not write one row per run" >&2
    exit 1
fi

echo "cli tests passed"
