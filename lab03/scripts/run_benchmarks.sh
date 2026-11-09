#!/usr/bin/env bash
# Runs the Lab 3 RSA/hybrid benchmark matrix and writes a CSV plus summary.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${1:-gcc}"
bin="$root/build/ubuntu-$compiler/rsatool"
out_dir="${OUT_DIR:-$root/output/ubuntu/benchmarks}"

if [[ ! -x "$bin" ]]; then
    "$root/scripts/build_linux.sh" "$compiler"
fi

runs_rsa="${RUNS_RSA:-${RUNS:-1000}}"
runs_small="${RUNS_SMALL:-${RUNS:-1000}}"
runs_large="${RUNS_LARGE:-${RUNS:-100}}"
warmup="${WARMUP:-1.0}"
threads="${THREADS:-4}"
if [[ -z "${PIN_CPU:-}" ]]; then
    PIN_CPU="0-3"
fi
launcher=()
if [[ "$PIN_CPU" != "none" ]] && command -v taskset >/dev/null 2>&1; then
    launcher=(taskset -c "$PIN_CPU")
fi

mkdir -p "$out_dir"
csv="$out_dir/bench_ubuntu_${compiler}_rsa_hybrid.csv"
echo "Lab 3 benchmark: OpenSSL 4.0.x, RSA=$runs_rsa, <=1MiB=$runs_small, 100MiB=$runs_large"
echo "threads=$threads, affinity=$PIN_CPU, warmup=${warmup}s -> $csv"
"${launcher[@]}" "$bin" bench --runs-rsa "$runs_rsa" --runs-small "$runs_small" \
    --runs-large "$runs_large" --threads "$threads" --warmup "$warmup" \
    --out "$csv" --verbose

python3 "$root/scripts/summarize_bench.py" --input "$csv" --out "$out_dir/summary_${compiler}.csv"
python3 "$root/scripts/plot_bench.py" --input "$csv" --out-dir "$out_dir/graph" --platform "ubuntu-$compiler"
echo "benchmarks done: $out_dir"
