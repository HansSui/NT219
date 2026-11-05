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

"$tool" encrypt --mode cbc --key key.bin --in plain.txt --out ct.b64 --encode base64
"$tool" decrypt --mode cbc --key key.bin --in ct.b64 --decode base64 --out b64_out.txt
cmp plain.txt b64_out.txt
b64_text="$(< ct.b64)"
"$tool" decrypt --mode cbc --key key.bin --text "$b64_text" --decode base64 --out b64_text_out.txt
cmp plain.txt b64_text_out.txt

nonce=00112233445566778899aabbccddeeff
"$tool" encrypt --mode ctr --key key.bin --iv "$nonce" --text first --out first.bin
if "$tool" encrypt --mode ctr --key key.bin --iv "$nonce" --text second \
        --out second.bin 2>/dev/null; then
    echo "nonce reuse was not rejected" >&2
    exit 1
fi

mkdir .aestool_nonces.lock
if "$tool" encrypt --mode ctr --key key.bin --iv ffeeddccbbaa99887766554433221100 \
        --text locked --out locked.bin 2>/dev/null; then
    echo "nonce lock was fail-open" >&2
    exit 1
fi
rmdir .aestool_nonces.lock
if [[ -e locked.bin ]]; then
    echo "lock contention wrote an output file" >&2
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

if "$tool" encrypt --mode xts --key key.bin --text data --out xts.bin 2>/dev/null; then
    echo "XTS accepted a missing IV" >&2
    exit 1
fi

if "$tool" encrypt --mode cbc --key key.bin --text data --threads 2 \
        --out threaded.bin 2>/dev/null; then
    echo "encrypt accepted --threads" >&2
    exit 1
fi

# AEAD flag and AAD policy failures.
if "$tool" encrypt --mode gcm --key key.bin --text data --out no_aead.bin 2>/dev/null; then
    echo "GCM was accepted without --aead" >&2
    exit 1
fi
if "$tool" encrypt --mode cbc --key key.bin --aad-text header --text data \
        --out aad_nonaead.bin 2>/dev/null; then
    echo "AAD was accepted for a non-AEAD mode" >&2
    exit 1
fi

# GCM with the wrong AAD must fail and leave no plaintext.
aad_nonce=112233445566778899aabbcc
"$tool" encrypt --mode gcm --aead --key key.bin --nonce "$aad_nonce" \
    --aad-text right --text "aad payload" --out aad_ct.bin
if "$tool" decrypt --mode gcm --aead --key key.bin --in aad_ct.bin \
        --aad-text wrong --out aad_out.txt 2>/dev/null; then
    echo "GCM accepted the wrong AAD" >&2
    exit 1
fi
if [[ -e aad_out.txt ]]; then
    echo "GCM wrong-AAD failure wrote an output file" >&2
    exit 1
fi

# CCM with a modified ciphertext must fail tag verification and write nothing.
ccm_nonce=ccddeeff0011223344556677
"$tool" encrypt --mode ccm --aead --key key.bin --nonce "$ccm_nonce" \
    --text "ccm payload" --out ccm_ct.bin
cp ccm_ct.bin ccm_bad.bin
printf 'X' >> ccm_bad.bin
if "$tool" decrypt --mode ccm --aead --key key.bin --in ccm_bad.bin \
        --out ccm_out.txt 2>/dev/null; then
    echo "CCM accepted a bad tag" >&2
    exit 1
fi
if [[ -e ccm_out.txt ]]; then
    echo "CCM bad-tag failure wrote an output file" >&2
    exit 1
fi

# The container header mode must match --mode.
"$tool" encrypt --mode cbc --key key.bin --text mismatch --out cbc_mismatch.bin
if "$tool" decrypt --mode gcm --aead --key key.bin --in cbc_mismatch.bin \
        --out mismatch.out 2>/dev/null; then
    echo "mode/header mismatch was not rejected" >&2
    exit 1
fi

# --allow-ecb is the explicit override for the 16 KiB limit.
"$tool" encrypt --mode ecb --key key.bin --allow-ecb --in large.bin \
    --out ecb_allowed.bin

gcm_nonce=00112233445566778899aabb
"$tool" encrypt --mode gcm --aead --key key.bin --nonce "$gcm_nonce" \
    --text "sidecar test payload" --out sidecar_ct.bin --sidecar sidecar.json
"$tool" decrypt --mode gcm --aead --key key.bin --in sidecar_ct.bin \
    --sidecar sidecar.json --out sidecar_out.txt
if [[ "$(< sidecar_out.txt)" != "sidecar test payload" ]]; then
    echo "sidecar round trip plaintext mismatch" >&2
    exit 1
fi

# --sidecar without a path defaults to OUT.json on encrypt and IN.json on decrypt.
default_nonce=aabbccddeeff001122334455
"$tool" encrypt --mode gcm --aead --key key.bin --nonce "$default_nonce" \
    --text "default sidecar" --out default_ct.bin --sidecar
"$tool" decrypt --mode gcm --aead --key key.bin --in default_ct.bin \
    --sidecar --out default_out.txt
if [[ "$(< default_out.txt)" != "default sidecar" ]]; then
    echo "default sidecar round trip plaintext mismatch" >&2
    exit 1
fi

# A copied sidecar in a fresh working directory must be rejected from the
# header alone; the per-directory registry there is empty.
mkdir sidecar-reuse
cp sidecar.json sidecar-reuse/
if (cd sidecar-reuse && "$tool" encrypt --mode gcm --aead --key ../key.bin \
        --iv "$gcm_nonce" --text reuse --out new.bin --sidecar sidecar.json 2>/dev/null); then
    echo "sidecar nonce reuse was not rejected" >&2
    exit 1
fi

"$tool" bench --mode ctr --key key.bin --size 1024 --runs 8 --warmup 0 \
    --threads 2 --out bench.csv >/dev/null
if [[ "$(wc -l < bench.csv)" -ne 9 ]]; then
    echo "threaded bench did not write one row per run" >&2
    exit 1
fi

echo "cli tests passed"
