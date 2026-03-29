#!/bin/bash
# FAEST Benchmark: Reference (Software AES) vs NEON Optimized
# Uses Catch2 bench_c2 for accurate microbenchmarking

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
REF_DIR="$ROOT_DIR/faest-ref"
NEON_DIR="$ROOT_DIR/faest-neon"

BUILD_DIR="build-bench"

echo "============================================================"
echo "  FAEST Benchmark: Reference vs NEON Optimized"
echo "  Using Catch2 Microbenchmark (bench_c2)"
echo "============================================================"
echo ""

# Build function with benchmarks enabled
build_faest() {
    local dir=$1
    local name=$2
    local build_dir="$dir/$BUILD_DIR"

    echo "Building $name..."
    cd "$dir"
    rm -rf "$build_dir"
    meson setup "$build_dir" \
        -Dopenssl=disabled \
        -Dbenchmarks=enabled \
        -Dcatch2=enabled \
        --buildtype=release > /dev/null 2>&1
    ninja -C "$build_dir" > /dev/null 2>&1
    echo "  Done."
}

# Parse Catch2 benchmark output to extract mean time in microseconds
parse_bench_output() {
    local output="$1"
    local operation="$2"

    # Catch2 output format:
    # keygen                                         100             2     3.2914 ms
    #                                          16.793 us    16.5172 us    17.4722 us
    #                                         2.06983 us    986.439 ns    3.87263 us
    # The mean value is the first value on the second line after benchmark name

    # Find the line with the operation name and get the next line (mean values)
    local mean_line=$(echo "$output" | grep -A 1 "^${operation} " | tail -1 | xargs)

    if [ -z "$mean_line" ]; then
        echo "N/A"
        return
    fi

    # Extract first value and unit (mean)
    local num=$(echo "$mean_line" | awk '{print $1}')
    local unit=$(echo "$mean_line" | awk '{print $2}')

    # Convert to microseconds
    case "$unit" in
        ns) printf "%.2f" $(echo "scale=6; $num / 1000" | bc) ;;
        us) printf "%.2f" "$num" ;;
        ms) printf "%.2f" $(echo "scale=6; $num * 1000" | bc) ;;
        s)  printf "%.2f" $(echo "scale=6; $num * 1000000" | bc) ;;
        *)  echo "N/A" ;;
    esac
}

# Run benchmark using bench_c2
run_bench() {
    local exe=$1
    local operation=$2

    if [ ! -f "$exe" ]; then
        echo "N/A"
        return
    fi

    # Run Catch2 benchmark
    local output=$("$exe" '[bench]' 2>/dev/null)
    parse_bench_output "$output" "$operation"
}

# Build both versions
echo "--- Building ---"
build_faest "$REF_DIR" "faest-ref (Reference)"
build_faest "$NEON_DIR" "faest-neon (NEON Optimized)"
echo ""

# Run benchmarks
echo "--- Benchmark Results (Catch2 mean values) ---"
echo ""

params=("128s" "128f" "em_128s" "em_128f" "192s" "192f" "em_192s" "em_192f" "256s" "256f" "em_256s" "em_256f")

# Print header
echo "┌─────────────────┬────────────────────────────────────┬────────────────────────────────────┬──────────┐"
echo "│ Parameter       │ Reference (us)                     │ NEON (us)                          │ Speedup  │"
echo "│                 │   keygen /     sign /    verify    │   keygen /     sign /    verify    │ (sign)   │"
echo "├─────────────────┼────────────────────────────────────┼────────────────────────────────────┼──────────┤"

for p in "${params[@]}"; do
    ref_exe="$REF_DIR/$BUILD_DIR/faest_$p/faest_${p}_bench_c2"
    neon_exe="$NEON_DIR/$BUILD_DIR/faest_$p/faest_${p}_bench_c2"

    # Format parameter name for progress
    name=$(echo "FAEST-$p" | sed 's/_/-/g' | tr '[:lower:]' '[:upper:]' | sed 's/EM-/EM-/')

    # Progress indicator
    echo -ne "\r  Running: $name (ref)...          " >&2

    # Get Reference timings
    if [ -f "$ref_exe" ]; then
        ref_output=$("$ref_exe" '[bench]' 2>/dev/null)
        ref_keygen=$(parse_bench_output "$ref_output" "keygen")
        ref_sign=$(parse_bench_output "$ref_output" "sign")
        ref_verify=$(parse_bench_output "$ref_output" "verify")
    else
        ref_keygen="N/A"
        ref_sign="N/A"
        ref_verify="N/A"
    fi

    echo -ne "\r  Running: $name (neon)...         " >&2

    # Get NEON timings
    if [ -f "$neon_exe" ]; then
        neon_output=$("$neon_exe" '[bench]' 2>/dev/null)
        neon_keygen=$(parse_bench_output "$neon_output" "keygen")
        neon_sign=$(parse_bench_output "$neon_output" "sign")
        neon_verify=$(parse_bench_output "$neon_output" "verify")
    else
        neon_keygen="N/A"
        neon_sign="N/A"
        neon_verify="N/A"
    fi

    echo -ne "\r                                    \r" >&2

    # Calculate speedup based on sign operation
    if [ "$ref_sign" != "N/A" ] && [ "$neon_sign" != "N/A" ]; then
        speedup=$(python3 -c "print(f'{float($ref_sign) / float($neon_sign):.2f}x')")
    else
        speedup="N/A"
    fi

    printf "│ %-15s │ %8s / %8s / %8s │ %8s / %8s / %8s │ %8s │\n" \
        "$name" "$ref_keygen" "$ref_sign" "$ref_verify" \
        "$neon_keygen" "$neon_sign" "$neon_verify" "$speedup"
done

echo "└─────────────────┴────────────────────────────────────┴────────────────────────────────────┴──────────┘"
echo ""
echo "Note: Reference uses pure software AES (no OpenSSL/hardware acceleration)"
echo "      NEON uses ARM64 NEON parallel AES (4-block/8-block)"
echo "      All times in microseconds (us), lower is better"
echo "============================================================"
