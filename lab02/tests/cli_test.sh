#!/usr/bin/env bash
set -euo pipefail

tool="${1:?usage: cli_test.sh /path/to/aestool /path/to/kat/vectors.json}"
kat="${2:?usage: cli_test.sh /path/to/aestool /path/to/kat/vectors.json}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

key128="2b7e151628aed2a6abf7158809cf4f3c"
iv128="f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
keyxts="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"

# 1. Round-trip test in CTR mode (16-byte key and 16-byte IV)
head -c 16 /dev/urandom > key16.bin
head -c 16 /dev/urandom > iv16.bin
printf 'Lab 2 Pure C++ AES-128 CTR round trip test payload! Arbitrary length text.' > plain.txt

"$tool" encrypt --mode ctr --key key16.bin --iv iv16.bin --in plain.txt --out ct.bin
"$tool" decrypt --mode ctr --key key16.bin --iv iv16.bin --in ct.bin --out out.txt
cmp plain.txt out.txt

# 2. Non-ASCII / Vietnamese path and text support
mkdir -p "thư mục"
printf 'Xin chào Việt Nam — AES-CTR UTF-8 test' > "thư mục/plain.txt"
"$tool" encrypt --mode ctr --key key16.bin --iv iv16.bin --in "thư mục/plain.txt" --out "thư mục/ct.bin"
"$tool" decrypt --mode ctr --key key16.bin --iv iv16.bin --in "thư mục/ct.bin" --out "thư mục/out.txt"
cmp "thư mục/plain.txt" "thư mục/out.txt"

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

# 5. XTS bonus round-trip, including ciphertext stealing.
printf 'XTS round-trip with a partial final block.' > xts.txt
"$tool" encrypt --mode xts --key-hex "$keyxts" --iv-hex 01000000000000000000000000000000 \
    --in xts.txt --out xts.bin
"$tool" decrypt --mode xts --key-hex "$keyxts" --iv-hex 01000000000000000000000000000000 \
    --in xts.bin --out xts.out
cmp xts.txt xts.out

# 6. --nonce is an alias for the CTR counter.
"$tool" encrypt --mode ctr --key-hex "$key128" --nonce "$iv128" \
    --text "nonce alias" --out nonce.bin
"$tool" decrypt --mode ctr --key-hex "$key128" --nonce-hex "$iv128" \
    --in nonce.bin --out nonce.out
cmp <(printf 'nonce alias') nonce.out

# 7. Base64 encode/decode round-trip.
"$tool" encrypt --mode ctr --key-hex "$key128" --iv-hex "$iv128" \
    --text "base64 round trip" --encode base64 --out ct.b64
"$tool" decrypt --mode ctr --key-hex "$key128" --iv-hex "$iv128" \
    --decode base64 --in ct.b64 --out b64.out
cmp <(printf 'base64 round trip') b64.out

# 8. Conflicting key sources are rejected.
if "$tool" encrypt --mode ctr --key key16.bin --key-hex "$key128" --iv-hex "$iv128" \
    --text data --out bad.bin 2>/dev/null; then
    echo "FAIL: --key and --key-hex were both accepted" >&2
    exit 1
fi

# 9. --threads is bench-only.
if "$tool" encrypt --mode ctr --key-hex "$key128" --iv-hex "$iv128" \
    --text data --threads 2 --out bad.bin 2>/dev/null; then
    echo "FAIL: encrypt accepted --threads" >&2
    exit 1
fi

# 10. bench rejects unknown options instead of silently ignoring them.
if "$tool" bench --mode ctr --key-hex "$key128" --size 1024 --runs 2 \
    --warmup 0 --mystery-flag 1 --out bad.csv 2>/dev/null; then
    echo "FAIL: bench accepted an unknown option" >&2
    exit 1
fi

# 11. Benchmark command produces a valid CSV and --verbose reports progress.
"$tool" bench --mode ctr --key-hex "$key128" --size 1024 --runs 5 \
    --warmup 0 --threads 2 --verbose --out bench.csv >/dev/null 2>bench.err
lines=$(wc -l < bench.csv)
if [[ "$lines" -ne 6 ]]; then
    echo "FAIL: bench CSV did not write header + 5 run rows" >&2
    exit 1
fi
if ! grep -q "progress:" bench.err; then
    echo "FAIL: bench --verbose did not report progress" >&2
    exit 1
fi
if ! grep -q "bench: completed 5 runs" bench.err; then
    echo "FAIL: bench --verbose did not report completion" >&2
    exit 1
fi

# 12. The JSON KAT runner covers CTR/ECB/XTS and fails on unsupported modes.
kat_out="$("$tool" --kat "$kat" 2>/dev/null)"
grep -q "9 passed, 0 failed" <<<"$kat_out"

cat > unsupported.json <<'JSON'
{"vectors":[{"name":"unsupported","mode":"gcm","key":"00","iv":"","plaintext":"00","ciphertext":"00"}]}
JSON
if "$tool" --kat unsupported.json >/dev/null 2>&1; then
    echo "FAIL: KAT runner accepted an unsupported mode without a skip failure" >&2
    exit 1
fi

echo "All CLI smoke tests passed successfully."
