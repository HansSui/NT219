#!/usr/bin/env bash
# Run every mode at every required payload size. Defaults: 1000 runs per CSV.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

compiler="${1:-gcc}"
if [[ "$compiler" == "gcc" || "$compiler" == "clang" ]]; then
    tool="$root/build_ubuntu_$compiler/aestool"
else
    tool="$compiler"
    compiler="${COMPILER_NAME:-custom}"
fi
if [[ ! -x "$tool" ]]; then
    echo "missing executable: $tool (build it first)" >&2
    exit 1
fi

runs="${RUNS:-1000}"
warmup="${WARMUP:-1}"
platform="${PLATFORM:-ubuntu}"
out="${OUT:-output/$platform/benchmarks}"
key="${KEY:-output/$platform/key_bin/key.bin}"
sizes="${SIZES:-1024 4096 16384 262144 1048576 8388608}"
modes="${MODES:-ecb cbc ofb cfb ctr xts ccm gcm}"

mkdir -p "$out" "$(dirname "$key")"
if [[ ! -f "$key" ]]; then
    head -c 32 /dev/urandom > "$key"
    echo "generated key: $key"
fi

log="$out/bench.log"
echo "tool:  $tool"
echo "runs:  $runs per mode/size"
echo "modes: $modes"
echo "sizes: $sizes"
echo "out:   $out"

size_label() {
    case "$1" in
        1024) echo "1KB" ;;
        4096) echo "4KB" ;;
        16384) echo "16KB" ;;
        262144) echo "256KB" ;;
        1048576) echo "1MB" ;;
        8388608) echo "8MB" ;;
        *) echo "${1}B" ;;
    esac
}

for mode in $modes; do
    for size in $sizes; do
        label="$(size_label "$size")"
        csv="$out/bench_${platform}_${compiler}_${mode}_${label}.csv"
        args=(bench --mode "$mode" --key "$key" --size "$size" --runs "$runs"
              --warmup "$warmup" --out "$csv")
        [[ "$mode" == "ecb" ]] && args+=(--allow-ecb)
        echo "benchmarking $mode $label ($runs runs)"
        if ! "$tool" "${args[@]}" >/dev/null 2>>"$log"; then
            echo "failed: $csv (see $log)" >&2
            exit 1
        fi
        echo "  wrote $(basename "$csv")"
    done
done

if command -v python3 >/dev/null 2>&1; then
    python3 scripts/summarize_bench.py --dir "$out"
fi

if [[ "${PLOT:-0}" == "1" ]]; then
    python3 scripts/plot_compare.py --dir "$out" --out-dir "$out/graph"
fi

echo "done: $out"
