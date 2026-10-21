#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -S . -B build_gui \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      "$@"
cmake --build build_gui --parallel

echo
echo "Built build_gui/libaescore.so"
echo "Run the GUI with: ./gui/run_gui.sh"
