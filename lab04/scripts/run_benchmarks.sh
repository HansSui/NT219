#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${1:-gcc}"
tool="$root/build/ubuntu-$compiler/hashtool"
outdir="$root/output/ubuntu/benchmarks"
mkdir -p "$outdir"

algos="${ALGOS:-sha256,sha512,sha3-256,sha3-512}"
sizes="${SIZES:-1,100,1024}"
io="${IO:-stream,mmap}"
warmup="${WARMUP:-1}"
runs_arg=()
if [[ -n "${RUNS:-}" ]]; then runs_arg=(--runs "$RUNS"); fi

"$tool" bench --algos "$algos" --sizes "$sizes" --io "$io" --warmup "$warmup" \
    "${runs_arg[@]}" --out "$outdir/bench_ubuntu_${compiler}.csv"

python3 "$root/scripts/summarize_bench.py" \
    --input "$outdir/bench_ubuntu_${compiler}.csv" \
    --out "$outdir/summary_ubuntu_${compiler}.csv"

python3 "$root/scripts/plot_bench.py" \
    --input "$outdir/summary_ubuntu_${compiler}.csv" \
    --out-dir "$outdir/graphs" --platform "Ubuntu ${compiler}"

echo "benchmarks written to $outdir"
