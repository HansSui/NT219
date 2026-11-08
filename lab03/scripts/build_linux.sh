#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${1:-gcc}"

case "$compiler" in
    gcc)   cxx="${CXX:-g++}" ;;
    clang) cxx="${CXX:-clang++}" ;;
    *)     echo "usage: $0 [gcc|clang]" >&2; exit 1 ;;
esac

# Lab 3 requires OpenSSL 4.0.2. Use OPENSSL_ROOT_DIR when set, otherwise
# auto-detect the local 4.0.2 install if present.
if [[ -z "${OPENSSL_ROOT_DIR:-}" ]]; then
    for candidate in "$HOME/CryptoLib/lib/openssl/gcc" /usr/local /opt/openssl; do
        if [[ -f "$candidate/include/openssl/evp.h" ]] &&
           grep -q 'OPENSSL_VERSION_STR "4\.0\.2"' "$candidate/include/openssl/opensslv.h"; then
            OPENSSL_ROOT_DIR="$candidate"
            break
        fi
    done
fi
if [[ -z "${OPENSSL_ROOT_DIR:-}" ]]; then
    echo "OpenSSL 4.0.2 not found; set OPENSSL_ROOT_DIR=/path/to/openssl" >&2
    exit 1
fi

build="$root/build/ubuntu-$compiler"
args=(-S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$cxx"
      -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR")
cmake "${args[@]}"
cmake --build "$build" -j "${JOBS:-$(nproc)}"
(cd "$build" && ctest --output-on-failure)
echo "built: $build/rsatool"
