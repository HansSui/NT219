#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${1:-gcc}"

case "$compiler" in
    gcc)   cxx="${CXX:-g++}" ;;
    clang) cxx="${CXX:-clang++}" ;;
    *)     echo "usage: $0 [gcc|clang]" >&2; exit 1 ;;
esac

build="$root/build_ubuntu_$compiler"
args=(-S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$cxx")
if [[ -n "${CRYPTOPP_ROOT:-}" ]]; then
    args+=(-DCRYPTOPP_ROOT="$CRYPTOPP_ROOT")
fi

cmake "${args[@]}"
cmake --build "$build" -j "${JOBS:-$(nproc)}"
(cd "$build" && ctest --output-on-failure)
echo "built: $build/aestool"
