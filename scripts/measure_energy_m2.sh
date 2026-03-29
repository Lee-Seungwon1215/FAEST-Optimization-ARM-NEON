#!/bin/bash
# Measure energy per operation on Apple M2 using powermetrics
#
# Usage: sudo ./measure_energy_m2.sh [--samples N]
#
# Output: benchmark_energy_M2/ directory with per-build per-param power logs
#         and energy_results_M2.csv summarizing results
#
# Requires: sudo (for powermetrics), pre-built bench_c2 binaries
#
# Build dirs expected:
#   faest-ref/build         -> ref (no OpenSSL)
#   faest-ref/build-opt     -> opt-ref (OpenSSL enabled)
#   faest-neon/build        -> neon
#   faest-neon/build-pthread -> neon-pthread (parallel-vole=enabled, 8 threads)

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: This script requires sudo to run powermetrics." >&2
    echo "Usage: sudo $0 [--samples N] [--setup-builds]" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$ROOT_DIR/benchmark_energy_M2"
POWERMETRICS_INTERVAL_MS=200   # sample every 200ms
WARMUP_SECS=2                  # idle warmup before starting benchmark
SAMPLES=10                     # Catch2 --benchmark-samples (reduced for energy; timing uses existing 100-sample data)
SETUP_BUILDS=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --samples) SAMPLES="$2"; shift 2 ;;
        --setup-builds) SETUP_BUILDS=1; shift ;;
        *) echo "Unknown argument: $1"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------
# Optional: build all 4 configurations into separate directories
# ---------------------------------------------------------------
if [ "$SETUP_BUILDS" -eq 1 ]; then
    echo "--- Setting up build directories ---"

    # ref: faest-ref/build (already exists; rebuild to ensure clean state)
    echo "[1/4] ref (faest-ref/build) ..."
    cd "$ROOT_DIR/faest-ref"
    meson setup build \
        -Dopenssl=disabled -Dbenchmarks=enabled -Dcatch2=enabled \
        --buildtype=release --wipe > /dev/null 2>&1
    ninja -C build > /dev/null 2>&1
    echo "  Done."

    # opt-ref: faest-ref/build-opt
    echo "[2/4] opt-ref (faest-ref/build-opt) ..."
    cd "$ROOT_DIR/faest-ref"
    PKG_CONFIG=pkgconf \
    PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig" \
    meson setup build-opt \
        -Dopenssl=enabled -Dbenchmarks=enabled -Dcatch2=enabled \
        --buildtype=release --wipe > /dev/null 2>&1
    ninja -C build-opt > /dev/null 2>&1
    echo "  Done."

    # neon: faest-neon/build (already exists; rebuild)
    echo "[3/4] neon (faest-neon/build) ..."
    cd "$ROOT_DIR/faest-neon"
    meson setup build \
        -Dopenssl=disabled -Dparallel-vole=disabled \
        -Dbenchmarks=enabled -Dcatch2=enabled \
        --buildtype=release --wipe > /dev/null 2>&1
    ninja -C build > /dev/null 2>&1
    echo "  Done."

    # neon-pthread: faest-neon/build-pthread
    echo "[4/4] neon-pthread (faest-neon/build-pthread) ..."
    cd "$ROOT_DIR/faest-neon"
    meson setup build-pthread \
        -Dopenssl=disabled -Dparallel-vole=enabled -Dnum-threads=4 \
        -Dbenchmarks=enabled -Dcatch2=enabled \
        --buildtype=release --wipe > /dev/null 2>&1
    ninja -C build-pthread > /dev/null 2>&1
    echo "  Done."

    cd "$ROOT_DIR"
    echo "--- Build setup complete ---"
    echo ""
fi

mkdir -p "$OUTPUT_DIR"

PARAMS=(128s 128f 192s 192f 256s 256f em_128s em_128f em_192s em_192f em_256s em_256f)

BUILDS=(ref opt-ref neon neon-pthread)

# Map build name -> bench_c2 binary directory (bash 3.2 compatible)
get_build_dir() {
    case "$1" in
        ref)          echo "$ROOT_DIR/faest-ref/build" ;;
        opt-ref)      echo "$ROOT_DIR/faest-ref/build-opt" ;;
        neon)         echo "$ROOT_DIR/faest-neon/build" ;;
        neon-pthread) echo "$ROOT_DIR/faest-neon/build-pthread" ;;
    esac
}

CSV="$OUTPUT_DIR/energy_results_M2.csv"
echo "build,param,operation,avg_power_mW,time_us,energy_uJ" > "$CSV"

