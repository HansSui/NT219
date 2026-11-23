#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
tool="../../build/ubuntu-gcc/hashtool"

printf '0123456789abcdef' > key.bin
printf 'amount=100&to=alice' > original_message.bin
printf '&to=mallory' > append.bin
cat key.bin original_message.bin > macinput.bin
mac="$("$tool" --algo sha256 --in macinput.bin)"
echo "$mac" > original_mac.txt
"$tool" lenext --mac "$mac" --key-len 16 --in original_message.bin \
    --append append.bin --out forged_message.bin > forged_report.txt
forged_mac="$(awk '/^forged_mac:/ {print $2}' forged_report.txt)"
echo "$forged_mac" > forged_mac.txt
cat key.bin forged_message.bin > verifyinput.bin
actual="$("$tool" --algo sha256 --in verifyinput.bin)"
[[ "$actual" == "$forged_mac" ]]
echo "original MAC: $mac"
echo "forged MAC:   $forged_mac"
echo "verification: $actual"
rm -f macinput.bin verifyinput.bin
