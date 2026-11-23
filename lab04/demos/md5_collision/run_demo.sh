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

"$hc/bin/md5_fastcoll" -q -p prefix.png -o png1.png png2.png
md5sum png1.png png2.png
python3 - <<'PY'
import struct, zlib, pathlib, hashlib
for name in ("png1.png", "png2.png"):
    data = pathlib.Path(name).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos = 8
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        crc = struct.unpack(">I", data[pos + 8 + length:pos + 12 + length])[0]
        assert crc == zlib.crc32(data[pos + 4:pos + 8 + length]) & 0xffffffff
        pos += 12 + length
        if kind == b"IEND":
            break
    print(name, "valid PNG; md5", hashlib.md5(data).hexdigest())
PY
