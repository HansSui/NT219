#!/usr/bin/env bash
# Runs the Lab 2 CTR benchmark matrix like scripts/run_benchmarks.bat:
# console-only progress (--verbose), CSVs per size, no log files.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${1:-gcc}"
bin="$root/build/ubuntu-$compiler/aestool"
out_dir="${OUT_DIR:-$root/output/ubuntu/benchmarks}"

if [[ ! -x "$bin" ]]; then
    echo "Executable not found at $bin. Building first..."
    "$root/scripts/build_linux.sh" "$compiler"
fi

run_1mb="${RUNS_1MB:-${RUNS:-1000}}"
run_100mb="${RUNS_100MB:-${RUNS:-100}}"
run_1gb="${RUNS_1GB:-${RUNS:-30}}"
warmup="${WARMUP:-1.0}"
threads="${THREADS:-4}"

if [[ -z "${PIN_CPU:-}" ]]; then
    if (( threads > 1 )); then PIN_CPU="0-3"; else PIN_CPU="2"; fi
fi
if (( threads > 1 )) && [[ "$PIN_CPU" != *,* && "$PIN_CPU" != *-* ]]; then
    echo "WARNING: PIN_CPU=$PIN_CPU pins all $threads threads to one CPU; use PIN_CPU=0-3." >&2
fi

launcher=()
if [[ "$PIN_CPU" != "none" ]] && command -v taskset >/dev/null 2>&1; then
    launcher=(taskset -c "$PIN_CPU")
fi

old_gov=""
changed_gov=0
if [[ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
    old_gov="$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
    if [[ "$old_gov" != "performance" ]] && command -v cpupower >/dev/null 2>&1; then
        if sudo -n cpupower frequency-set -g performance >/dev/null 2>&1; then
            changed_gov=1
        else
            echo "WARNING: could not set performance governor; run: sudo cpupower frequency-set -g performance" >&2
        fi
    fi
fi
restore_gov() {
    if (( changed_gov )); then
        sudo -n cpupower frequency-set -g "$old_gov" >/dev/null 2>&1 || true
    fi
}
trap restore_gov EXIT

echo "Running Lab 2 AES-128 CTR benchmarks using $compiler..."
matrix="Matrix: 1 MiB/$run_1mb, 100 MiB/$run_100mb"
if [[ "${BENCH_1GB:-1}" == "1" ]]; then
    matrix+=", 1 GiB/$run_1gb"
fi
echo "$matrix, warm-up ${warmup}s"
echo "Affinity: $PIN_CPU, threads: $threads"
echo "Results: $out_dir"
mkdir -p "$out_dir"

run_one() {
    local size="$1" runs="$2"
    local csv="$out_dir/bench_ubuntu_${compiler}_ctr_${size}.csv"
    echo "Benchmarking size $size bytes, $runs runs -> $csv"
    "${launcher[@]}" "$bin" bench --mode ctr --size "$size" --runs "$runs" \
        --warmup "$warmup" --threads "$threads" --verbose --out "$csv"
}

run_one 1048576 "$run_1mb"
run_one 104857600 "$run_100mb"
if [[ "${BENCH_1GB:-1}" == "1" ]]; then
    run_one 1073741824 "$run_1gb"
fi

echo "Ubuntu benchmarks completed. Results stored in $out_dir"

if command -v python3 >/dev/null 2>&1; then
    python3 "$root/scripts/summarize_bench.py" --dir "$out_dir"
fi
