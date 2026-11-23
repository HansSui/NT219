#!/usr/bin/env bash
set -euo pipefail
domain="${1:?usage: collect_evidence.sh DOMAIN}"
cd "$(dirname "${BASH_SOURCE[0]}")"

openssl s_client -connect "$domain:443" -servername "$domain" -tls1_2 -showcerts \
    </dev/null > tls1_2.txt 2>&1
openssl s_client -connect "$domain:443" -servername "$domain" -tls1_3 -showcerts \
    </dev/null > tls1_3.txt 2>&1
curl -sS -D http_headers.txt -o /dev/null "https://$domain/"
openssl s_client -connect "$domain:443" -servername "$domain" -showcerts \
    </dev/null 2>/dev/null | sed -n '/BEGIN CERTIFICATE/,/END CERTIFICATE/p' > certificate_chain.pem

grep -E 'Protocol|Cipher|Verify return code' tls1_2.txt | head -5
grep -E 'Protocol|Cipher|Verify return code' tls1_3.txt | head -5
grep -i '^strict-transport-security' http_headers.txt || true
echo "wrote tls1_2.txt tls1_3.txt http_headers.txt certificate_chain.pem"
