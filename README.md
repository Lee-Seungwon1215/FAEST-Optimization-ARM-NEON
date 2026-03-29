# FAEST ARM Optimized

ARM-optimized implementation of the FAEST post-quantum signature scheme, targeting Apple Silicon (M1/M2/M3) and Raspberry Pi 4 (Cortex-A72).

> **Paper:** [Accelerating FAEST Signatures on ARM: NEON SIMD AES and Parallel VOLE Optimization]
> **Authors:** [Seung-Won Lee, Ha-Gyeong Kim, Min-Ho Song, Si-Woo Eum and Hwa-Jeong Seo*]
> **Venue:** [MDPI]
> **Preprint:** [https://eprint.iacr.org/2026/499]

---

## Overview

FAEST is a post-quantum digital signature scheme based on AES and VOLE-in-the-Head (VOLEitH), submitted to the NIST PQC standardization process. This repository provides ARM-optimized implementations that achieve significant speedups over the pure-C reference by exploiting:

- **ARM NEON SIMD**: Parallel AES using 4-block and 8-block interleaving via AES intrinsics
- **Multi-threaded VOLE**: Optional pthread-based parallelism for the VOLE phase

---

## Repository Structure

```
faest-arm-optimized/
├── faest-ref/              # Reference implementation (pure C, optional OpenSSL)
├── faest-neon/             # NEON-optimized (Apple Silicon / macOS assembler syntax)
├── faest-neon-RPi4/        # NEON-optimized (Raspberry Pi 4 / GNU assembler syntax)
├── scripts/
│   ├── benchmark.sh            # Quick benchmark runner (ref vs neon, prints to terminal)
│   ├── generate_csvs.py        # Parse raw Catch2 output → results/*.csv
│   ├── generate_figures.py     # Generate performance comparison figures (PDF)
│   ├── generate_energy_figures.py  # Generate energy figures (PDF)
│   └── measure_energy_m2.sh    # Apple M2 energy measurement via powermetrics (requires sudo)
└── results/
    ├── m2_performance.csv      # Apple M2 keygen/sign/verify (mean, stddev, 95% CI)
    ├── rpi4_performance.csv    # Raspberry Pi 4 (same schema)
    └── energy_m2.csv           # Apple M2 energy per operation (mW, µs, µJ)
```

---

## Implementations

| Build | Directory | AES Backend | Parallelism |
|-------|-----------|-------------|-------------|
| `ref` | `faest-ref` | Pure C (no hardware acceleration) | None |
| `opt-ref` | `faest-ref` | OpenSSL (uses ARM crypto extensions) | None |
| `neon` | `faest-neon` / `faest-neon-RPi4` | Custom NEON intrinsics (4/8-block parallel AES) | None |
| `neon-pthread` | `faest-neon` / `faest-neon-RPi4` | Custom NEON intrinsics | pthread (4 threads) |

> **Note on `faest-neon` vs `faest-neon-RPi4`**: Both are functionally identical. The difference is assembler syntax — `faest-neon` uses macOS/Apple Clang syntax (e.g., `.align`), while `faest-neon-RPi4` uses GNU assembler syntax (e.g., `.balign`) for compatibility with Linux on Cortex-A72.

---

## Test Environment

Results in this repository were obtained on:

| Platform | Details |
|----------|---------|
| Apple M2 | MacBook Air, macOS 15, Apple Clang 17.0.0, OpenSSL 3.6.0 (Homebrew) |
| Raspberry Pi 4 | Cortex-A72 @ 1.5 GHz, Ubuntu 22.04, GCC 11.4.0 |

---

## Build Instructions

### Prerequisites

- Meson >= 0.57
- Ninja
- GCC or Clang with C11 support
- OpenSSL >= 3.x (optional, required for `opt-ref` build)
- Python >= 3.8 with `matplotlib`, `numpy` (optional, for figure generation)

On macOS (Homebrew):
```bash
brew install meson ninja openssl@3 pkgconf
pip3 install matplotlib numpy
```

On Ubuntu/Raspberry Pi OS:
```bash
sudo apt install meson ninja-build gcc libssl-dev python3-matplotlib python3-numpy
```

### Building

All commands are run from the repository root.

**ref** (pure C, no hardware acceleration):
```bash
cd faest-ref
meson setup build -Dopenssl=disabled --buildtype=release
ninja -C build
```

**opt-ref** (OpenSSL-backed AES, hardware-accelerated on ARM):

macOS:
```bash
cd faest-ref
PKG_CONFIG=pkgconf PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig" \
  meson setup build-opt -Dopenssl=enabled --buildtype=release
ninja -C build-opt
```

Linux:
```bash
cd faest-ref
meson setup build-opt -Dopenssl=enabled --buildtype=release
ninja -C build-opt
```

**neon** (Apple Silicon):
```bash
cd faest-neon
meson setup build -Dopenssl=disabled -Dparallel-vole=disabled --buildtype=release
ninja -C build
```

**neon-pthread** (Apple Silicon, 4 threads):
```bash
cd faest-neon
meson setup build-pthread \
  -Dopenssl=disabled -Dparallel-vole=enabled -Dnum-threads=4 --buildtype=release
ninja -C build-pthread
```

**neon** (Raspberry Pi 4):
```bash
cd faest-neon-RPi4
meson setup build -Dopenssl=disabled -Dparallel-vole=disabled --buildtype=release
ninja -C build
```

**neon-pthread** (Raspberry Pi 4, 4 threads):
```bash
cd faest-neon-RPi4
meson setup build-pthread \
  -Dopenssl=disabled -Dparallel-vole=enabled -Dnum-threads=4 --buildtype=release
ninja -C build-pthread
```

### Running Tests

```bash
# Example: test the neon build
cd faest-neon
ninja -C build test
```

---

## Reproducing Benchmark Results

### Step 1: Build with Catch2 benchmarking enabled

Run the following from each implementation directory. Shown here for Apple Silicon; substitute `faest-neon-RPi4` for Raspberry Pi 4.

```bash
# ref
cd faest-ref
meson setup build \
  -Dopenssl=disabled -Dbenchmarks=enabled -Dcatch2=enabled --buildtype=release
ninja -C build

# opt-ref (macOS)
PKG_CONFIG=pkgconf PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig" \
  meson setup build-opt \
  -Dopenssl=enabled -Dbenchmarks=enabled -Dcatch2=enabled --buildtype=release
ninja -C build-opt
cd ..

# neon
cd faest-neon
meson setup build \
  -Dopenssl=disabled -Dparallel-vole=disabled \
  -Dbenchmarks=enabled -Dcatch2=enabled --buildtype=release
ninja -C build

# neon-pthread (4 threads)
meson setup build-pthread \
  -Dopenssl=disabled -Dparallel-vole=enabled -Dnum-threads=4 \
  -Dbenchmarks=enabled -Dcatch2=enabled --buildtype=release
ninja -C build-pthread
cd ..
```

### Step 2: Run all benchmarks and save raw output

Run the following from the repository root. This collects all 4 builds × 12 parameter sets into raw text files.

```bash
# Create output directories first
mkdir -p benchmark_results_M2
mkdir -p benchmark_results_RPi4   # on Raspberry Pi 4 only

PARAMS="128s 128f 192s 192f 256s 256f em_128s em_128f em_192s em_192f em_256s em_256f"

# ref
for P in $PARAMS; do
  faest-ref/build/faest_${P}/faest_${P}_bench_c2 "[bench]" --benchmark-samples 100 \
    > benchmark_results_M2/ref_${P}.txt
done

# opt-ref (macOS)
for P in $PARAMS; do
  faest-ref/build-opt/faest_${P}/faest_${P}_bench_c2 "[bench]" --benchmark-samples 100 \
    > benchmark_results_M2/opt-ref_${P}.txt
done

# neon
for P in $PARAMS; do
  faest-neon/build/faest_${P}/faest_${P}_bench_c2 "[bench]" --benchmark-samples 100 \
    > benchmark_results_M2/neon_${P}.txt
done

# neon-pthread
for P in $PARAMS; do
  faest-neon/build-pthread/faest_${P}/faest_${P}_bench_c2 "[bench]" --benchmark-samples 100 \
    > benchmark_results_M2/neon-pthread_${P}.txt
done
```

For Raspberry Pi 4, substitute `faest-neon-RPi4` for `faest-neon` and save to `benchmark_results_RPi4/` instead.

> **Quick comparison only** (ref vs neon, prints to terminal, no files saved):
> ```bash
> ./scripts/benchmark.sh
> ```

### Step 3: Generate CSVs from raw result files

```bash
# Reads benchmark_results_M2/ and benchmark_results_RPi4/ → results/*.csv
python3 scripts/generate_csvs.py
```

Expected input file naming: `benchmark_results_M2/{build}_{param}.txt`
where `build` ∈ `{ref, opt-ref, neon, neon-pthread}` and `param` ∈ `{128s, 128f, ..., em_256f}`

### Step 4: Generate figures (optional)

```bash
# Performance comparison bar charts → PDF
python3 scripts/generate_figures.py

# Energy figures → PDF (requires results/energy_m2.csv from Step 5)
python3 scripts/generate_energy_figures.py
```

### Step 5: Energy Measurement (Apple M2 only, requires sudo)

```bash
sudo ./scripts/measure_energy_m2.sh
# Output: benchmark_energy_M2/energy_results_M2.csv
# Re-run Step 3 to update results/energy_m2.csv
```

---

## Performance Results

All benchmarks use Catch2 microbenchmarking with 100 samples. Times shown are mean values.
Full results with confidence intervals are in [`results/m2_performance.csv`](results/m2_performance.csv) and [`results/rpi4_performance.csv`](results/rpi4_performance.csv).

---

### Apple M2 (4P+4E cores)

#### Key Generation

| Parameter | ref | opt-ref | neon | neon-pthread (4T) |
|-----------|-----|---------|------|-------------------|
| FAEST-128s | 15.44 µs | 0.75 µs | 0.63 µs | 0.65 µs |
| FAEST-128f | 15.66 µs | 0.73 µs | 0.63 µs | 0.63 µs |
| FAEST-192s | 31.52 µs | 0.75 µs | 0.67 µs | 0.69 µs |
| FAEST-192f | 30.38 µs | 0.77 µs | 0.66 µs | 0.67 µs |
| FAEST-256s | 37.15 µs | 0.72 µs | 0.71 µs | 0.73 µs |
| FAEST-256f | 36.79 µs | 0.73 µs | 0.70 µs | 0.73 µs |
| FAEST-EM-128s | 15.55 µs | 0.76 µs | 0.63 µs | 0.64 µs |
| FAEST-EM-128f | 15.63 µs | 0.72 µs | 0.62 µs | 0.64 µs |
| FAEST-EM-192s | 25.24 µs | 24.58 µs | 25.49 µs | 25.93 µs |
| FAEST-EM-192f | 25.19 µs | 24.87 µs | 24.61 µs | 25.14 µs |
| FAEST-EM-256s | 40.46 µs | 40.78 µs | 41.33 µs | 42.71 µs |
| FAEST-EM-256f | 41.07 µs | 40.51 µs | 41.10 µs | 42.00 µs |

> **Note:** FAEST-EM-192/256 keygen involves a fixed AES key schedule, so all builds take similar time regardless of implementation.

#### Signing

| Parameter | ref | opt-ref | neon | neon-pthread (4T) | Speedup (ref → neon-pthread) |
|-----------|-----|---------|------|-------------------|-------------------------------|
| FAEST-128s | 5.430 s | 43.87 ms | 43.86 ms | 34.59 ms | **157×** |
| FAEST-128f | 747.5 ms | 16.05 ms | 16.00 ms | 14.88 ms | **50×** |
| FAEST-192s | 19.78 s | 120.8 ms | 133.1 ms | 107.2 ms | **185×** |
| FAEST-192f | 2.485 s | 49.90 ms | 51.62 ms | 48.62 ms | **51×** |
| FAEST-256s | 43.28 s | 186.6 ms | 235.8 ms | 172.8 ms | **250×** |
| FAEST-256f | 5.103 s | 70.34 ms | 76.18 ms | 69.22 ms | **74×** |
| FAEST-EM-128s | 4.104 s | 32.73 ms | 30.16 ms | 23.33 ms | **176×** |
| FAEST-EM-128f | 568.4 ms | 13.36 ms | 13.00 ms | 12.25 ms | **46×** |
| FAEST-EM-192s | 17.41 s | 83.42 ms | 92.31 ms | 64.38 ms | **270×** |
| FAEST-EM-192f | 1.860 s | 34.94 ms | 35.98 ms | 33.61 ms | **55×** |
| FAEST-EM-256s | 36.48 s | 128.0 ms | 163.0 ms | 110.5 ms | **330×** |
| FAEST-EM-256f | 4.297 s | 61.16 ms | 65.23 ms | 59.68 ms | **72×** |

#### Verification

| Parameter | ref | opt-ref | neon | neon-pthread (4T) | Speedup (ref → neon-pthread) |
|-----------|-----|---------|------|-------------------|-------------------------------|
| FAEST-128s | 5.399 s | 39.20 ms | 38.75 ms | 29.52 ms | **183×** |
| FAEST-128f | 737.0 ms | 12.41 ms | 12.43 ms | 11.15 ms | **66×** |
| FAEST-192s | 20.06 s | 101.2 ms | 114.5 ms | 87.55 ms | **229×** |
| FAEST-192f | 2.453 s | 37.00 ms | 38.43 ms | 35.29 ms | **70×** |
| FAEST-256s | 43.25 s | 169.8 ms | 218.7 ms | 156.0 ms | **277×** |
| FAEST-256f | 5.066 s | 53.63 ms | 58.77 ms | 52.17 ms | **97×** |
| FAEST-EM-128s | 4.095 s | 28.39 ms | 26.11 ms | 18.93 ms | **216×** |
| FAEST-EM-128f | 559.0 ms | 9.864 ms | 9.509 ms | 8.763 ms | **64×** |
| FAEST-EM-192s | 17.40 s | 72.48 ms | 81.68 ms | 52.98 ms | **328×** |
| FAEST-EM-192f | 1.819 s | 25.14 ms | 26.31 ms | 23.73 ms | **77×** |
| FAEST-EM-256s | 36.41 s | 109.7 ms | 144.2 ms | 90.36 ms | **403×** |
| FAEST-EM-256f | 4.249 s | 43.43 ms | 47.46 ms | 42.72 ms | **99×** |

---

### Raspberry Pi 4 (Cortex-A72, 4 cores)

#### Key Generation

| Parameter | ref | opt-ref | neon | neon-pthread (4T) |
|-----------|-----|---------|------|-------------------|
| FAEST-128s | 61.21 µs | 3.888 µs | 2.745 µs | 2.755 µs |
| FAEST-128f | 58.91 µs | 3.900 µs | 2.761 µs | 2.735 µs |
| FAEST-192s | 122.3 µs | 4.807 µs | 3.241 µs | 3.239 µs |
| FAEST-192f | 122.1 µs | 4.801 µs | 3.236 µs | 3.241 µs |
| FAEST-256s | 146.5 µs | 5.174 µs | 3.815 µs | 3.844 µs |
| FAEST-256f | 146.6 µs | 5.086 µs | 3.817 µs | 3.850 µs |
| FAEST-EM-128s | 58.94 µs | 3.936 µs | 2.744 µs | 2.743 µs |
| FAEST-EM-128f | 58.92 µs | 4.007 µs | 2.757 µs | 2.773 µs |
| FAEST-EM-192s | 99.18 µs | 99.24 µs | 99.35 µs | 99.47 µs |
| FAEST-EM-192f | 99.16 µs | 99.17 µs | 99.22 µs | 99.27 µs |
| FAEST-EM-256s | 165.1 µs | 165.2 µs | 165.1 µs | 165.1 µs |
| FAEST-EM-256f | 165.2 µs | 165.1 µs | 165.1 µs | 165.1 µs |

#### Signing

| Parameter | ref | opt-ref | neon | neon-pthread (4T) | Speedup (ref → neon-pthread) |
|-----------|-----|---------|------|-------------------|-------------------------------|
| FAEST-128s | 21.79 s | 489.2 ms | 400.0 ms | 258.7 ms | **84×** |
| FAEST-128f | 3.001 s | 96.34 ms | 84.66 ms | 65.04 ms | **46×** |
| FAEST-192s | 79.39 s | 1.778 s | 1.567 s | 1.111 s | **71×** |
| FAEST-192f | 10.09 s | 391.2 ms | 361.4 ms | 306.3 ms | **33×** |
| FAEST-256s | 175.7 s | 3.268 s | 3.005 s | 2.048 s | **86×** |
| FAEST-256f | 20.78 s | 657.2 ms | 633.5 ms | 512.8 ms | **41×** |
| FAEST-EM-128s | 16.51 s | 380.5 ms | 304.4 ms | 189.6 ms | **87×** |
| FAEST-EM-128f | 2.278 s | 78.87 ms | 68.50 ms | 53.14 ms | **43×** |
| FAEST-EM-192s | 70.07 s | 1.251 s | 1.064 s | 598.7 ms | **117×** |
| FAEST-EM-192f | 7.429 s | 259.3 ms | 238.1 ms | 191.1 ms | **39×** |
| FAEST-EM-256s | 147.0 s | 2.228 s | 1.959 s | 1.074 s | **137×** |
| FAEST-EM-256f | 17.41 s | 523.2 ms | 496.8 ms | 393.8 ms | **44×** |

#### Verification

| Parameter | ref | opt-ref | neon | neon-pthread (4T) | Speedup (ref → neon-pthread) |
|-----------|-----|---------|------|-------------------|-------------------------------|
| FAEST-128s | 21.75 s | 470.8 ms | 381.3 ms | 239.2 ms | **91×** |
| FAEST-128f | 2.963 s | 84.85 ms | 73.05 ms | 53.71 ms | **55×** |
| FAEST-192s | 79.23 s | 1.662 s | 1.455 s | 1.009 s | **79×** |
| FAEST-192f | 9.952 s | 323.6 ms | 295.5 ms | 243.0 ms | **41×** |
| FAEST-256s | 175.4 s | 3.167 s | 2.910 s | 1.911 s | **92×** |
| FAEST-256f | 20.52 s | 560.0 ms | 532.6 ms | 414.9 ms | **50×** |
| FAEST-EM-128s | 16.47 s | 363.9 ms | 287.4 ms | 173.7 ms | **95×** |
| FAEST-EM-128f | 2.245 s | 67.69 ms | 57.43 ms | 42.59 ms | **53×** |
| FAEST-EM-192s | 69.98 s | 1.194 s | 1.008 s | 544.7 ms | **129×** |
| FAEST-EM-192f | 7.319 s | 210.5 ms | 189.7 ms | 142.5 ms | **51×** |
| FAEST-EM-256s | 146.8 s | 2.127 s | 1.851 s | 966.2 ms | **152×** |
| FAEST-EM-256f | 17.18 s | 421.9 ms | 390.2 ms | 289.6 ms | **59×** |

---

### Key Observations

- **opt-ref ≈ neon** on FAEST (non-EM): Both leverage ARM AES hardware — opt-ref via OpenSSL, neon via direct NEON intrinsics.
- **neon outperforms opt-ref** on FAEST-EM: The EM variant's OWF is more amenable to the custom NEON parallel pipeline.
- **neon-pthread** provides an additional 20–35% improvement over neon by parallelizing VOLE across 4 threads.
- **256-bit variants** show the largest absolute speedups (up to 330× on M2 for FAEST-EM-256s sign, 403× for verify).
- **FAEST-EM keygen** is build-independent at 192/256-bit: the key schedule uses a fixed AES key, so no AES pipeline optimization applies.

---

## Energy Results (Apple M2)

Energy per operation measured on Apple M2 using `powermetrics` (200 ms sampling interval).
Full data in [`results/energy_m2.csv`](results/energy_m2.csv). To regenerate figures: `python3 scripts/generate_energy_figures.py`.

### Energy per Sign Operation (µJ)

| Parameter | ref | opt-ref | neon | neon-pthread (4T) | Energy Reduction (ref → neon-pthread) |
|-----------|-----|---------|------|-------------------|---------------------------------------|
| FAEST-128s | 13,404,018 | 149,950 | 184,888 | 128,189 | **99×** |
| FAEST-128f | 1,894,542 | 51,573 | 58,902 | 46,111 | **41×** |
| FAEST-192s | 50,584,183 | 514,700 | 634,823 | 434,957 | **116×** |
| FAEST-192f | 7,010,992 | 165,649 | 273,732 | 176,635 | **40×** |
| FAEST-256s | 117,484,192 | 773,951 | 1,128,937 | 891,686 | **132×** |
| FAEST-256f | 13,449,360 | 257,619 | 376,137 | 239,894 | **56×** |
| FAEST-EM-128s | 10,479,164 | 108,115 | 165,457 | 97,178 | **108×** |
| FAEST-EM-128f | 1,480,361 | 48,028 | 64,053 | 39,382 | **38×** |
| FAEST-EM-192s | 50,977,858 | 298,517 | 429,298 | 301,541 | **169×** |
| FAEST-EM-192f | 4,577,226 | 139,163 | 110,026 | 105,860 | **43×** |
| FAEST-EM-256s | 103,863,385 | 569,106 | 664,311 | 562,400 | **185×** |
| FAEST-EM-256f | 13,276,363 | 246,520 | 212,798 | 196,481 | **68×** |

> Energy values for RPi4 are estimates based on Cortex-A72 power consumption from literature (single-core: ~1.1 W, quad-core: ~4.0 W). See [`results/energy_m2.csv`](results/energy_m2.csv) for measured M2 values including avg_power_mW.

---

## Supported Parameter Sets

12 FAEST parameter sets across two variants:

| Variant | Security Level | Parameters |
|---------|---------------|------------|
| FAEST | 128 / 192 / 256 bit | 128s, 128f, 192s, 192f, 256s, 256f |
| FAEST-EM | 128 / 192 / 256 bit | EM-128s, EM-128f, EM-192s, EM-192f, EM-256s, EM-256f |

(`s` = short signatures, `f` = fast signing)

---

## Target Platforms

| Platform | Directory | Assembler Syntax |
|----------|-----------|-----------------|
| Apple Silicon (M1/M2/M3) | `faest-neon` | macOS/Apple Clang |
| Raspberry Pi 4 (Cortex-A72) | `faest-neon-RPi4` | GNU (Linux) |
| Other ARM64 Linux | `faest-neon-RPi4` | GNU (Linux) |