# ---------------------------------------------------------------
# Helper: parse Catch2 bench_c2 output, return mean in microseconds
# ---------------------------------------------------------------
parse_mean_us() {
    local output="$1"
    local op="$2"
    # Mean is on the line immediately after the "op  100  ..." header line
    # Uses BSD awk compatible syntax (no 3-arg match)
    echo "$output" | awk -v op="$op" '
        $0 ~ "^[[:space:]]*" op "[[:space:]]" { found=1; next }
        found && NF >= 2 {
            val = $1+0; unit = $2
            if (val == 0) { found=0; next }
            if (unit == "ns") val = val / 1000
            else if (unit == "ms") val = val * 1000
            else if (unit == "s")  val = val * 1000000
            printf "%.4f", val
            exit
        }
    '
}

# ---------------------------------------------------------------
# Helper: compute average CPU Power (mW) from a powermetrics log
# ---------------------------------------------------------------
avg_cpu_power_mw() {
    local logfile="$1"
    # powermetrics text output has lines like:  "CPU Power: 1234 mW"
    awk '/^CPU Power:/ {
        gsub(/[^0-9.]/, "", $3)
        sum += $3; count++
    }
    END { if (count > 0) printf "%.1f", sum/count; else print "N/A" }' "$logfile"
}

echo "============================================================"
echo "  FAEST M2 Energy Measurement"
echo "  powermetrics interval: ${POWERMETRICS_INTERVAL_MS}ms"
echo "  Catch2 samples: ${SAMPLES}"
echo "  Output: $OUTPUT_DIR"
echo "============================================================"

for BUILD in "${BUILDS[@]}"; do
    BUILD_BASE="$(get_build_dir "$BUILD")"
    echo ""
    echo ">>> Build: $BUILD  (dir: $BUILD_BASE)"

    for PARAM in "${PARAMS[@]}"; do
        # Bench binary
        EXE="$BUILD_BASE/faest_${PARAM}/faest_${PARAM}_bench_c2"
        if [ ! -f "$EXE" ]; then
            echo "  [SKIP] $PARAM - binary not found: $EXE"
            continue
        fi

        echo -n "  $PARAM ... "

        POWER_LOG="$OUTPUT_DIR/${BUILD}_${PARAM}_power.log"
        BENCH_LOG="$OUTPUT_DIR/${BUILD}_${PARAM}_bench.log"

        # --- warmup: let CPU settle ---
        sleep "$WARMUP_SECS"

        # --- start powermetrics in background ---
        # Note: omit -n so powermetrics runs until killed
        powermetrics \
            --samplers cpu_power \
            -i "$POWERMETRICS_INTERVAL_MS" \
            > "$POWER_LOG" 2>/dev/null &
        PMID=$!

        # Wait for first sample to appear (powermetrics needs ~1 interval to initialize)
        sleep 1

        # --- run benchmark ---
        "$EXE" '[bench]' \
            --benchmark-samples "$SAMPLES" \
            > "$BENCH_LOG" 2>&1
        BENCH_EXIT=$?

        # Wait one extra interval so powermetrics can flush the last sample
        sleep 1

        # --- stop powermetrics ---
        kill "$PMID" 2>/dev/null || true
        wait "$PMID" 2>/dev/null || true

        if [ "$BENCH_EXIT" -ne 0 ]; then
            echo "FAILED (bench exit $BENCH_EXIT)"
            continue
        fi

        BENCH_OUTPUT=$(cat "$BENCH_LOG")

        # Parse power average
        AVG_POWER=$(avg_cpu_power_mw "$POWER_LOG")

        # Parse timing for each operation
        for OP in keygen sign verify; do
            TIME_US=$(parse_mean_us "$BENCH_OUTPUT" "$OP")
            if [ -z "$TIME_US" ]; then
                TIME_US="N/A"
                ENERGY_UJ="N/A"
            elif [ "$AVG_POWER" = "N/A" ]; then
                ENERGY_UJ="N/A"
            else
                # Energy (µJ) = Power (mW) * time (µs) / 1000
                #   mW * µs = mW * µs * (1e-6 s/µs) = 1e-6 * mW·s = 1e-6 * 1e-3 J = nJ
                #   → nJ / 1000 = µJ
                ENERGY_UJ=$(awk -v p="$AVG_POWER" -v t="$TIME_US" \
                    'BEGIN { printf "%.4f", p * t / 1000 }')
            fi
            echo "$BUILD,$PARAM,$OP,$AVG_POWER,$TIME_US,$ENERGY_UJ" >> "$CSV"
        done

        echo "done  (CPU avg ${AVG_POWER} mW)"
    done
done

echo ""
echo "============================================================"
echo "  Results written to: $CSV"
echo "  Raw power logs:      $OUTPUT_DIR/"
echo "============================================================"
