#!/usr/bin/env bash
# Runs AES-128 CTR benchmarks and records a system-tracking log next to the CSVs.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${1:-gcc}"
shift || true
bin="$root/build/ubuntu-$compiler/aestool"

if [[ ! -x "$bin" ]]; then
    echo "Executable not found at $bin. Building first..."
    "$root/scripts/build_linux.sh" "$compiler"
fi

out_dir="${OUT_DIR:-$root/output/ubuntu/benchmarks}"
mkdir -p "$out_dir"
stamp="$(date -u +%Y%m%dT%H%M%SZ)"

# Optional single size: run_benchmarks.sh <compiler> <size_bytes> [runs]
if [[ $# -ge 1 ]]; then
    sizes=("$1")
    runs=("${2:-${RUNS:-100}}")
    log="$out_dir/bench_ubuntu_${compiler}_ctr_${sizes[0]}_${stamp}.log"
else
    # Per-size runs; RUNS overrides all three, RUNS_1MB/RUNS_100MB/RUNS_1GB override one.
    run_1mb="${RUNS_1MB:-${RUNS:-1000}}"
    run_100mb="${RUNS_100MB:-${RUNS:-500}}"
    run_1gb="${RUNS_1GB:-${RUNS:-50}}"
    sizes=(1048576 104857600)
    runs=("$run_1mb" "$run_100mb")
    if [[ "${BENCH_1GB:-1}" == "1" ]]; then
        sizes+=(1073741824)
        runs+=("$run_1gb")
    fi
    log="$out_dir/bench_ubuntu_${compiler}_${stamp}.log"
fi

# Mirror everything to the terminal and to the log.
exec > >(tee -a "$log") 2>&1

case "$compiler" in
    gcc)   cxx="${CXX:-g++}" ;;
    clang) cxx="${CXX:-clang++}" ;;
    *)     cxx="" ;;
esac
cxx_version="$("$cxx" --version 2>/dev/null | head -1 || echo n/a)"

echo "===== BENCHMARK ====="
echo "timestamp_utc=$stamp"
echo "host=$(hostname)"
echo "kernel=$(uname -srm)"
echo "cpu_model=$(awk -F': ' '/model name/{print $2; exit}' /proc/cpuinfo)"
echo "cpu_cores=$(nproc)"
echo "cpu_governor=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo n/a)"
echo "cpu_mhz=$(awk '/cpu MHz/{printf "%.0f", $4; exit}' /proc/cpuinfo)"
echo "mem_total_kb=$(awk '/MemTotal/{print $2}' /proc/meminfo)"
echo "mem_available_kb=$(awk '/MemAvailable/{print $2}' /proc/meminfo)"
echo "swap_total_kb=$(awk '/SwapTotal/{print $2}' /proc/meminfo)"
echo "compiler=$compiler"
echo "compiler_version=$cxx_version"
echo "git_commit=$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo n/a)"
echo "binary=$bin"
echo "log=$log"
echo "sample_interval_s=${SAMPLE_INTERVAL:-2}"
echo "--- CLI ---"
"$bin" --help || true
echo "--- MATRIX ---"
for i in "${!sizes[@]}"; do
    echo "size=${sizes[$i]} runs=${runs[$i]}"
done
echo "===== RUN ====="

prev_total=0
prev_idle=0

cpu_busy_pct() {
    local _ u n s i w irq sirq st rest total idle dt di
    read -r _ u n s i w irq sirq st rest < /proc/stat
    total=$((u + n + s + i + w + irq + sirq + st))
    idle=$((i + w))
    if (( prev_total > 0 )); then
        dt=$((total - prev_total))
        di=$((idle - prev_idle))
        if (( dt > 0 )); then
            BUSY="$(awk -v dt="$dt" -v di="$di" 'BEGIN { printf "%.1f", 100 * (dt - di) / dt }')"
        else
            BUSY="0.0"
        fi
    else
        BUSY="0.0"
    fi
    prev_total=$total
    prev_idle=$idle
}

sys_sample() {
    local size="$1" pid="$2"
    local ts load1 load5 load15 mem_total mem_avail swap_total swap_free busy proc=""
    ts="$(date -u +%FT%TZ)"
    read -r load1 load5 load15 _ < /proc/loadavg
    mem_total=$(awk '/MemTotal/{print $2}' /proc/meminfo)
    mem_avail=$(awk '/MemAvailable/{print $2}' /proc/meminfo)
    swap_total=$(awk '/SwapTotal/{print $2}' /proc/meminfo)
    swap_free=$(awk '/SwapFree/{print $2}' /proc/meminfo)
    cpu_busy_pct
    busy="$BUSY"
    if [[ -n "$pid" ]] && [[ -d "/proc/$pid" ]]; then        proc="$(ps -o pcpu=,rss= -p "$pid" 2>/dev/null | awk '{ printf "proc_cpu_pct=%s proc_rss_mb=%.0f", $1, $2 / 1024 }')"
    fi
    printf 'SYS %s size=%s cpu_busy_pct=%s load=%s/%s/%s mem_used_mb=%d mem_avail_mb=%d swap_used_mb=%d %s\n' \
        "$ts" "$size" "$busy" "$load1" "$load5" "$load15" \
        $(((mem_total - mem_avail) / 1024)) $((mem_avail / 1024)) \
        $(((swap_total - swap_free) / 1024)) "$proc"
}

run_one() {
    local size="$1" n="$2"
    local csv="$out_dir/bench_ubuntu_${compiler}_ctr_${size}.csv"
    echo "===== BENCHMARK size=${size} runs=${n} ====="
    echo "+ $bin bench --size $size --runs $n --out $csv"
    "$bin" bench --size "$size" --runs "$n" --out "$csv" &
    local pid=$!
    local status=0
    while kill -0 "$pid" 2>/dev/null && [[ "$(ps -o stat= -p "$pid" 2>/dev/null | cut -c1)" != "Z" ]]; do
        sys_sample "$size" "$pid"
        sleep "${SAMPLE_INTERVAL:-2}"
    done
    wait "$pid" || status=$?
    if (( status != 0 )); then
        echo "FAIL size=$size exit=$status"
        return "$status"
    fi
    sys_sample "$size" ""
    echo "csv=$csv rows=$(($(wc -l < "$csv") - 1))"
    echo "===== END size=${size} ====="
}

SECONDS=0
for i in "${!sizes[@]}"; do
    run_one "${sizes[$i]}" "${runs[$i]}"
done

echo "===== END BENCHMARK ====="
echo "elapsed_s=$SECONDS"

if command -v python3 >/dev/null 2>&1; then
    echo "Summarizing results..."
    python3 "$root/scripts/summarize_bench.py" --dir "$out_dir"
fi
