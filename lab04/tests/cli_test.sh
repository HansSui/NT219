#!/usr/bin/env bash
set -euo pipefail

tool="${1:?usage: cli_test.sh /path/to/hashtool}"
kat="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/kat/hash_vectors.json"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

printf 'abc' > abc.bin
[[ "$("$tool" --algo sha256 --in abc.bin)" == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]]
[[ "$("$tool" --algo sha3-256 --text abc)" == "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532" ]]
[[ "$("$tool" --algo shake128 --outlen 256 --in abc.bin)" == "5881092dd818bf5cf8a3ddb793fbcba74097d5c526a6d35f97b83351940f2cc8" ]]

"$tool" --algo sha256 --in abc.bin --out digest.bin
[[ "$(stat -c %s digest.bin 2>/dev/null || stat -f %z digest.bin)" == "32" ]]
"$tool" --algo sha256 --in abc.bin --out digest.hex --encode hex
[[ "$(cat digest.hex)" == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]]
"$tool" --algo sha256 --in abc.bin --encode base64 | base64 -d > digest.raw
cmp digest.bin digest.raw

for bad in "--algo md5 --in abc.bin" "--algo sha256 --outlen 256 --in abc.bin" \
           "--algo shake128 --in abc.bin" "--algo shake128 --outlen 7 --in abc.bin" \
           "--algo sha256 --in missing.bin" "--algo sha256 --in abc.bin --encode nope"; do
    if "$tool" $bad >/dev/null 2>&1; then
        echo "expected failure: $bad" >&2
        exit 1
    fi
done

"$tool" --kat "$kat" | grep -q "0 failed"

if command -v openssl >/dev/null 2>&1; then
    export OPENSSL_CONF="${OPENSSL_CONF:-/etc/ssl/openssl.cnf}"
    openssl req -x509 -newkey rsa:3072 -nodes -keyout cert.key -out cert.pem -days 1 \
        -subj '/CN=lab4.example/O=NT219' -addext 'subjectAltName=DNS:lab4.example' \
        -addext 'keyUsage=digitalSignature,keyEncipherment' -sha256 >/dev/null 2>&1
    "$tool" cert --in cert.pem | grep -q "UNVERIFIED"
    "$tool" cert --in cert.pem --issuer cert.pem --require-verified | grep -q "verified against supplied issuer key"
    if "$tool" cert --in cert.pem --require-verified >/dev/null 2>&1; then
        echo "unverified certificate passed --require-verified" >&2
        exit 1
    fi
    openssl req -x509 -newkey rsa:2048 -nodes -keyout other.key -out other.pem -days 1 \
        -subj '/CN=other.example' -sha256 >/dev/null 2>&1
    if "$tool" cert --in cert.pem --issuer other.pem >/dev/null 2>&1; then
        echo "wrong issuer was accepted" >&2
        exit 1
    fi
    printf 'not a certificate' > bad.pem
    if "$tool" cert --in bad.pem >/dev/null 2>&1; then
        echo "malformed certificate was accepted" >&2
        exit 1
    fi
fi

printf '0123456789abcdef' > key.bin
printf 'amount=100&to=alice' > message.bin
cat key.bin message.bin > macinput.bin
mac="$("$tool" --algo sha256 --in macinput.bin)"
"$tool" lenext --mac "$mac" --key-len 16 --in message.bin --append-text '&to=mallory' --out forged.bin > report.txt
forged_mac="$(awk '/^forged_mac:/ {print $2}' report.txt)"
[[ -n "$forged_mac" ]]
cat key.bin forged.bin > verify.bin
[[ "$("$tool" --algo sha256 --in verify.bin)" == "$forged_mac" ]]
if "$tool" lenext --mac 00 --key-len 16 --in message.bin --append-text x >/dev/null 2>&1; then
    echo "short MAC was accepted" >&2
    exit 1
fi

"$tool" bench --algos sha256 --sizes 1 --io stream,mmap --runs 1 --warmup 0 --out bench.csv
grep -q '^algo,size_bytes,size_mib,io,run,elapsed_ms,mbps,cpu_ms,cpu_pct$' bench.csv
[[ "$(wc -l < bench.csv)" == "3" ]]

echo "all Lab 4 CLI cases passed"
