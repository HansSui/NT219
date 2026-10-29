#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${1:-gcc}"
bin="$root/build/ubuntu-$compiler/aestool"

if [[ ! -x "$bin" ]]; then
    echo "Executable not found at $bin. Building first..."
    "$root/scripts/build_linux.sh" "$compiler"
fi

out_dir="$root/output/ubuntu/benchmarks"
mkdir -p "$out_dir"

# Sizes per Lab 2 specification: 1 MiB, 100 MiB (and 16KB / 4MB for scaling)
sizes=(1048576 104857600)
# Optional 1 GiB: 1073741824 (uncomment if RAM and time permit)
if [[ "${BENCH_1GB:-0}" == "1" ]]; then
    sizes+=(1073741824)
fi

echo "Running Lab 2 benchmarks for AES-128 CTR mode using $compiler..."
for size in "${sizes[@]}"; do
    csv="$out_dir/bench_ubuntu_${compiler}_ctr_${size}.csv"
    echo "  Benchmarking size ${size} bytes -> $csv"
    "$bin" bench --mode ctr --size "$size" --runs "${RUNS:-100}" --out "$csv"
done

echo "Benchmarks completed. Results stored in $out_dir"

if command -v python3 >/dev/null 2>&1; then
    echo "Summarizing results..."
    python3 "$root/scripts/summarize_bench.py" --dir "$out_dir"
fi
