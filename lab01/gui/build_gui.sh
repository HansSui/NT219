#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -S . -B build/ubuntu-gcc-shared \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      "$@"
cmake --build build/ubuntu-gcc-shared --parallel

echo
echo "Built build/ubuntu-gcc-shared/libaescore.so"
echo "Run the GUI with: ./gui/run_gui.sh"
