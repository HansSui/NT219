#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

hc="${HASHCLASH_DIR:-/tmp/hashclash-static/hashclash-static-release-v1.2b}"
if [[ ! -x "$hc/bin/md5_fastcoll" ]]; then
    tmp="$(mktemp -d)"
    curl -L --fail -o "$tmp/hashclash.tar.gz" \
        https://github.com/cr-marcstevens/hashclash/releases/download/hashclash-static-release-v1.2b/hashclash-static-release-v1.2b.tar.gz
    tar xzf "$tmp/hashclash.tar.gz" -C "$tmp"
    hc="$tmp/hashclash-static-release-v1.2b"
fi

g++ -std=c++17 -O2 make_certs.cpp -lcrypto -o make_certs
HASHCLASH="$hc/bin/md5_fastcoll" OPENSSL_CONF=/etc/ssl/openssl.cnf ./make_certs

python3 - <<'PY'
import hashlib
from cryptography import x509
for name in ("certA.pem", "certB.pem"):
    cert = x509.load_pem_x509_certificate(open(name, "rb").read())
    print(name, "MD5(TBS) =", hashlib.md5(cert.tbs_certificate_bytes).hexdigest(),
          "signature =", cert.signature.hex()[:32] + "...")
PY

rm -f make_certs prefix.bin collision1.bin collision2.bin demo_key.pem
