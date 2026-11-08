#!/usr/bin/env bash
set -euo pipefail

tool="${1:?usage: cli_test.sh /path/to/rsatool}"
kat="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/kat/oaep_vectors.json"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

"$tool" keygen --bits 3072 --pub pub.pem --priv priv.pem
[[ -s pub.pem && -s priv.pem && -s pub.der && -s priv.der && -s priv.json ]]
grep -q '"modulus_bits": 3072' priv.json

printf 'direct OAEP payload' > small.txt
"$tool" encrypt --in small.txt --pub pub.pem --out small.ct --label ctx
"$tool" decrypt --in small.ct --priv priv.pem --out small.out --label ctx
cmp small.txt small.out

# Wrong label fails and writes nothing.
if "$tool" decrypt --in small.ct --priv priv.pem --out wrong.out --label other 2>/dev/null; then
    echo "wrong OAEP label was accepted" >&2
    exit 1
fi
[[ ! -e wrong.out ]]

# Automatic hybrid mode above 318 bytes.
head -c 4096 /dev/urandom > big.bin
"$tool" encrypt --in big.bin --pub pub.pem --out big.env --label ctx
"$tool" decrypt --in big.env --priv priv.pem --out big.out --label ctx
cmp big.bin big.out

# Base64 envelope round-trip.
"$tool" encrypt --in big.bin --pub pub.pem --out big.b64 --encode base64
"$tool" decrypt --in big.b64 --priv priv.pem --decode base64 --out big_b64.out
cmp big.bin big_b64.out

# Tampered payload, header and malformed input fail closed.
cp big.env bad_payload.env
printf 'X' >> bad_payload.env
if "$tool" decrypt --in bad_payload.env --priv priv.pem --out bad.out 2>/dev/null; then
    echo "tampered envelope payload was accepted" >&2
    exit 1
fi
[[ ! -e bad.out ]]

sed 's/RSA-OAEP-AES-GCM/AES-CBC/' big.env > bad_header.env
if "$tool" decrypt --in bad_header.env --priv priv.pem --out bad_header.out 2>/dev/null; then
    echo "tampered envelope header was accepted" >&2
    exit 1
fi

printf 'short' > short.ct
if "$tool" decrypt --in short.ct --priv priv.pem --out short.out 2>/dev/null; then
    echo "malformed ciphertext was accepted" >&2
    exit 1
fi

# Wrong key.
"$tool" keygen --bits 3072 --pub pub2.pem --priv priv2.pem
if "$tool" decrypt --in small.ct --priv priv2.pem --out wrongkey.out --label ctx 2>/dev/null; then
    echo "wrong private key was accepted" >&2
    exit 1
fi

# Unsupported key size and bad encoding.
if "$tool" keygen --bits 2048 --pub weak.pem --priv weak-priv.pem 2>/dev/null; then
    echo "2048-bit key generation was accepted" >&2
    exit 1
fi
printf 'abc' > odd.hex
if "$tool" decrypt --in odd.hex --priv priv.pem --decode hex --out odd.out 2>/dev/null; then
    echo "odd-length hex was accepted" >&2
    exit 1
fi

"$tool" --kat "$kat" | grep -q "2 passed, 0 failed"

# Independent OpenSSL format and OAEP cross-check when the CLI is available.
if command -v openssl >/dev/null 2>&1; then
    openssl pkey -in pub.pem -pubin -noout
    openssl pkey -in priv.pem -noout
    printf 'openssl oracle payload' > openssl.txt
    openssl pkeyutl -encrypt -pubin -inkey pub.pem -in openssl.txt -out openssl.ct \
        -pkeyopt rsa_padding_mode:oaep -pkeyopt rsa_oaep_md:sha256 \
        -pkeyopt rsa_mgf1_md:sha256
    "$tool" decrypt --in openssl.ct --priv priv.pem --out openssl.out
    cmp openssl.txt openssl.out
fi

echo "all Lab 3 CLI cases passed"
